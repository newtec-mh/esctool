#include "sii.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <fstream>
#include "esidefs.h"
#include "esctooldefs.h"
#include "esctoolhelpers.h"

uint32_t EC_SII_EEPROM_SIZE			(1024);

void SII::encodeEEPROMBinary(uint32_t vendor_id, Device* dev, const bool encodepdo, 
	const std::string& file, const std::string& output, const bool verbose)
{
	if(EC_SII_EEPROM_SIZE < dev->eepromsize)
		EC_SII_EEPROM_SIZE = dev->eepromsize;

	uint8_t* sii_eeprom = NULL;
	printf("Encoding '%s' to '%s' EEPROM\n",file.c_str(),output.c_str());
	sii_eeprom = new uint8_t[EC_SII_EEPROM_SIZE];
	memset(sii_eeprom,0,EC_SII_EEPROM_SIZE);

	std::ofstream out;
	out.open(output.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
	if(!out.fail()) {
		// Pointer for writing
		uint8_t* p = &sii_eeprom[0];

		// Write configdata part
		for(uint8_t i = 0; i < EC_SII_CONFIGDATA_SIZEB; ++i,++p)
			*p = dev->configdata[i];

		// Write Vendor ID (Word 0x0008)
		*(p++) = (vendor_id & 0xFF);
		*(p++) = (vendor_id >> 8) & 0xFF;
		*(p++) = (vendor_id >> 16) & 0xFF;
		*(p++) = (vendor_id >> 24) & 0xFF;

		// Write Product Code (Word 0x000A)
		*(p++) = (dev->product_code & 0xFF);
		*(p++) = (dev->product_code >> 8) & 0xFF;
		*(p++) = (dev->product_code >> 16) & 0xFF;
		*(p++) = (dev->product_code >> 24) & 0xFF;

		// Write Revision No (Word 0x000C)
		*(p++) = (dev->revision_no & 0xFF);
		*(p++) = (dev->revision_no >> 8) & 0xFF;
		*(p++) = (dev->revision_no >> 16) & 0xFF;
		*(p++) = (dev->revision_no >> 24) & 0xFF;

		// Handle out/in mailbox offsets
		for(SyncManager* sm : dev->syncmanagers) {
			if(0 == strcmp(sm->type,"MBoxOut")) {
				// Write Mailbox Out (Word 0x0018)
				p = sii_eeprom + EC_SII_EEPROM_MAILBOX_OUT_OFFSET_BYTE;
				*(p++) = sm->startaddress & 0xFF;
				*(p++) = (sm->startaddress >> 8) & 0xFF;
				*(p++) = sm->defaultsize & 0xFF;
				*(p++) = (sm->defaultsize >> 8) & 0xFF;
			} else
			if(0 == strcmp(sm->type,"MBoxIn")) {
				// Write Mailbox In (Word 0x001A)
				p = sii_eeprom + EC_SII_EEPROM_MAILBOX_IN_OFFSET_BYTE;
				*(p++) = sm->startaddress & 0xFF;
				*(p++) = (sm->startaddress >> 8) & 0xFF;
				*(p++) = sm->defaultsize & 0xFF;
				*(p++) = (sm->defaultsize >> 8) & 0xFF;
			}
		}

		// Write Mailbox Protocol (Word 0x001C)
		p = sii_eeprom + EC_SII_EEPROM_MAILBOX_PROTO_OFFSET_BYTE;
		uint16_t mailbox_proto = 0x0;
		if(dev->mailbox) {
			if(dev->mailbox->aoe) mailbox_proto |= 0x0001;
			if(dev->mailbox->eoe) mailbox_proto |= 0x0002;
			if(dev->mailbox->coe) mailbox_proto |= 0x0004;
			if(dev->mailbox->foe) mailbox_proto |= 0x0008;
			if(dev->mailbox->soe) mailbox_proto |= 0x0010;
			if(dev->mailbox->voe) mailbox_proto |= 0x0020;
		}
		*(p++) = mailbox_proto & 0xFF;
		*(p++) = (mailbox_proto >> 8) & 0xFF;

		// EEPROM Size 0x003E
		p = sii_eeprom + EC_SII_EEPROM_VERSION_OFFSET_BYTE - 2;
		uint16_t sz = ((EC_SII_EEPROM_SIZE * 8) / 1024) - 1;
		*(p++) = sz & 0xFF;
		*(p++) = (sz >> 8) & 0xFF;

		// Version
		*(p++) = EC_SII_VERSION & 0xFF;
		*(p++) = (EC_SII_VERSION >> 8) & 0xFF;

		p = sii_eeprom + EC_SII_EEPROM_FIRST_CAT_HDR_OFFSET_BYTE;
		// First category header (seems to be STRINGS usually) (Word 0x040)
		*(p++) = EEPROMCategorySTRINGS & 0xFF;
		*(p++) = (EEPROMCategorySTRINGS >> 8) & 0xFF;

		// Default: two strings, device group name first, then device name
		std::list<const char*> strings;
		strings.push_back(dev->group->type);
		strings.push_back(dev->name);

		// First category word size (Word 0x041)
		// +1 for the number of strings and +1 for the
		// string length of each string (of which there are two) so +3
		uint16_t stringcatlen = 0x1; // For the number of strings byte
		for(auto str : strings) {
			stringcatlen += strlen(str);
			++stringcatlen; // The stringlength byte
		}

		// Pad to complete word
		uint16_t stringcatpad = (stringcatlen % 2);
		stringcatlen += stringcatpad;
		stringcatlen /= 2; // "Convert" to words
		*(p++) = stringcatlen & 0xFF;
		*(p++) = (stringcatlen >> 8) & 0xFF;

		// Create STRINGS category (ETG2000 Table 6)
		*(p++) = strings.size() & 0xFF;
		for(auto str : strings) {
			uint8_t len = strlen(str);
			*(p++) = len;
			for(uint8_t i = 0; i < len; ++i) {
				*(p++) = str[i];
			}
		}
		// Adjust write pointer for padding
		p += stringcatpad;

		// Next category, seems to be GENERAL (ETG2000 Table 7)
		*(p++) = EEPROMCategoryGeneral & 0xFF;
		*(p++) = (EEPROMCategoryGeneral >> 8) & 0xFF;

		*(p++) = 0x10; // General category is 16 words
		*(p++) = 0x0;

		*(p++) = 0x1; // Group name index to STRINGS (1 as per above)
		*(p++) = 0x0; // Image name index to STRINGS (0, not supported yet TODO)
		*(p++) = 0x0; // Device order number index to STRINGS (0, not supported yet TODO)
		*(p++) = 0x2; // Device name index to STRINGS (2 as per above)

		*(p++); // Reserved

		uint8_t coedetails = 0x0;
		if(dev->mailbox) {
			coedetails |= dev->mailbox->coe ? 0x1 : 0x0;
			coedetails |= dev->mailbox->coe_sdoinfo ? (0x1 << 1) : 0x0;
			coedetails |= dev->mailbox->coe_pdoassign ? (0x1 << 2) : 0x0;
			coedetails |= dev->mailbox->coe_pdoconfig ? (0x1 << 3) : 0x0;
			coedetails |= dev->mailbox->coe_pdoupload ? (0x1 << 4) : 0x0;
			coedetails |= dev->mailbox->coe_completeaccess ? (0x1 << 5) : 0x0;
		}
		*(p++) = coedetails;

		uint8_t foedetails = 0x0;
		if(dev->mailbox) {
			foedetails |= dev->mailbox->foe ? 0x1 : 0x0;
		}
		*(p++) = foedetails;

		uint8_t eoedetails = 0x0;
		if(dev->mailbox) {
			eoedetails |= dev->mailbox->eoe ? 0x1 : 0x0;
		}
		*(p++) = eoedetails;

		*(p++); // SoEChannels, reserved
		*(p++); // DS402Channels, reserved
		*(p++); // SysmanClass, reserved

		uint8_t flags = 0x0;
		// flags |= StartToSafeopNoSync ? 0x1 : 0x0; // TODO Esi:Info:StateMachine:Behavior:StartToSafeopNoSync
		// flags |= Enable notLRW ? (0x1 << 1) : 0x0; // TODO Esi:DeviceType:Type
		if(dev->mailbox && dev->mailbox->datalinklayer)
			flags |= (0x1 << 2);
		// flags |= Identification ? (0x1 << 3) : 0x0; // TODO ETG2000 Table 8
		// flags |= Identification ? (0x1 << 4) : 0x0; // TODO ETG2000 Table 8
		*(p++) = flags;

		uint16_t ebuscurrent = 0;
		*(p++) = ebuscurrent & 0xFF;
		*(p++) = (ebuscurrent >> 8) & 0xFF;

		*(p++) = 0x0; // GroupIdx, index to STRINGS (compatibility duplicate)
		*(p++); // Reserved1

		uint16_t physicalport = 0x0;
		for(uint8_t ppidx = 0; ppidx < strlen(dev->physics); ++ppidx) {
			// 0x00: not use
			// 0x01: MII
			// 0x02: reserved
			// 0x03: EBUS
			// 0x04: Fast Hot Connect
			switch (dev->physics[ppidx]){
				case 'Y':
					physicalport |= 0x1 << (ppidx*4);
				break;
				case 'K': // LVDS, EBUS? TODO
					physicalport |= 0x3 << (ppidx*4);
				break;
				case 'H':
					physicalport |= 0x4 << (ppidx*4);
				break;
				case ' ':
					physicalport |= 0x0 << (ppidx*4);
				break;
			}
		}
		*(p++) = physicalport & 0xFF;
		*(p++) = (physicalport >> 8) & 0xFF;

		uint16_t physicalmemaddr = 0x0;
		*(p++) = physicalmemaddr & 0xFF;
		*(p++) = (physicalmemaddr >> 8) & 0xFF;

		p += 12; // Reserved2

		// FMMU category if needed
		if(dev->fmmus.size() > 0) {
			*(p++) = EEPROMCategoryFMMU & 0xFF;
			*(p++) = (EEPROMCategoryFMMU >> 8) & 0xFF;

			uint16_t fmmucatlen = dev->fmmus.size();
			uint8_t fmmupadding = fmmucatlen % 2;
			fmmucatlen += fmmupadding;
			fmmucatlen /= 2;

			*(p++) = fmmucatlen & 0xFF;
			*(p++) = (fmmucatlen >> 8) & 0xFF;

			for(auto fmmu : dev->fmmus) {
				if(0 == strcmp(fmmu->type,"Outputs")) {
					*(p++) = 0x1;
				} else if(0 == strcmp(fmmu->type,"Inputs")) {
					*(p++) = 0x2;
				} else if(0 == strcmp(fmmu->type,"MBoxState")) {
					*(p++) = 0x3;
				} else{
					*(p++) = 0x0;
				}
				// TODO future dynamic thingies
			}
			p += fmmupadding;
		}

		// SyncManager category if needed
		if(dev->syncmanagers.size() > 0) {
			*(p++) = EEPROMCategorySyncM & 0xFF;
			*(p++) = (EEPROMCategorySyncM >> 8) & 0xFF;

			uint16_t smcatlen = dev->syncmanagers.size() * 8;
			uint8_t smpadding = (smcatlen % 2) * 8;
			smcatlen += smpadding;
			smcatlen /= 2;

			*(p++) = smcatlen & 0xFF;
			*(p++) = (smcatlen >> 8) & 0xFF;

			for(auto sm : dev->syncmanagers) {
				*(p++) = sm->startaddress & 0xFF;
				*(p++) = (sm->startaddress >> 8) & 0xFF;

				*(p++) = sm->defaultsize & 0xFF;
				*(p++) = (sm->defaultsize >> 8) & 0xFF;

				*(p++) = sm->controlbyte;
				*(p++) = 0x0; // Status, dont care

				uint8_t enableSM = sm->enable ? 0x1 : 0x0;
				// TODO additional bits
				*(p++) = enableSM;

				if(0 == strcmp(sm->type,"MBoxOut")) {
					*(p++) = 0x1;
				} else if(0 == strcmp(sm->type,"MBoxIn")) {
					*(p++) = 0x2;
				} else if(0 == strcmp(sm->type,"Outputs")) {
					*(p++) = 0x3;
				} else if(0 == strcmp(sm->type,"Inputs")) {
					*(p++) = 0x4;
				} else{
					*(p++) = 0x0;
				}
				// TODO future dynamic thingies
			}
			p += smpadding;
		}

		if(encodepdo && (dev->mailbox && !dev->mailbox->coe_sdoinfo)) {
			// FMMU_EX
		}

		// TXPDO category if needed
		if(encodepdo && !dev->txpdo.empty())
		{
			for(Pdo* pdo : dev->txpdo) {
				*(p++) = EEPROMCategoryTXPDO & 0xFF;
				*(p++) = (EEPROMCategoryTXPDO >> 8) & 0xFF;
				// Each PDO entry takes 8 bytes, and a "header" of 8 bytes
				uint16_t txpdocatlen = ((pdo->entries.size() * 0x8) + 8) >> 1;
				*(p++) = txpdocatlen & 0xFF;
				*(p++) = (txpdocatlen >> 8) & 0xFF;

				uint16_t index = pdo->index & 0xFFFF; // HexDec
				*(p++) = index & 0xFF;
				*(p++) = (index >> 8) & 0xFF;
	
				*(p++) = pdo->entries.size() & 0xFF;
				*(p++) = pdo->syncmanager;
				*(p++) = 0x0; // TODO Fixme, DC
				*(p++) = 0x0; // TODO Name index to STRINGS

				uint16_t flags = 0x0;
				if(pdo->mandatory) flags |= 0x0001;
				if(pdo->fixed) flags |= 0x0010;
				// TODO more flags...
				*(p++) = flags & 0xFF;
				*(p++) = (flags >> 8) & 0xFF;

				for(PdoEntry* entry : pdo->entries) {
					index = entry->index & 0xFFFF;
					*(p++) = index & 0xFF;
					*(p++) = (index >> 8) & 0xFF;
					*(p++) = entry->subindex & 0xFF;
					*(p++) = 0x0; // TODO Name entry into STRINGS
					*(p++) = getCoEDataType(entry->datatype);
					*(p++) = entry->bitlen & 0xFF;
					*(p++) = 0x0; // Reserved, flags
					*(p++) = 0x0; // Reserved, flags
				}
			}
		}

		// RXPDO category if needed
		if(encodepdo && !dev->rxpdo.empty())
		{
			for(Pdo* pdo : dev->rxpdo) {
				*(p++) = EEPROMCategoryRXPDO & 0xFF;
				*(p++) = (EEPROMCategoryRXPDO >> 8) & 0xFF;
				// Each PDO entry takes 8 bytes, and a "header" of 8 bytes
				uint16_t rxpdocatlen = ((pdo->entries.size() * 0x8) + 8) >> 1;
				*(p++) = rxpdocatlen & 0xFF;
				*(p++) = (rxpdocatlen >> 8) & 0xFF;

				uint16_t index = pdo->index & 0xFFFF; // HexDec
				*(p++) = index & 0xFF;
				*(p++) = (index >> 8) & 0xFF;
				*(p++) = pdo->entries.size() & 0xFF;
				*(p++) = pdo->syncmanager;
				*(p++) = 0x0; // TODO Fixme, DC
				*(p++) = 0x0; // TODO Name index to STRINGS

				uint16_t flags = 0x0;
				if(pdo->mandatory) flags |= 0x0001;
				if(pdo->fixed) flags |= 0x0010;
				// TODO more flags...
				*(p++) = flags & 0xFF;
				*(p++) = (flags >> 8) & 0xFF;

				for(PdoEntry* entry : pdo->entries) {
					index = entry->index & 0xFFFF;
					*(p++) = index & 0xFF;
					*(p++) = (index >> 8) & 0xFF;
					*(p++) = entry->subindex & 0xFF;
					*(p++) = 0x0; // TODO Name entry into STRINGS
					*(p++) = getCoEDataType(entry->datatype);
					*(p++) = entry->bitlen & 0xFF;
					*(p++) = 0x0; // Reserved, flags
					*(p++) = 0x0; // Reserved, flags
				}
			}
		}

		// SyncUnit if necessary
		if(dev->syncunit)
		{
			*(p++) = EEPROMCategorySyncUnit & 0xFF;
			*(p++) = (EEPROMCategorySyncUnit >> 8) & 0xFF;
			// For now, its 1 byte long
			*(p++) = (0x1) & 0xFF;
			*(p++) = (0x1 >> 8) & 0xFF;
			// TODO
			*(p++) = (0x0 & 0xFF);
			*(p++) = (0x0 >> 8) & 0xFF;
		}

		// DC category if needed
		if(dev->dc) {
			*(p++) = EEPROMCategoryDC & 0xFF;
			*(p++) = (EEPROMCategoryDC >> 8) & 0xFF;
			// Each DC Opmode is 0xC words
			uint16_t dccatlen = dev->dc->opmodes.size() * 0xC;
			*(p++) = dccatlen & 0xFF;
			*(p++) = (dccatlen >> 8) & 0xFF;

			for(auto dc : dev->dc->opmodes) {
				uint32_t cts0 = dc->cycletimesync0;
				*(p++) = cts0 & 0xFF;
				*(p++) = (cts0 >> 8) & 0xFF;
				*(p++) = (cts0 >> 16) & 0xFF;
				*(p++) = (cts0 >> 24) & 0xFF;

				uint32_t sts0 = dc->shifttimesync0;
				*(p++) = sts0 & 0xFF;
				*(p++) = (sts0 >> 8) & 0xFF;
				*(p++) = (sts0 >> 16) & 0xFF;
				*(p++) = (sts0 >> 24) & 0xFF;

				uint32_t sts1 = dc->shifttimesync1;
				*(p++) = sts1 & 0xFF;
				*(p++) = (sts1 >> 8) & 0xFF;
				*(p++) = (sts1 >> 16) & 0xFF;
				*(p++) = (sts1 >> 24) & 0xFF;

				int16_t cts1f = dc->cycletimesync1factor;
				*(p++) = cts1f & 0xFF;
				*(p++) = (cts1f >> 8) & 0xFF;

				uint16_t aa = dc->assignactivate;
				*(p++) = aa & 0xFF;
				*(p++) = (aa >> 8) & 0xFF;

				int16_t cts0f = dc->cycletimesync0factor;
				*(p++) = cts0f & 0xFF;
				*(p++) = (cts0f >> 8) & 0xFF;

				*(p++) = 0x0; // Name index into STRINGS, unsupported TODO
				*(p++) = 0x0; // Description index into STRINGS, unsupported TODO
				p += 4; // Reserved
			}
		}

		*(p++) = 0xFF; // End
		*(p++) = 0xFF;

		if(verbose) {
			printf("EEPROM contents:\n");
			// Print EEPROM data
			for(uint16_t i = 0; i < EC_SII_EEPROM_SIZE; i=i+2)
				printf("%04X / %04X: %.02X %.02X\n",i/2,i,sii_eeprom[i],sii_eeprom[i+1]);
		}

		printf("Writing EEPROM...");
		for(uint32_t i = 0; i < EC_SII_EEPROM_SIZE; ++i) out << sii_eeprom[i];
		printf("Done\n");
		out.sync_with_stdio();
		out.close();

		delete sii_eeprom;
	} else {
		printf("Failed writing EEPROM data to '%s'\n",output.c_str());
	}
}