#include <cstring>
#include <string>
#include <errno.h>
#include <fstream>
#include <cstdint>
#include <list>
#include "tinyxml2/tinyxml2.h"

#define ESI_ROOTNODE_NAME	"EtherCATInfo"
#define ESI_VENDOR_NAME		"Vendor"
#define ESI_ID_NAME		"Id"
#define ESI_DEVICE_TYPE_NAME	"Type"
#define ESI_DEVICE_PRODUCTCODE_ATTR_NAME	"ProductCode"
#define ESI_DEVICE_REVISIONNO_ATTR_NAME		"RevisionNo"

#define EC_SII_EEPROM_VENDOR_OFFSET_BYTE	(0x08 * 2)
#define EC_SII_EEPROM_SIZE			(1024)
#define EC_SII_CONFIGDATA_SIZEB			(16)

unsigned char crc8(unsigned char* ptr, unsigned char len)
{
	unsigned char i;
	unsigned char crc = 0xff;/*  The initial of the calculation crc value  */

	while (len--)
	{
		crc ^= *ptr++;  /*  XOR with the data to be calculated each time , After calculation, point to the next data  */
		for (i = 8; i > 0; --i)   /*  The following calculation process is the same as calculating a byte crc equally  */
		{
			if (crc & 0x80) crc = (crc << 1) ^ 0x07;// polynomial
			else crc = (crc << 1);
		}
	}
	return (crc);
}

uint8_t* sii_eeprom = NULL;

uint32_t vendor_id = 0x0;
const char* vendor_name = NULL;

struct Group {
	const char* name = NULL;
	const char* type = NULL;
};

struct DistributedClock {
	const char* name = NULL;
	const char* desc = NULL;
	uint16_t assignactivate = 0;
};

struct SyncManager {
	const char* type = NULL;
	uint16_t minsize = 0;
	uint16_t maxsize = 0;
	uint16_t defaultsize = 0;
	uint16_t startaddress = 0;
	uint8_t controlbyte = 0;
	bool enable = false;
};

struct FMMU {
	const char* type = NULL;
	int syncmanager = -1;
	int syncunit = -1;
};

struct Mailbox {
	bool datalinklayer = false;
	bool coe = false;
	bool coe_sdoinfo = false;
	bool coe_pdoassign = false;
	bool coe_pdoconfig = false;
	bool coe_pdoupload = false;
	bool coe_completeaccess = false;
};

struct PdoEntry {
	bool fixed = false;
	const char* index = NULL;
	uint32_t subindex = 0;
	uint16_t bitlen = 0;
	const char* datatype = NULL;
	const char* name = NULL;
};

struct Pdo {
	bool fixed = false;
	bool mandatory = false;
	int syncmanager = 2;
	int syncunit = 2;
	const char* index = NULL;
	const char* name = NULL;
	std::list<PdoEntry*> entries;
};

struct Device {
	uint32_t product_code = 0x0;
	uint32_t revision_no = 0x0;
	const char* name = NULL;
	const char* physics = NULL;
	Group* group = NULL;
	const char* type = NULL;
	std::list<FMMU*> fmmus;
	std::list<SyncManager*> syncmanagers;
	std::list<Mailbox*> mailboxes;
	DistributedClock* dc = NULL;
	uint32_t eepromsize = 0x0;
	uint8_t configdata[EC_SII_CONFIGDATA_SIZEB];
	std::list<Pdo*> txpdo;
	std::list<Pdo*> rxpdo;
};

std::list<Group*> groups;
std::list<Device*> devices;

void printUsage(const char* name) {
	printf("Usage: %s [options] --input <input-file>\n",name);
	printf("Options:\n");
	printf("\t --decode : Decode a binary SII file\n");
	printf("\n");
}

int decodeSII(const std::string& file) {
	printf("Decoding '%s'\n",file.c_str());
	return 0;
}

uint32_t EC_SII_HexToUint32(const char* s) {
	char c[strlen(s)+1]; // Wooooo lord stringlengths and input sanitizing
	strcpy(c,s);
	uint32_t r = 0;
	if(c[0] == '#' && (c[1] == 'x' || c[1] == 'X')) {
		c[0] = '0'; // Make it easy for sscanf
		if(1 != sscanf(c,"%x",&r)) r = 0;
	}
	return r;
}

void parseXMLGroup(const tinyxml2::XMLElement* xmlgroup) {
	Group* group = new Group();
	for (const tinyxml2::XMLElement* child = xmlgroup->FirstChildElement();
		child != 0; child = child->NextSiblingElement())
	{
		if(0 == strcmp(child->Name(),"Name")) {
			group->name = child->GetText();
		} else
		if(0 == strcmp(child->Name(),"Type")) {
			group->type = child->GetText();
		} else
		{
			printf("Unhandled Group element '%s':'%s'\n",child->Name(),child->Value());
		}
	}
	groups.push_back(group);
}

void parseXMLMailbox(const tinyxml2::XMLElement* xmlmailbox,Device* dev) {
	Mailbox* mb = new Mailbox();
	for (const tinyxml2::XMLAttribute* attr = xmlmailbox->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		if(0 == strcmp(attr->Name(),"DataLinkLayer")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&mb->datalinklayer)) {
				mb->datalinklayer = (attr->IntValue() == 1) ? true : false;
			}
			printf("Mailbox/@DataLinkLayer: %s ('%s')\n",mb->datalinklayer?"yes":"no",attr->Value());
		} else
		{
			printf("Unhandled Device/Mailbox Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
		}
	}
	for (const tinyxml2::XMLElement* mboxchild = xmlmailbox->FirstChildElement();
		mboxchild != 0; mboxchild = mboxchild->NextSiblingElement())
	{
		if(0 == strcmp(mboxchild->Name(),"CoE")) {
			printf("Mailbox/CoE Enabled\n");
			mb->coe = true;
			for (const tinyxml2::XMLAttribute* coeattr = mboxchild->FirstAttribute();
				coeattr != 0; coeattr = coeattr->Next())
			{
				if(0 == strcmp(coeattr->Name(),"SdoInfo")) {
					if(tinyxml2::XML_SUCCESS != coeattr->QueryBoolValue(&mb->coe_sdoinfo))
						mb->coe_sdoinfo = (coeattr->IntValue() == 1) ? true : false;
					printf("Mailbox/CoE/@SdoInfo: %s ('%s')\n",mb->coe_sdoinfo?"yes":"no",coeattr->Value());
				} else
				if(0 == strcmp(coeattr->Name(),"PdoAssign")) {
					if(tinyxml2::XML_SUCCESS != coeattr->QueryBoolValue(&mb->coe_pdoassign))
						mb->coe_pdoassign = coeattr->IntValue() == 1 ? true : false;
					printf("Mailbox/CoE/@PdoAssign: %s ('%s')\n",mb->coe_pdoassign?"yes":"no",coeattr->Value());
				} else
				if(0 == strcmp(coeattr->Name(),"PdoConfig")) {
					if(tinyxml2::XML_SUCCESS != coeattr->QueryBoolValue(&mb->coe_pdoconfig))
						mb->coe_pdoconfig = coeattr->IntValue() == 1 ? true : false;
					printf("Mailbox/CoE/@PdoConfig: %s ('%s')\n",mb->coe_pdoconfig?"yes":"no",coeattr->Value());
				} else
				if(0 == strcmp(coeattr->Name(),"PdoUpload")) {
					if(tinyxml2::XML_SUCCESS != coeattr->QueryBoolValue(&mb->coe_pdoupload))
						mb->coe_pdoupload = coeattr->IntValue() == 1 ? true : false;
					printf("Mailbox/CoE/@PdoUpload: %s ('%s')\n",mb->coe_pdoupload?"yes":"no",coeattr->Value());
				} else
				if(0 == strcmp(coeattr->Name(),"CompleteAccess")) {
					if(tinyxml2::XML_SUCCESS != coeattr->QueryBoolValue(&mb->coe_completeaccess))
						mb->coe_completeaccess = coeattr->IntValue() == 1 ? true : false;
					printf("Mailbox/CoE/@CompleteAccess: %s ('%s')\n",mb->coe_completeaccess?"yes":"no",coeattr->Value());
				}
				else {
					printf("Unhandled Device/Mailbox/CoE attribute: '%s' = '%s'\n",coeattr->Name(),coeattr->Value());
				}
			}
		} else
		{
			printf("Unhandled Device/Mailbox element '%s':'%s'\n",mboxchild->Name(),mboxchild->GetText());
		}
	}
	dev->mailboxes.push_back(mb);
}

void parseXMLPdo(const tinyxml2::XMLElement* xmlpdo, std::list<Pdo*>* pdolist) {
	Pdo* pdo = new Pdo();
	for (const tinyxml2::XMLAttribute* attr = xmlpdo->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		if(0 == strcmp(attr->Name(),"Mandatory")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&pdo->mandatory))
				pdo->mandatory = (attr->IntValue() == 1) ? true : false;
			printf("Device/%s/@Mandatory: %s ('%s')\n",xmlpdo->Name(),pdo->mandatory ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"Fixed")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&pdo->fixed))
				pdo->fixed = (attr->IntValue() == 1) ? true : false;
			printf("Device/%s/@Fixed: %s ('%s')\n",xmlpdo->Name(),pdo->fixed ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"Sm")) {
			pdo->syncmanager = attr->IntValue();
			printf("Device/%s/@Sm: '%d'\n",xmlpdo->Name(),pdo->syncmanager);
		} else
		if(0 == strcmp(attr->Name(),"Su")) {
			pdo->syncunit = attr->IntValue();
			printf("Device/%s/@Su: '%d'\n",xmlpdo->Name(),pdo->syncunit);
		} else
		{
			printf("Unhandled Device/%s Attribute: '%s' = '%s'\n",xmlpdo->Name(),attr->Name(),attr->Value());
		}
	}
	for (const tinyxml2::XMLElement* pdochild = xmlpdo->FirstChildElement();
		pdochild != 0; pdochild = pdochild->NextSiblingElement())
	{
		if(0 == strcmp(pdochild->Name(),"Index")) {
			pdo->index = pdochild->GetText();
			printf("Device/%s/Index: '%s'\n",xmlpdo->Name(),pdo->index);
		} else
		if(0 == strcmp(pdochild->Name(),"Name")) {
			pdo->name = pdochild->GetText();
			printf("Device/%s/Name: '%s'\n",xmlpdo->Name(),pdo->name);
		} else
		if(0 == strcmp(pdochild->Name(),"Entry")) {
			PdoEntry* entry = new PdoEntry();
			for (const tinyxml2::XMLElement* entrychild = pdochild->FirstChildElement();
				entrychild != 0; entrychild = entrychild->NextSiblingElement())
			{
				if(0 == strcmp(entrychild->Name(),"Name")) {
					entry->name = entrychild->GetText();
					printf("Device/%s/Entry/Name: '%s'\n",xmlpdo->Name(),entry->name);
				} else
				if(0 == strcmp(entrychild->Name(),"Index")) {
					entry->index = entrychild->GetText();
					printf("Device/%s/Entry/Index: '%s'\n",xmlpdo->Name(),entry->index);
				} else
				if(0 == strcmp(entrychild->Name(),"BitLen")) {
					entry->bitlen = entrychild->IntText();
					printf("Device/%s/Entry/BitLen: '%d'\n",xmlpdo->Name(),entry->bitlen);
				} else
				if(0 == strcmp(entrychild->Name(),"SubIndex")) {
					entry->subindex = entrychild->IntText(); // TODO: HexDec
					printf("Device/%s/Entry/SubIndex: '%d'\n",xmlpdo->Name(),entry->subindex);
				} else
				if(0 == strcmp(entrychild->Name(),"DataType")) {
					entry->datatype = entrychild->GetText();
					printf("Device/%s/Entry/DataType: '%s'\n",xmlpdo->Name(),entry->datatype);
				} else
				{
					printf("Unhandled Device/%s/Entry Element: '%s' = '%s'\n",xmlpdo->Name(),entrychild->Name(),entrychild->GetText());
				}
			}
			pdo->entries.push_back(entry);
		} else
		{
			printf("Unhandled Device/%s Element: '%s' = '%s'\n",xmlpdo->Name(),pdochild->Name(),pdochild->GetText());
		}
	}
	pdolist->push_back(pdo);
}

void parseXMLDevice(const tinyxml2::XMLElement* xmldevice) {
	Device* dev = new Device();
	memset(dev->configdata,0,EC_SII_CONFIGDATA_SIZEB);

	for (const tinyxml2::XMLAttribute* attr = xmldevice->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		if(0 == strcmp(attr->Name(),"Physics")) {
			dev->physics = attr->Value();
		} else
		{
			printf("Unhandled Device Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
		}
	}
	for (const tinyxml2::XMLElement* child = xmldevice->FirstChildElement();
		child != 0; child = child->NextSiblingElement())
	{
		if(0 == strcmp(child->Name(),"Name")) {
			dev->name = child->GetText();
		} else
		if(0 == strcmp(child->Name(),"Type")) {
			dev->type = child->GetText();
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"ProductCode")) {
					dev->product_code = EC_SII_HexToUint32(attr->Value());
					printf("Device/Type/@ProductCode: 0x%.08X\n",dev->product_code);
				} else
				if(0 == strcmp(attr->Name(),"RevisionNo")) {
					dev->revision_no = EC_SII_HexToUint32(attr->Value());
					printf("Device/Type/@RevisionNo: 0x%.08X\n",dev->revision_no);
				} else
				{
					printf("Unhandled Device/Type Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
				}
			}
		} else
		if(0 == strcmp(child->Name(),"GroupType")) {
			const char * g = child->GetText();
			if(NULL != g) {
				for(auto grp : groups) {
					if(0 == strcmp(grp->type,g)) {
						dev->group = grp;
						printf("Device belongs to grouptype '%s' ('%s')\n",grp->type,g);
						break;
					}
				}
			}
		} else
		if(0 == strcmp(child->Name(),"Mailbox")) {
			parseXMLMailbox(child,dev);
		} else
		if(0 == strcmp(child->Name(),"Fmmu")) {
			FMMU* fmmu = new FMMU();
			fmmu->type = child->GetText();
			printf("Device/Fmmu: %s\n",fmmu->type);
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"Sm")) {
					fmmu->syncmanager = attr->IntValue();
				} else
				if(0 == strcmp(attr->Name(),"Su")) {
					fmmu->syncunit = attr->IntValue();
				} else
				{
					printf("Unhandled Device/Fmmu Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
				}
			}
			dev->fmmus.push_back(fmmu);
		} else
		if(0 == strcmp(child->Name(),"Eeprom")) {
			for (const tinyxml2::XMLElement* eepchild = child->FirstChildElement();
				eepchild != 0; eepchild = eepchild->NextSiblingElement())
			{
				if(0 == strcmp(eepchild->Name(),"ConfigData")) {
					const char* data = eepchild->GetText();
					const char* ptr = data;
					for(unsigned int p = 0,b=0; p < strlen(data); p += 2,ptr += 2,++b) {
						char s[5] = {'0','x',*ptr,*(ptr+1),'\0'};
						uint32_t i;
						if(1 == sscanf(s,"%x",&i))
							dev->configdata[b] = i;
						else {
							printf("Failed deciphering configdata byte '%s'\n",s);
							break;
						}
					}
					// Calculate CRC8 value of the first 7 words
					dev->configdata[EC_SII_CONFIGDATA_SIZEB-2] =
						crc8(dev->configdata,EC_SII_CONFIGDATA_SIZEB-2);
					printf("Device/Eeprom/ConfigData: ");
					for(uint8_t i = 0; i < EC_SII_CONFIGDATA_SIZEB; ++i) {
						if(i == EC_SII_CONFIGDATA_SIZEB-1) printf("%.02X",dev->configdata[i]);
						else  printf("%.02X ",dev->configdata[i]);
					}
					printf("\n");
				} else
				if(0 == strcmp(eepchild->Name(),"ByteSize")) {
					dev->eepromsize = eepchild->UnsignedText();
				} else
				{
					printf("Unhandled Device/Eeprom element: '%s' = '%s'\n",eepchild->Name(),eepchild->GetText());
				}
			}
		} else
		if(0 == strcmp(child->Name(),"Dc")) {
			DistributedClock* dc = new DistributedClock();
			for (const tinyxml2::XMLElement* dcchild = child->FirstChildElement();
				dcchild != 0; dcchild = dcchild->NextSiblingElement())
			{
				if(0 == strcmp(dcchild->Name(),"OpMode")) {
					for (const tinyxml2::XMLElement* dcopmodechild = dcchild->FirstChildElement();
						dcopmodechild != 0; dcopmodechild = dcopmodechild->NextSiblingElement())
					{
						if(0 == strcmp(dcopmodechild->Name(),"Name")) {
							dc->name = dcopmodechild->GetText();
							printf("Device/Dc/Opmode/Name: %s\n",dc->name);
						} else
						if(0 == strcmp(dcopmodechild->Name(),"Desc")) {
							dc->desc = dcopmodechild->GetText();
							printf("Device/Dc/Opmode/Desc: %s\n",dc->desc);
						} else
						if(0 == strcmp(dcopmodechild->Name(),"AssignActivate")) {
							dc->assignactivate = EC_SII_HexToUint32(dcopmodechild->GetText());
							printf("Device/Dc/Opmode/AssignActivate: 0x%.04X\n",dc->assignactivate);
						}
						else {
							printf("Unhandled Device/Dc/Opmode element: '%s' = '%s'\n",dcopmodechild->Name(),dcopmodechild->GetText());
						}
					}
				} else
				{
					printf("Unhandled Device/Dc element: '%s' = '%s'\n",dcchild->Name(),dcchild->GetText());
				}
			}
			dev->dc = dc;
		} else
		if(0 == strcmp(child->Name(),"Sm")) {
			SyncManager* sm = new SyncManager();
			sm->type = child->GetText();
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"DefaultSize")) {
					uint32_t defaultsize = attr->UnsignedValue();
					if(0 == defaultsize) defaultsize = EC_SII_HexToUint32(attr->Value());
					if(0 == defaultsize) printf("Failed to decipher SyncManager DefaultSize\n");
					printf("Device/Sm/@DefaultSize: 0x%.04X\n",sm->defaultsize);
				} else
				if(0 == strcmp(attr->Name(),"Enable")) { // hexdecvalue
					sm->enable = attr->UnsignedValue() > 0 ? true : false;
					printf("Device/Sm/@Enable: '%s'\n",sm->enable ? "yes" : "no");
				} else
				if(0 == strcmp(attr->Name(),"ControlByte")) { // hexdecvalue
					sm->controlbyte = (EC_SII_HexToUint32(attr->Value()) & 0xFF);
					printf("Device/Sm/@ControlByte: 0x%.02X\n",sm->controlbyte);
				} else
				if(0 == strcmp(attr->Name(),"StartAddress")) {
					sm->startaddress = EC_SII_HexToUint32(attr->Value()) & 0xFFFF;
					printf("Device/Sm/@StartAddress: 0x%.04X\n",sm->startaddress);
				} else
				if(0 == strcmp(attr->Name(),"MinSize")) {
					sm->minsize = EC_SII_HexToUint32(attr->Value()) & 0xFFFF;
					printf("Device/Sm/@MinSize: 0x%.04X\n",sm->minsize);
				} else
				if(0 == strcmp(attr->Name(),"MaxSize")) {
					sm->maxsize = EC_SII_HexToUint32(attr->Value()) & 0xFFFF;
					printf("Device/Sm/@MaxSize: 0x%.04X\n",sm->maxsize);
				} else
				{
					printf("Unhandled Device/Sm Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
				}
			}
			dev->syncmanagers.push_back(sm);
		} else
		if(0 == strcmp (child->Name(),"TxPdo")) {
			parseXMLPdo(child,&(dev->txpdo));
/*			Pdo* txpdo = new Pdo();
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"Mandatory")) {
					if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&txpdo->mandatory))
						txpdo->mandatory = (attr->IntValue() == 1) ? true : false;
					printf("Device/TxPdo/@Mandatory: %s ('%s')\n",txpdo->mandatory ? "yes" : "no",attr->Value());
				} else
				if(0 == strcmp(attr->Name(),"Fixed")) {
					if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&txpdo->fixed))
						txpdo->fixed = (attr->IntValue() == 1) ? true : false;
					printf("Device/TxPdo/@Fixed: %s ('%s')\n",txpdo->fixed ? "yes" : "no",attr->Value());
				} else
				if(0 == strcmp(attr->Name(),"Sm")) {
					txpdo->syncmanager = attr->IntValue();
					printf("Device/TxPdo/@Sm: '%d'\n",txpdo->syncmanager);
				} else
				if(0 == strcmp(attr->Name(),"Su")) {
					txpdo->syncunit = attr->IntValue();
					printf("Device/TxPdo/@Su: '%d'\n",txpdo->syncunit);
				} else
				{
					printf("Unhandled Device/TxPdo Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
				}
			}
			for (const tinyxml2::XMLElement* pdochild = child->FirstChildElement();
				pdochild != 0; pdochild = pdochild->NextSiblingElement())
			{
				if(0 == strcmp(pdochild->Name(),"Index")) {
					txpdo->index = pdochild->GetText();
					printf("Device/TxPdo/Index: '%s'\n",txpdo->index);
				} else
				if(0 == strcmp(pdochild->Name(),"Name")) {
					txpdo->name = pdochild->GetText();
					printf("Device/TxPdo/Name: '%s'\n",txpdo->name);
				} else
				if(0 == strcmp(pdochild->Name(),"Entry")) {
					PdoEntry* entry = new PdoEntry();
					for (const tinyxml2::XMLElement* entrychild = pdochild->FirstChildElement();
						entrychild != 0; entrychild = entrychild->NextSiblingElement())
					{
						if(0 == strcmp(entrychild->Name(),"Name")) {
							entry->name = entrychild->GetText();
							printf("Device/TxPdo/Entry/Name: '%s'\n",entry->name);
						} else
						if(0 == strcmp(entrychild->Name(),"Index")) {
							entry->index = entrychild->GetText();
							printf("Device/TxPdo/Entry/Index: '%s'\n",entry->index);
						} else
						if(0 == strcmp(entrychild->Name(),"BitLen")) {
							entry->bitlen = entrychild->IntText();
							printf("Device/TxPdo/Entry/BitLen: '%d'\n",entry->bitlen);
						} else
						if(0 == strcmp(entrychild->Name(),"SubIndex")) {
							entry->subindex = entrychild->IntText(); // TODO: HexDec
							printf("Device/TxPdo/Entry/SubIndex: '%d'\n",entry->subindex);
						} else
						if(0 == strcmp(entrychild->Name(),"DataType")) {
							entry->datatype = entrychild->GetText();
							printf("Device/TxPdo/Entry/DataType: '%s'\n",entry->datatype);
						} else
						{
							printf("Unhandled Device/TxPdo/Entry Element: '%s' = '%s'\n",entrychild->Name(),entrychild->GetText());
						}
					}
					txpdo->entries.push_back(entry);
				} else
				{
					printf("Unhandled Device/TxPdo Element: '%s' = '%s'\n",pdochild->Name(),pdochild->GetText());
				}
			}
			dev->txpdo.push_back(txpdo);*/
		} else
		if(0 == strcmp (child->Name(),"RxPdo")) {
			parseXMLPdo(child,&(dev->rxpdo));
/*			Pdo* rxpdo = new Pdo();
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"Mandatory")) {
					if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&rxpdo->mandatory))
						rxpdo->mandatory = (attr->IntValue() == 1) ? true : false;
					printf("Device/RxPdo/@Mandatory: %s ('%s')\n",rxpdo->mandatory ? "yes" : "no",attr->Value());
				} else
				if(0 == strcmp(attr->Name(),"Fixed")) {
					if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&rxpdo->fixed))
						rxpdo->fixed = (attr->IntValue() == 1) ? true : false;
					printf("Device/RxPdo/@Fixed: %s ('%s')\n",rxpdo->fixed ? "yes" : "no",attr->Value());
				} else
				if(0 == strcmp(attr->Name(),"Sm")) {
					rxpdo->syncmanager = attr->IntValue();
					printf("Device/RxPdo/@Sm: '%d'\n",rxpdo->syncmanager);
				} else
				if(0 == strcmp(attr->Name(),"Su")) {
					rxpdo->syncunit = attr->IntValue();
					printf("Device/RxPdo/@Su: '%d'\n",rxpdo->syncunit);
				} else
				{
					printf("Unhandled Device/RxPdo Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
				}
			}
			for (const tinyxml2::XMLElement* pdochild = child->FirstChildElement();
				pdochild != 0; pdochild = pdochild->NextSiblingElement())
			{
				if(0 == strcmp(pdochild->Name(),"Index")) {
					rxpdo->index = pdochild->GetText();
					printf("Device/RxPdo/Index: '%s'\n",rxpdo->index);
				} else
				if(0 == strcmp(pdochild->Name(),"Name")) {
					rxpdo->name = pdochild->GetText();
					printf("Device/RxPdo/Name: '%s'\n",rxpdo->name);
				} else
				if(0 == strcmp(pdochild->Name(),"Entry")) {
					PdoEntry* entry = new PdoEntry();
					for (const tinyxml2::XMLElement* entrychild = pdochild->FirstChildElement();
						entrychild != 0; entrychild = entrychild->NextSiblingElement())
					{
						if(0 == strcmp(entrychild->Name(),"Name")) {
							entry->name = entrychild->GetText();
							printf("Device/RxPdo/Entry/Name: '%s'\n",entry->name);
						} else
						if(0 == strcmp(entrychild->Name(),"Index")) {
							entry->index = entrychild->GetText();
							printf("Device/RxPdo/Entry/Index: '%s'\n",entry->index);
						} else
						if(0 == strcmp(entrychild->Name(),"BitLen")) {
							entry->bitlen = entrychild->IntText();
							printf("Device/RxPdo/Entry/BitLen: '%d'\n",entry->bitlen);
						} else
						if(0 == strcmp(entrychild->Name(),"SubIndex")) {
							entry->subindex = entrychild->IntText(); // TODO: HexDec
							printf("Device/RxPdo/Entry/SubIndex: '%d'\n",entry->subindex);
						} else
						if(0 == strcmp(entrychild->Name(),"DataType")) {
							entry->datatype = entrychild->GetText();
							printf("Device/RxPdo/Entry/DataType: '%s'\n",entry->datatype);
						} else
						{
							printf("Unhandled Device/RxPdo/Entry Element: '%s' = '%s'\n",entrychild->Name(),entrychild->GetText());
						}
					}
					rxpdo->entries.push_back(entry);
				} else
				{
					printf("Unhandled Device/RxPdo Element: '%s' = '%s'\n",pdochild->Name(),pdochild->GetText());
				}
			}
			dev->rxpdo.push_back(rxpdo);*/
		} else
		{
			printf("Unhandled Device element '%s':'%s'\n",child->Name(),child->GetText());
		}
	}
	devices.push_back(dev);
}

void parseXMLVendor(const tinyxml2::XMLElement* xmlvendor) {
	for (const tinyxml2::XMLElement* child = xmlvendor->FirstChildElement();
		child != 0; child = child->NextSiblingElement())
	{
		if(0 == strcmp(child->Name(),"Id")) {
	/*		uint8_t* ptr = &sii_eeprom[EC_SII_EEPROM_VENDOR_OFFSET_BYTE];
			uint32_t id = EC_SII_HexToUint32(element->GetText());
			sii_eeprom[EC_SII_EEPROM_VENDOR_OFFSET_BYTE]   = (id & 0xFF);
			sii_eeprom[EC_SII_EEPROM_VENDOR_OFFSET_BYTE+1] = (id >> 8) & 0xFF;
			sii_eeprom[EC_SII_EEPROM_VENDOR_OFFSET_BYTE+2] = (id >> 16) & 0xFF;
			sii_eeprom[EC_SII_EEPROM_VENDOR_OFFSET_BYTE+3] = (id >> 24) & 0xFF;
			printf("Vendor ID unsigned: 0x%.04X\n",id);*/
			vendor_id = EC_SII_HexToUint32(child->GetText());
			printf("Vendor ID: 0x%.08X\n",vendor_id);
		} else
		if(0 == strcmp(child->Name(),"Name"))
		{
			vendor_name = child->GetText();
			printf("Vendor Name: '%s'\n",vendor_name);
		}
	}
}

void parseXMLElement(const tinyxml2::XMLElement* element, void* data = NULL) {
	if(NULL == element) return;
	printf("Element name: '%s'\n",element->Name());
	if(element->GetText()) printf("Element text: '%s'\n",element->GetText());
	for (const tinyxml2::XMLAttribute* attr = element->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		printf("Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
	}
	for (const tinyxml2::XMLElement* child = element->FirstChildElement();
		child != 0; child = child->NextSiblingElement())
	{
		if(0 == strcmp(child->Name(),"Group")) {
			parseXMLGroup(child);
		} else
		if(0 == strcmp(child->Name(),"Device")) {
			parseXMLDevice(child);
		} else
		if(0 == strcmp (child->Name(),"Vendor")) {
			parseXMLVendor(child);
		} else
		{
			printf("Child element name: '%s'\n",child->Name());
			if(!child->NoChildren()) parseXMLElement(child);
			else printf("Unhandled element '%s'\n",child->Name());
		}
	}
	return;
}

int encodeSII(const std::string& file, std::string output = "") {
	sii_eeprom = new uint8_t[EC_SII_EEPROM_SIZE];
	memset(sii_eeprom,0,EC_SII_EEPROM_SIZE);

	if("" == output) output = file + "_eeprom.bin";
	tinyxml2::XMLDocument doc;
	if(tinyxml2::XML_SUCCESS != doc.LoadFile( file.c_str() )) {
		printf("Could not open '%s'\n",file.c_str());
		return -EINVAL;
	}
	printf("Encoding '%s' to '%s' EEPROM\n",file.c_str(),output.c_str());
	const tinyxml2::XMLElement* root = doc.RootElement();
	if(NULL != root) {
		if(0 != strcmp(ESI_ROOTNODE_NAME,root->Name())) {
			printf("Document seemingly does not contain EtherCAT information (root node name is not '%s' but '%s')\n",ESI_ROOTNODE_NAME,root->Name());
			return -EINVAL;
		}
		parseXMLElement(root);

	} else {
		printf("Document has no root node!\n");
	}
	std::ofstream out;
	out.open(output.c_str(),std::ios::out | std::ios::binary | std::ios::trunc);
	if(!out.fail()) {
		for(uint32_t i; i < EC_SII_EEPROM_SIZE; ++i) out << sii_eeprom[i];
	} else {
		printf("Failed writing eepro data to '%s'\n",output.c_str());
	}
	return 0;
}

int main(int argc, char* argv[])
{
	// We by default assume we're encoding a XML slave specification
	bool encode = true;
	bool decode = false;
	std::string inputfile = "";
	std::string outputfile = "";
	for(int i = 0; i < argc; ++i) {
		if(0 == strcmp(argv[i],"--input") ||
		   0 == strcmp(argv[i],"-i"))
		{
			inputfile = argv[++i];
		} else
		if(0 == strcmp(argv[i],"--output") ||
		   0 == strcmp(argv[i],"-o"))
		{
			outputfile = argv[++i];
		} else
		if(0 == strcmp(argv[i],"--decode")) {
			decode = true;
			encode = false;
		}
	}
	if("" == inputfile) {
		printUsage(argv[0]);
		return -EINVAL;
	}
	if(decode) {
		return decodeSII(inputfile);
	} else if(encode) {
		return encodeSII(inputfile,outputfile);
	}
	return 0;
}