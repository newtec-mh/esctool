/**
 * @file main.cpp
 * 
 * @author Martin Hejnfelt (mh@newtec.dk)
 * @brief EtherCAT Slave Controller tool
 * @version 0.1
 * @date 2022-10-06
 * 
 * @copyright Copyright (c) 2022 Newtec A/S
 *
 * Obviously missing (amongst others):
 * Size check compared to EEPROM size
 * Many categories and XML elements
 */
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <errno.h>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <algorithm>
#include <list>
#include <vector>
#include "tinyxml2/tinyxml2.h"

#define APP_NAME				"esctool"
#define APP_VERSION				"0.1"

#define ESI_ROOTNODE_NAME			"EtherCATInfo"
#define ESI_VENDOR_NAME				"Vendor"
#define ESI_ID_NAME				"Id"
#define ESI_DEVICE_TYPE_NAME			"Type"
#define ESI_DEVICE_PRODUCTCODE_ATTR_NAME	"ProductCode"
#define ESI_DEVICE_REVISIONNO_ATTR_NAME		"RevisionNo"

#define EC_SII_EEPROM_VENDOR_OFFSET_BYTE	(0x08 * 2)
#define EC_SII_EEPROM_MAILBOX_OUT_OFFSET_BYTE	(0x18 * 2)
#define EC_SII_EEPROM_MAILBOX_IN_OFFSET_BYTE	(0x1A * 2)
#define EC_SII_EEPROM_MAILBOX_PROTO_OFFSET_BYTE	(0x1C * 2)
#define EC_SII_EEPROM_SIZE_OFFSET_BYTE		(0x3E * 2)
#define EC_SII_EEPROM_VERSION_OFFSET_BYTE	(0x3F * 2)
#define EC_SII_EEPROM_FIRST_CAT_HDR_OFFSET_BYTE	(0x40 * 2)
#define EC_SII_CONFIGDATA_SIZEB			(16)

uint32_t EC_SII_EEPROM_SIZE			(1024);

// SII / EEPROM Category definitions, note these are (mostly) in decimal
#define EEPROMCategoryNOP		(0)
// Device specific	01-09
#define EEPROMCategorySTRINGS		(10)
#define EEPROMCategoryDataTypes		(20)
#define EEPROMCategoryGeneral		(30)
#define EEPROMCategoryFMMU		(40)
#define EEPROMCategorySyncM		(41)
#define EEPROMCategoryFMMUX		(42)
#define EEPROMCategorySyncUnit		(43)
#define EEPROMCategoryTXPDO		(50)
#define EEPROMCategoryRXPDO		(51)
#define EEPROMCategoryDC		(60)
#define EEPROMCategoryTimeouts		(70)
#define EEPROMCategoryDictionary	(80)
#define EEPROMCategoryHardware		(90)
#define EEPROMCategoryVendorInformation	(100)
#define EEPROMCategoryImages		(110)
// Vendor specific			0x0800-0x1FFF
// Application specific			0x2000-0x2FFF
// Vendor specific			0x3000-0xFFFE
// End					0xFFFF


uint8_t getCoEDataType(const char* dt) {
	if(0 == strcmp(dt,"BOOL") || 0 == strcmp(dt,"BIT")) {
		return 0x01;
	} else if(0 == strcmp(dt,"SINT")) {
		return 0x02;
	} else if(0 == strcmp(dt,"INT")) {
		return 0x03;
	} else if(0 == strcmp(dt,"DINT")) {
		return 0x04;
	} else if(0 == strcmp(dt,"USINT")) {
		return 0x05;
	} else if(0 == strcmp(dt,"UINT")) {
		return 0x06;
	} else if(0 == strcmp(dt,"UDINT")) {
		return 0x07;
	} else if(0 == strcmp(dt,"REAL")) {
		return 0x08;
	}
	return 0x0;
}

const char* getCategoryString(const uint16_t category) {
	switch(category) {
		case EEPROMCategorySTRINGS: return "STRINGS";
		case EEPROMCategoryDataTypes: return "DataTypes";
		case EEPROMCategoryGeneral: return "General";
		case EEPROMCategoryFMMU: return "FMMU";
		case EEPROMCategorySyncM: return "SyncM";
		case EEPROMCategoryFMMUX: return "FMMUX";
		case EEPROMCategorySyncUnit: return "SyncUnit";
		case EEPROMCategoryTXPDO: return "TXPDO";
		case EEPROMCategoryRXPDO: return "RXPDO";
		case EEPROMCategoryDC: return "Distributed Clock/DC";
		case EEPROMCategoryTimeouts: return "Timeouts";
		case EEPROMCategoryDictionary: return "Dictionary";
		case EEPROMCategoryHardware: return "Hardware";
		case EEPROMCategoryVendorInformation: return "Vendor Information";
		case EEPROMCategoryImages: return "Images";
		default: return "UNKNOWN";
	}
	return "NULL";
};

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

bool verbose = false;
bool writeobjectdict = false;
bool nosii = false;
std::string objectdictfile = "objectlist.c";

uint8_t* sii_eeprom = NULL;

uint32_t vendor_id = 0x0;
const char* vendor_name = NULL;

struct Group {
	const char* name = NULL;
	const char* type = NULL;
};

struct DcOpmode {
	const char* name = NULL;
	const char* desc = NULL;
	uint16_t assignactivate = 0x0;
	uint32_t cycletimesync0 = 0;
	uint32_t cycletimesync1 = 0;
	uint32_t shifttimesync0 = 0;
	uint32_t shifttimesync1 = 0;
	int16_t cycletimesync0factor = 0;
	int16_t cycletimesync1factor = 0;
};

struct DistributedClock {
	std::list<DcOpmode*> opmodes;
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
	bool aoe = false;
	bool eoe = false;
	bool coe = false;
	bool foe = false;
	bool soe = false;
	bool voe = false;

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
	int syncmanager = 0;
	int syncunit = 0;
	const char* index = NULL;
	const char* name = NULL;
	std::list<PdoEntry*> entries;
};

struct ChannelInfo {
	uint32_t profileNo = 0;
};

struct ObjectAccess {
	const char* access = NULL;
	const char* readrestrictions = NULL;
	const char* writerestrictions = NULL;
};

struct ObjectFlags {
	const char* category = NULL;
	ObjectAccess* access = NULL;
	const char* pdomapping = NULL;
	const char* sdoaccess = NULL;
};

struct ArrayInfo {
	uint8_t lowerbound = 0;
	uint8_t elements = 0;
};

struct DataType {
	const char* name = NULL;
	const char* type = NULL;
	uint32_t bitsize = 0;
	uint32_t bitoffset = 0;
	const char* basetype = NULL;
	uint8_t subindex = 0;
	ArrayInfo* arrayinfo = NULL;
	std::list<DataType*> subitems;
	ObjectFlags* flags = NULL;
};

struct Object {
	const char* index = NULL;
	const char* name = NULL;
	const char* type = NULL;
	DataType* datatype = NULL;
	uint32_t bitsize = 0;
	uint32_t bitoffset = 0;
	const char* defaultdata = NULL;
	ObjectFlags* flags = NULL;
	std::list<Object*> subitems;
};

struct Dictionary {
	std::list<DataType*> datatypes;
	std::list<Object*> objects;
};

struct Profile {
	ChannelInfo* channelinfo;
	Dictionary* dictionary;
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
	Mailbox* mailbox = NULL;
	DistributedClock* dc = NULL;
	uint32_t eepromsize = 0x0;
	uint8_t configdata[EC_SII_CONFIGDATA_SIZEB];
	std::list<Pdo*> txpdo;
	std::list<Pdo*> rxpdo;
	Profile* profile;
};

std::list<Group*> groups;
std::list<Device*> devices;

void printUsage(const char* name) {
	printf("Usage: %s [options] --input/-i <input-file>\n",name);
	printf("Options:\n");
	printf("\t --decode : Decode and print a binary SII file\n");
	printf("\t --verbose/-v : Flood some more information to stdout when applicable\n");
	printf("\t --nosii/-n : Don't generate SII EEPROM binary (only for !--decode)\n");
	printf("\t --dictionary/-d : Generate object dictionary (default if --nosii and !--decode)\n");
	printf("\n");
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

std::string CNameify(const char* str) {
	std::string r(str);
	for(uint8_t i = 0; i < r.size(); ++i) {
		r[i] = std::tolower(r[i]);
		if(r[i] == ' ') r[i] = '_';
	}
	return r;
};

void printObject (Object* o) {
	printf("Obj: Index: 0x%.04X, Name: '%s'\n",(NULL != o->index ? EC_SII_HexToUint32(o->index) : 0),o->name);
	for(Object* si : o->subitems) printObject(si);
};

void printDataType (DataType* dt) {
	printf("DataType: Name: '%s', Type: '%s'\n",dt->name,dt->type);
	for(DataType* dsi : dt->subitems) printDataType(dsi);
};

void printDataTypeVerbose (DataType* dt) {
	printf("DataType:\n");
	printf("Name: '%s'\n",dt->name);
	printf("Type: '%s'\n",dt->type);
	printf("BaseType: '%s'\n",dt->basetype);
	printf("BitSize: '%d'\n",dt->bitsize);
	printf("BitOffset: '%d'\n",dt->bitoffset);
	printf("SubIndex: '%d'\n",dt->subindex);
	printf("SubItems: '%lu'\n",dt->subitems.size());
	printf("ArrayInfo: '%s'\n",dt->arrayinfo ? "yes" : "no");
	if(dt->arrayinfo) {
		printf("Elements: '%d'\n",dt->arrayinfo->elements);
		printf("LowerBound: '%d'\n",dt->arrayinfo->lowerbound);
	}
	printf("Flags: '%s'\n",dt->flags ? "yes" : "none");
	if(dt->flags) {
		if(dt->flags->category) printf("Category: '%s'\n",dt->flags->category);
		if(dt->flags->pdomapping) printf("PdOMapping: '%s'\n",dt->flags->pdomapping);
		if(dt->flags->access) {
			if(dt->flags->access->access) printf("Access: '%s'\n",dt->flags->access->access);
		}
	}
	for(DataType* si : dt->subitems) {
		printf("SubItem:\n");
		printDataTypeVerbose(si);
	}
};

void parseXMLGroup(const tinyxml2::XMLElement* xmlgroup) {
	Group* group = new Group();
	for (const tinyxml2::XMLElement* child = xmlgroup->FirstChildElement();
		child != 0; child = child->NextSiblingElement())
	{
		if(0 == strcmp(child->Name(),"Name")) {
			group->name = child->GetText();
			printf("Group/Name: '%s'\n",group->name);
		} else
		if(0 == strcmp(child->Name(),"Type")) {
			group->type = child->GetText();
			printf("Group/Type: '%s'\n",group->type);
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
	dev->mailbox = mb;
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
					if(verbose) printf("Device/%s/Entry/Name: '%s'\n",xmlpdo->Name(),entry->name);
				} else
				if(0 == strcmp(entrychild->Name(),"Index")) {
					entry->index = entrychild->GetText();
					if(verbose) printf("Device/%s/Entry/Index: '%s'\n",xmlpdo->Name(),entry->index);
				} else
				if(0 == strcmp(entrychild->Name(),"BitLen")) {
					entry->bitlen = entrychild->IntText();
					if(verbose) printf("Device/%s/Entry/BitLen: %d\n",xmlpdo->Name(),entry->bitlen);
				} else
				if(0 == strcmp(entrychild->Name(),"SubIndex")) {
					entry->subindex = entrychild->IntText(); // TODO: HexDec
					if(verbose) printf("Device/%s/Entry/SubIndex: '%d'\n",xmlpdo->Name(),entry->subindex);
				} else
				if(0 == strcmp(entrychild->Name(),"DataType")) {
					entry->datatype = entrychild->GetText();
					if(verbose) printf("Device/%s/Entry/DataType: '%s'\n",xmlpdo->Name(),entry->datatype);
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

void parseXMLDistributedClock(const tinyxml2::XMLElement* xmldc, DistributedClock* dc) {
	for (const tinyxml2::XMLElement* dcchild = xmldc->FirstChildElement();
		dcchild != 0; dcchild = dcchild->NextSiblingElement())
	{
		if(0 == strcmp(dcchild->Name(),"OpMode")) {
			DcOpmode* opmode = new DcOpmode();
			for (const tinyxml2::XMLElement* dcopmodechild = dcchild->FirstChildElement();
				dcopmodechild != 0; dcopmodechild = dcopmodechild->NextSiblingElement())
			{
				if(0 == strcmp(dcopmodechild->Name(),"Name")) {
					opmode->name = dcopmodechild->GetText();
					printf("Device/Dc/Opmode/Name: %s\n",opmode->name);
				} else
				if(0 == strcmp(dcopmodechild->Name(),"Desc")) {
					opmode->desc = dcopmodechild->GetText();
					printf("Device/Dc/Opmode/Desc: %s\n",opmode->desc);
				} else
				if(0 == strcmp(dcopmodechild->Name(),"CycleTimeSync0")) {
					opmode->cycletimesync0 = dcopmodechild->UnsignedText();
					if(verbose) printf("Device/Dc/Opmode/CycleTimeSync0: %u\n",opmode->cycletimesync0);
					for (const tinyxml2::XMLAttribute* cts0attr = dcopmodechild->FirstAttribute();
						cts0attr != 0; cts0attr = cts0attr->Next())
					{
						if(0 == strcmp(cts0attr->Name(),"Factor")) {
							opmode->cycletimesync0factor = cts0attr->IntValue();
						} else
						{
							printf("Unhandled Device/Dc/Opmode/CycleTimeSync0 attribute: '%s' = '%s'\n",cts0attr->Name(),cts0attr->Value());
						}
					}
				} else
				if(0 == strcmp(dcopmodechild->Name(),"CycleTimeSync1")) {
					opmode->cycletimesync1 = dcopmodechild->UnsignedText();
					if(verbose) printf("Device/Dc/Opmode/CycleTimeSync1: %u\n",opmode->cycletimesync1);
					for (const tinyxml2::XMLAttribute* cts1attr = dcopmodechild->FirstAttribute();
						cts1attr != 0; cts1attr = cts1attr->Next())
					{
						if(0 == strcmp(cts1attr->Name(),"Factor")) {
							opmode->cycletimesync1factor = cts1attr->IntValue();
						} else
						{
							printf("Unhandled Device/Dc/Opmode/CycleTimeSync1 attribute: '%s' = '%s'\n",cts1attr->Name(),cts1attr->Value());
						}
					}
				} else
				if(0 == strcmp(dcopmodechild->Name(),"ShiftTimeSync0")) {
					opmode->shifttimesync0 = dcopmodechild->UnsignedText();
					if(verbose) printf("Device/Dc/Opmode/ShiftTimeSync0: %u\n",opmode->shifttimesync0);
					for (const tinyxml2::XMLAttribute* sts0attr = dcopmodechild->FirstAttribute();
						sts0attr != 0; sts0attr = sts0attr->Next())
					{
						printf("Unhandled Device/Dc/Opmode/ShiftTimeSync0 attribute: '%s' = '%s'\n",sts0attr->Name(),sts0attr->Value());
					}
				} else
				if(0 == strcmp(dcopmodechild->Name(),"ShiftTimeSync1")) {
					opmode->shifttimesync1 = dcopmodechild->UnsignedText();
					if(verbose) printf("Device/Dc/Opmode/ShiftTimeSync1: %u\n",opmode->shifttimesync1);
					for (const tinyxml2::XMLAttribute* sts1attr = dcopmodechild->FirstAttribute();
						sts1attr != 0; sts1attr = sts1attr->Next())
					{
						printf("Unhandled Device/Dc/Opmode/ShiftTimeSync1 attribute: '%s' = '%s'\n",sts1attr->Name(),sts1attr->Value());
					}
				} else
				if(0 == strcmp(dcopmodechild->Name(),"AssignActivate")) { // HexDecInt
					opmode->assignactivate = (EC_SII_HexToUint32(dcopmodechild->GetText()) & 0xFFFF);
					printf("Device/Dc/Opmode/AssignActivate: 0x%.04X\n",opmode->assignactivate);
				}
				else {
					printf("Unhandled Device/Dc/Opmode element: '%s' = '%s'\n",dcopmodechild->Name(),dcopmodechild->GetText());
				}
			}
			dc->opmodes.push_back(opmode);
		} else
		{
			printf("Unhandled Device/Dc element: '%s' = '%s'\n",dcchild->Name(),dcchild->GetText());
		}
	}
}

void parseXMLObject(const tinyxml2::XMLElement* xmlobject, Dictionary* dict, Object* parent = NULL) {
	Object* obj = new Object;
	for (const tinyxml2::XMLElement* objchild = xmlobject->FirstChildElement();
		objchild != 0; objchild = objchild->NextSiblingElement())
	{
		if(0 == strcmp(objchild->Name(),"Index")) {
			obj->index = objchild->GetText();
		} else
		if(0 == strcmp(objchild->Name(),"Name")) {
			obj->name = objchild->GetText();
		} else
		if(0 == strcmp(objchild->Name(),"Type")) {
			obj->type = objchild->GetText();
		} else
		if(0 == strcmp(objchild->Name(),"BitSize")) {
			obj->bitsize = objchild->IntText();
		} else
		if(0 == strcmp(objchild->Name(),"BitOffs")) {
			obj->bitoffset = objchild->IntText();
		} else
		if(0 == strcmp(objchild->Name(),"Info")) {
			for (const tinyxml2::XMLElement* infochild = objchild->FirstChildElement();
				infochild != 0; infochild = infochild->NextSiblingElement())
			{
				if(0 == strcmp(infochild->Name(),"DefaultData")) {
					obj->defaultdata = infochild->GetText();
				} else
				if(0 == strcmp(infochild->Name(),"SubItem")) {
					parseXMLObject(infochild,dict,obj);
				} else
				{
					printf("Unhandled Device/Profile/Objects/Object/Info element: '%s' = '%s'\n",objchild->Name(),objchild->GetText());
				}
			}
		} else
		if(0 == strcmp(objchild->Name(),"Flags")) {
			ObjectFlags* flags = new ObjectFlags;
			for (const tinyxml2::XMLElement* flagschild = objchild->FirstChildElement();
				flagschild != 0; flagschild = flagschild->NextSiblingElement())
			{
				if(0 == strcmp(flagschild->Name(),"Access")) {
					ObjectAccess* access = new ObjectAccess;
					access->access = flagschild->GetText();
					for (const tinyxml2::XMLAttribute* attr = flagschild->FirstAttribute();
						attr != 0; attr = attr->Next())
					{
						if(0 == strcmp(attr->Name(),"ReadRestrictions")) {
							access->readrestrictions = attr->Value();
						} else
						if(0 == strcmp(attr->Name(),"WriteRestrictions")) {
							access->writerestrictions = attr->Value();
						} else
						{
							printf("Unhandled Device/Profile/Objects/Object/Flags/Access Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
						}
					}
					flags->access = access;
				} else
				if(0 == strcmp(flagschild->Name(),"Category")) {
					flags->category = flagschild->GetText();
				} else
				if(0 == strcmp(flagschild->Name(),"PdoMapping")) {
					flags->pdomapping = flagschild->GetText();
				} else
				if(0 == strcmp(flagschild->Name(),"SdoAccess")) {
					flags->sdoaccess = flagschild->GetText();
				} else
				{
					printf("Unhandled Device/Profile/Objects/Object/Flags element: '%s' = '%s'\n",objchild->Name(),objchild->GetText());
				}

			}
			obj->flags = flags;
		} else
		if(0 == strcmp(objchild->Name(),"SubItem")) {
			parseXMLObject(objchild,dict,obj);
		} else
		{
			printf("Unhandled Device/Profile/Objects/Object element: '%s' = '%s'\n",objchild->Name(),objchild->GetText());
		}
	}
	if(NULL != parent) {
		if(NULL == obj->index) obj->index = parent->index;
		if(NULL == obj->type) obj->type = parent->type;
		if(NULL == obj->flags) obj->flags = parent->flags;
		parent->subitems.push_back(obj);
	} else dict->objects.push_back(obj);

	for(DataType* dt : dict->datatypes) {
		if(0 == strcmp(obj->type,dt->name)) {
			obj->datatype = dt;
			if(parent) {
				int subitemno = parent->subitems.size()-1;
				if(parent != NULL && !dt->subitems.empty() &&
					dt->subitems.size() > subitemno)
				{
					auto dtptr = dt->subitems.begin();
					std::advance(dtptr,subitemno);
					obj->datatype = (*dtptr);
					break;
				}
			}
			break;
		}
	}
}

void parseXMLDataType(const tinyxml2::XMLElement* xmldatatype, Dictionary* dict = NULL, DataType* parent = NULL) {
	DataType* datatype = new DataType;
	for (const tinyxml2::XMLElement* dtchild = xmldatatype->FirstChildElement();
		dtchild != 0; dtchild = dtchild->NextSiblingElement())
	{
		if(0 == strcmp(dtchild->Name(),"Name")) {
			datatype->name = dtchild->GetText();
		} else
		if(0 == strcmp(dtchild->Name(),"Type")) {
			datatype->type = dtchild->GetText();
		} else
		if(0 == strcmp(dtchild->Name(),"SubIdx")) {
			datatype->subindex = dtchild->IntText();
		} else
		if(0 == strcmp(dtchild->Name(),"BitSize")) {
			datatype->bitsize = dtchild->IntText();
		} else
		if(0 == strcmp(dtchild->Name(),"BitOffs")) {
			datatype->bitoffset = dtchild->IntText();
		} else
		if(0 == strcmp(dtchild->Name(),"BaseType")) {
			datatype->basetype = dtchild->GetText();
		} else
		if(0 == strcmp(dtchild->Name(),"ArrayInfo")) {
			ArrayInfo* arrinfo = new ArrayInfo;
			for (const tinyxml2::XMLElement* arrchild = dtchild->FirstChildElement();
				arrchild != 0; arrchild = arrchild->NextSiblingElement())
			{
				if(0 == strcmp(arrchild->Name(),"LBound")) {
					arrinfo->lowerbound = arrchild->IntText();
				} else
				if(0 == strcmp(arrchild->Name(),"Elements")) {
					arrinfo->elements = arrchild->IntText();
				} else
				{
					printf("Unhandled Device/Profile/DataTypes/DataType/ArrayInfo element: '%s' = '%s'\n",arrchild->Name(),arrchild->GetText());
				}
			}
			datatype->arrayinfo = arrinfo;
		} else
		if(0 == strcmp(dtchild->Name(),"Flags")) {
			ObjectFlags* flags = new ObjectFlags;
			for (const tinyxml2::XMLElement* flagschild = dtchild->FirstChildElement();
				flagschild != 0; flagschild = flagschild->NextSiblingElement())
			{
				if(0 == strcmp(flagschild->Name(),"Access")) {
					ObjectAccess* access = new ObjectAccess;
					access->access = flagschild->GetText();
					for (const tinyxml2::XMLAttribute* attr = flagschild->FirstAttribute();
						attr != 0; attr = attr->Next())
					{
						if(0 == strcmp(attr->Name(),"ReadRestrictions")) {
							access->readrestrictions = attr->Value();
						} else
						if(0 == strcmp(attr->Name(),"WriteRestrictions")) {
							access->writerestrictions = attr->Value();
						} else
						{
							printf("Unhandled Device/Profile/DataTypes/DataType/Flags/Access Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
						}
					}
					flags->access = access;
				} else
				if(0 == strcmp(flagschild->Name(),"Category")) {
					flags->category = flagschild->GetText();
				} else
				if(0 == strcmp(flagschild->Name(),"PdoMapping")) {
					flags->pdomapping = flagschild->GetText();
				} else
				{
					printf("Unhandled Device/Profile/DataTypes/DataType/Flags element: '%s' = '%s'\n",dtchild->Name(),dtchild->GetText());
				}

			}
			datatype->flags = flags;
		} else
		if(0 == strcmp(dtchild->Name(),"SubItem")) {
			parseXMLDataType(dtchild,dict,datatype);
		} else
		{
			printf("Unhandled Device/Profile/DataTypes/Object element: '%s' = '%s'\n",dtchild->Name(),dtchild->GetText());
		}
	}
	// TODO: Improve the var/record/array stuff...
	if(NULL != parent) {
		if(NULL != datatype->type) {
			for(DataType* dt : dict->datatypes) {
				if(0 == strcmp(datatype->type,dt->name)) {
					if(dt->arrayinfo) {
						datatype->type = dt->basetype;
						parent->type = dt->basetype;
						parent->arrayinfo = dt->arrayinfo;
					}
				}
			}
		}
		if(NULL == datatype->type) datatype->type = parent->type;
		if(NULL == datatype->flags) datatype->flags = parent->flags;
		parent->subitems.push_back(datatype);
	} else {
		dict->datatypes.push_back(datatype);
	}
}

void parseXMLProfile(const tinyxml2::XMLElement* xmlprofile, Device *dev) {
	Profile* profile = new Profile;
	dev->profile = profile;
	for (const tinyxml2::XMLElement* child = xmlprofile->FirstChildElement();
		child != 0; child = child->NextSiblingElement())
	{
		if(0 == strcmp(child->Name(),"Dictionary")) {
			Dictionary* dict = new Dictionary;
			profile->dictionary = dict;
			for (const tinyxml2::XMLElement* dictchild = child->FirstChildElement();
				dictchild != 0; dictchild = dictchild->NextSiblingElement())
			{
				if(0 == strcmp(dictchild->Name(),"Objects")) {
					for (const tinyxml2::XMLElement* objschild = dictchild->FirstChildElement();
						objschild != 0; objschild = objschild->NextSiblingElement())
					{
						if(0 == strcmp(objschild->Name(),"Object")) {
							parseXMLObject(objschild,dict);
						} else
						{
							printf("Unhandled Device/Profile/Dictionary/Objects element: '%s' = '%s'\n",objschild->Name(),objschild->GetText());
						}
					}
				} else
				if(0 == strcmp(dictchild->Name(),"DataTypes")) {
					for (const tinyxml2::XMLElement* dtchild = dictchild->FirstChildElement();
						dtchild != 0; dtchild = dtchild->NextSiblingElement())
					{
						if(0 == strcmp(dtchild->Name(),"DataType")) {
							parseXMLDataType(dtchild,dict);
						} else
						{
							printf("Unhandled Device/Profile/Dictionary/DataTypes element: '%s' = '%s'\n",dtchild->Name(),dtchild->GetText());
						}
					}
				} else
				{
					printf("Unhandled Device/Profile/Dictionary element: '%s' = '%s'\n",dictchild->Name(),dictchild->GetText());
				}
			}
		} else
		{
			printf("Unhandled Device/Profile element: '%s' = '%s'\n",child->Name(),child->GetText());
		}
	}
}

void parseXMLDevice(const tinyxml2::XMLElement* xmldevice) {
	Device* dev = new Device;
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
			printf("Device/Name: '%s'\n",dev->name);
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
					printf("Device/Eeprom/ByteSize: %u\n",dev->eepromsize);
					if(EC_SII_EEPROM_SIZE < dev->eepromsize)
						EC_SII_EEPROM_SIZE = dev->eepromsize;
				} else
				{
					printf("Unhandled Device/Eeprom element: '%s' = '%s'\n",eepchild->Name(),eepchild->GetText());
				}
			}
		} else
		if(0 == strcmp(child->Name(),"Dc")) {
			DistributedClock* dc = new DistributedClock();
			parseXMLDistributedClock(child,dc);
			dev->dc = dc;
		} else
		if(0 == strcmp(child->Name(),"Sm")) {
			SyncManager* sm = new SyncManager();
			sm->type = child->GetText();
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"DefaultSize")) {
					sm->defaultsize = attr->UnsignedValue();
					if(0 == sm->defaultsize) sm->defaultsize = EC_SII_HexToUint32(attr->Value());
					if(0 == sm->defaultsize) printf("Failed to decipher SyncManager DefaultSize\n");
					if(verbose) printf("Device/Sm/@DefaultSize: 0x%.04X\n",sm->defaultsize);
				} else
				if(0 == strcmp(attr->Name(),"Enable")) { // hexdecvalue
					sm->enable = attr->UnsignedValue() > 0 ? true : false;
					printf("Device/Sm/@Enable: '%s'\n",sm->enable ? "yes" : "no");
				} else
				if(0 == strcmp(attr->Name(),"ControlByte")) { // hexdecvalue
					sm->controlbyte = (EC_SII_HexToUint32(attr->Value()) & 0xFF);
					if(verbose) printf("Device/Sm/@ControlByte: 0x%.02X\n",sm->controlbyte);
				} else
				if(0 == strcmp(attr->Name(),"StartAddress")) {
					sm->startaddress = EC_SII_HexToUint32(attr->Value()) & 0xFFFF;
					printf("Device/Sm/@StartAddress: 0x%.04X\n",sm->startaddress);
				} else
				if(0 == strcmp(attr->Name(),"MinSize")) {
					sm->minsize = EC_SII_HexToUint32(attr->Value()) & 0xFFFF;
					if(verbose) printf("Device/Sm/@MinSize: 0x%.04X\n",sm->minsize);
				} else
				if(0 == strcmp(attr->Name(),"MaxSize")) {
					sm->maxsize = EC_SII_HexToUint32(attr->Value()) & 0xFFFF;
					if(verbose) printf("Device/Sm/@MaxSize: 0x%.04X\n",sm->maxsize);
				} else
				{
					printf("Unhandled Device/Sm Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
				}
			}
			dev->syncmanagers.push_back(sm);
		} else
		if(0 == strcmp (child->Name(),"Profile")) {
			parseXMLProfile(child,dev);
		} else
		if(0 == strcmp (child->Name(),"TxPdo")) {
			parseXMLPdo(child,&(dev->txpdo));
		} else
		if(0 == strcmp (child->Name(),"RxPdo")) {
			parseXMLPdo(child,&(dev->rxpdo));
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
	if("" == output) output = file + "_eeprom.bin";
	tinyxml2::XMLDocument doc;
	if(tinyxml2::XML_SUCCESS != doc.LoadFile( file.c_str() )) {
		printf("Could not open '%s'\n",file.c_str());
		return -EINVAL;
	}
	const tinyxml2::XMLElement* root = doc.RootElement();
	if(NULL != root) {
		if(0 != strcmp(ESI_ROOTNODE_NAME,root->Name())) {
			printf("Document seemingly does not contain EtherCAT information (root node name is not '%s' but '%s')\n",ESI_ROOTNODE_NAME,root->Name());
			return -EINVAL;
		}
		parseXMLElement(root);
		// TODO check mandatory items
		// Group Name
		// Device Name
		// ...
		Device* dev = devices.front();
		if(!devices.empty()) {
			printf("Profile: %s\n",dev->profile ? "yes" : "no");

			if(NULL != dev->profile) {
				printf("Dictionary: %s\n",dev->profile->dictionary ? "yes" : "no");
				if(NULL != dev->profile->dictionary) {
					printf("Objects: %lu\n",dev->profile->dictionary->objects.size());
					for(Object* o : dev->profile->dictionary->objects) {
						//printObject(o);
					}
					printf("DataTypes: %lu\n",dev->profile->dictionary->datatypes.size());
					for(DataType* dt : dev->profile->dictionary->datatypes) {
						//printDataTypeVerbose(dt);
					}
				}
			}
			// Write SII EEPROM file
			if(!nosii) {
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

					// EEPROM Size 0x003E TODO

					// EEPROM Version 0x003E (currently 1)
					p = sii_eeprom + EC_SII_EEPROM_VERSION_OFFSET_BYTE;
					*(p++) = 0x1;
					*(p++) = 0x0;

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
					//for(; stringcatpad > 0; --stringcatpad) ++p;

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

					// TXPDO category if needed
					if(!dev->txpdo.empty())
					{
						for(Pdo* pdo : dev->txpdo) {
							*(p++) = EEPROMCategoryTXPDO & 0xFF;
							*(p++) = (EEPROMCategoryTXPDO >> 8) & 0xFF;
							// Each PDO entry takes 8 bytes, and a "header" of 8 bytes
							uint16_t txpdocatlen = ((pdo->entries.size() * 0x8) + 8) >> 1;
							*(p++) = txpdocatlen & 0xFF;
							*(p++) = (txpdocatlen >> 8) & 0xFF;

							uint16_t index = EC_SII_HexToUint32(pdo->index) & 0xFFFF; // HexDec
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
								index = EC_SII_HexToUint32(entry->index) & 0xFFFF;
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
					if(!dev->rxpdo.empty())
					{
						for(Pdo* pdo : dev->rxpdo) {
							*(p++) = EEPROMCategoryRXPDO & 0xFF;
							*(p++) = (EEPROMCategoryRXPDO >> 8) & 0xFF;
							// Each PDO entry takes 8 bytes, and a "header" of 8 bytes
							uint16_t rxpdocatlen = ((pdo->entries.size() * 0x8) + 8) >> 1;
							*(p++) = rxpdocatlen & 0xFF;
							*(p++) = (rxpdocatlen >> 8) & 0xFF;

							uint16_t index = EC_SII_HexToUint32(pdo->index) & 0xFFFF; // HexDec
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
								index = EC_SII_HexToUint32(entry->index) & 0xFFFF;
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

			// Write slave stack object dictionary
			if(writeobjectdict && NULL != dev->profile->dictionary) {
				std::ofstream typesout;
				typesout.open("utypes.h", std::ios::out | std::ios::trunc);
				if(!typesout.fail()) {
					typesout << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n\n";
					typesout << "#ifndef UTYPES_H\n";
					typesout << "#define UTYPES_H\n\n";
					typesout << "#include <stdint.h>\n";
					typesout << "\n";
					for(Object* o : dev->profile->dictionary->objects) {
						uint16_t index = EC_SII_HexToUint32(o->index);
						if(index < 0x2000) continue;
						typesout << "typedef struct {\n";
						int subitem = 0;
						for(Object* si : o->subitems) {
							if(0 == subitem) {
								++subitem;
								continue;
							}

							const char* type = si->datatype ? 
								(si->datatype->type ? si->datatype->type :
									si->datatype->name) :
								o->type;
							if(NULL == type) {
								printf("WARNING: Could not determine C-datatype for '%s':'%s'\n",o->name,si->name);
								continue;
							}
							typesout << "\t";
							if(0 == strncmp(type,"BOOL",4)) {
								typesout << "bool";
							} else
							if(0 == strcmp(type,"SINT")) {
								typesout << "int8_t";
							} else
							if(0 == strcmp(type,"INT")) {
								typesout << "int16_t";
							} else
							if(0 == strcmp(type,"DINT")) {
								typesout << "int32_t";
							} else
							if(0 == strcmp(type,"USINT")) {
								typesout << "uint8_t";
							} else
							if(0 == strcmp(type,"UINT")) {
								typesout << "uint16_t";
							} else
							if(0 == strcmp(type,"UDINT")) {
								typesout << "uint32_t";
							}
							typesout << " ";
							typesout << CNameify(si->name);
							typesout << ";";
							typesout << " /* ";
							typesout << si->index << ".";
							typesout << std::uppercase
								 << std::hex
								 << std::setfill('0')
								 << std::setw(2)
								 << subitem;
							typesout << "*/\n";
							++subitem;
						}
						typesout << "} " << o->name << ";\n\n";
					}
					typesout << "#endif /* UTYPES_H */\n";
					typesout.sync_with_stdio();
					typesout.close();
				}


				std::ofstream out;
				out.open(objectdictfile.c_str(), std::ios::out | std::ios::trunc);
				if(!out.fail()) {
					printf("Writing SOES compatible object dictionary to '%s'\n",objectdictfile.c_str());
					out << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n"
					<< "#include \"esc_coe.h\"\n"
					<< "#include \"utypes.h\"\n"
					<< "#include <stddef.h>\n"
					<< "\n";

					// Generate string objects
					for(Object* o : dev->profile->dictionary->objects) {
						out << "static const char acName"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << EC_SII_HexToUint32(o->index);
						out << "[] = \"" << o->name << "\";\n";
						// TODO handle several levels?
						int subitem = 0;
						for(Object* si : o->subitems) {
							out << "static const char acName"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << EC_SII_HexToUint32(si->index);
							out << "_";
							out << std::setfill('0')
							    << std::setw(2)
							    << subitem;
							if(subitem == 0) {
								out << "[] = \"Max SubIndex\";\n";
							} else {
								out << "[] = \"" << si->name << "\";\n";
							}
							++subitem;
						}
						if(NULL != o->type && 0 == strncmp("STRING",o->type,5)) {
							out << "static char acValue"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << EC_SII_HexToUint32(o->index);
							out << "_00[] = \"";
							if(NULL != o->defaultdata) {
								out << o->defaultdata;
							} else {
								out << "(null)";
							}
							out << "\";\n";
						}
					}
					out << "\n";

					auto writeObject = [&out](Object* obj, int& subitem, const int nitems, Dictionary* dict = NULL) {
						bool objref = false;
						out << "{ 0x"
						    << std::setw(2)
						    << subitem
						    << ", ";

						DataType* datatype = obj->datatype;
						const char* type = datatype ? (datatype->type ? datatype->type : datatype->name) : obj->type;
						const ObjectFlags* flags = obj->flags ? obj->flags : (datatype ? datatype->flags : NULL);
						uint32_t bitsize = obj->bitsize ? obj->bitsize : (datatype ? datatype->bitsize : 0);
						if(NULL == type) {
							printf("\033[0;31mWARNING:\033[0m DataType of object '%s' subitem '%d' is \033[0;33mNULL\033[0m\n\n\n",obj->index,subitem);
						} else
						if(0 == strncmp(type,"STRING",5)) {
							out << "DTYPE_VISIBLE_STRING" << ", ";
							out << "sizeof(";
							out << "acValue"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << EC_SII_HexToUint32(obj->index);
							out << "_00) << 3";
							objref = true;
						} else  // capitalization of all these strings?
						if(0 == strncmp(type,"BOOL",4)) {
							out << "DTYPE_BOOLEAN";
						} else
						if(0 == strcmp(type,"SINT")) {
							out << "DTYPE_INTEGER8";
						} else
						if(0 == strcmp(type,"INT")) {
							out << "DTYPE_INTEGER16";
						} else
						if(0 == strcmp(type,"DINT")) {
							out << "DTYPE_INTEGER32";
						} else
						if(0 == strcmp(type,"USINT")) {
							out << "DTYPE_UNSIGNED8";
						} else
						if(0 == strcmp(type,"UINT")) {
							out << "DTYPE_UNSIGNED16";
						} else
						if(0 == strcmp(type,"UDINT")) {
							out << "DTYPE_UNSIGNED32";
						} else
						{ // TODO handle more types
							printf("\033[0;31mWARNING:\033[0m Unhandled Datatype '%s'\n",obj->type);
						}
						out << ", ";

						out << std::dec << (bitsize == 0 ? obj->bitsize : bitsize);
						out << ", ";

						// TODO handle the preRW and whatever in read/write-restrictions
						if(NULL == flags ||
						   NULL == flags->access ||
						   0 == strcmp(flags->access->access,"ro"))
						{
							out << "ATYPE_RO";
						} else
						if(0 == strcmp(flags->access->access,"rw")) {
							out << "ATYPE_RW";
						}
						out << ", ";

						out << "&acName"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << EC_SII_HexToUint32(obj->index);
						if(nitems == 0) {
							out << "[0]";
						} else {
							out << "_"
							    << std::setw(2)
							    << subitem
							    << "[0]";
						}
						out << ", ";

						if(!objref) {
							if(NULL != obj->defaultdata) {
								out << "0x"
								    << obj->defaultdata
								    << ", NULL }";
							} else { 
								out << 0
								    << ", NULL }";
							}
						} else {
							out << "0, ";
							out << "&acValue"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << EC_SII_HexToUint32(obj->index);
							out << "_00[0] }";
						}
						++subitem;
						if(subitem < nitems) out << ",\n";
					};

					for(Object* o : dev->profile->dictionary->objects) {
						int subitem = 0;
						out << "const _objd SDO"
						    << std::hex << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << EC_SII_HexToUint32(o->index);
						out << "[] = {\n";
						if(o->subitems.empty()) {
							writeObject(o,subitem,0,dev->profile->dictionary);
						} else {
							// TODO handle several levels?
							for(Object* si : o->subitems) {
								writeObject(si,subitem,o->subitems.size(),dev->profile->dictionary);
							}
						}
						out << " };\n\n";
					}

					out << "const _objectlist SDOobjects[] = {\n";
					for(Object* o : dev->profile->dictionary->objects) {
						uint16_t index = EC_SII_HexToUint32(o->index);
						out << "{ 0x"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << index;
						out << ", ";

						if(o->subitems.empty()) {
							out << "OTYPE_VAR";
						} else {
							if(NULL != o->datatype && NULL != o->datatype->arrayinfo)
							{
								out << "OTYPE_ARRAY";
							} else {
								out << "OTYPE_RECORD";
							}
						}
						out << ", ";

						out << "0x"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(2)
						    << (o->subitems.empty() ? 0 : o->subitems.size()-1);
						out << ", ";

						out << "0x0";
						out << ", ";

						out << "&acName"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << index << "[0]";
						out << ", ";

						out << "&SDO"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << index
						    << "[0]";
						out << " },\n";
					}
					out << "{ 0xFFFF, 0xFF, 0xFF, 0xFF, NULL, NULL } };\n";
					out << "\n\n";

					out.sync_with_stdio();
					out.close();
				} else {
					printf("Could not open '%s' for writing object dictionary\n",objectdictfile.c_str());
				}
			} else if (writeobjectdict) {
				printf("No dictionary could be parsed...\n");
			}

			printf("Finished\n");
		} else {
			printf("No 'Devices' nodes could be parsed\n");
		}
	} else {
		printf("Document has no root node!\n");
	}

	return 0;
}

int decodeSII(const std::string& file) {
	int fd = open(file.c_str(), O_RDONLY);
	if(fd < 0){
		printf("Could not open '%s'\n",file.c_str());
		return 1;
	}
	printf("Decoding SII from '%s'\n",file.c_str());
	struct stat statbuf;
	int err = fstat(fd, &statbuf);
	if(err < 0){
		printf("Could not stat '%s'\n",file.c_str());
		return 2;
	}

	uint8_t *p = (uint8_t*) mmap(NULL, statbuf.st_size,
		PROT_READ, MAP_PRIVATE, fd, 0);
	uint8_t *ptr = p;
	if(ptr == MAP_FAILED){
		printf("Mapping Failed (%d)\n",errno);
		return 1;
	}

	printf("ConfigData: '\033[0;32m");
	for(uint8_t i = 0; i < EC_SII_CONFIGDATA_SIZEB; ++i) {
		printf("0x%.02X",*(ptr++));
		if(i < EC_SII_CONFIGDATA_SIZEB-1) printf(" ");
		// TODO verify checksum
	}
	printf("\033[0m' \033[0;36m[Esi:DeviceType:Eeprom:ConfigData]\033[0m\n");
	printf("Vendor: \033[0;32m0x%.08X \033[0;36m[Esi:EtherCATInfo:Vendor]\033[0m\n",*(uint32_t*)ptr);
	ptr += 4;
	printf("Product: \033[0;32m0x%.08X \033[0;36m[Esi:DeviceType:Type:ProductCode]\033[0m\n",*(uint32_t*)ptr);
	ptr += 4;
	printf("Revision: \033[0;32m0x%.08X \033[0;36m[Esi:DeviceType:Type:RevisionNo]\033[0m\n",*(uint32_t*)ptr);
	ptr += 4;

	// TODO eepromsize and version

	ptr = p + EC_SII_EEPROM_FIRST_CAT_HDR_OFFSET_BYTE; // Categories

	int categoryNo = 1;
	uint16_t* category = (uint16_t*)ptr;
	uint8_t* nextCategoryPtr = ptr;
	std::vector<uint8_t*> stringPointers;

	while(*category != 0 && *category != 0xFFFF) {
		uint16_t categorySizeW = *(uint16_t*)(ptr+2);
		printf("--------------------------\n");
		printf("Category %d: \033[0;31m%s\033[0m (%d) - %d words\n\n",categoryNo,
			getCategoryString(*category),*category,categorySizeW);
		nextCategoryPtr += (categorySizeW*2)+4; // +4 = header and category size
		ptr += 4;
		switch(*category) {
			case EEPROMCategorySTRINGS: {
				uint8_t stringcnt = *ptr++;
				printf("%d strings present\n",stringcnt);
				for(uint8_t s = 1; s <= stringcnt; ++s) {
					stringPointers.push_back(p);
					uint8_t stringlen = *ptr++;
					printf("\033[0;33mString %d\033[0m: '\033[0;32m",s);
					for(uint8_t i = 0; i < stringlen; ++i) {
						printf("%c",(const char)(*ptr++));
					}
					printf("\033[0m' (length: %d)\n",stringlen);
				}
			}
			break;
			case EEPROMCategoryGeneral: {
				printf("Group Index: \033[0;32m%d \033[0;36m[Esi:DeviceType:GroupType]\033[0m\n",*ptr++);
				printf("Image Index: \033[0;32m%d \033[0;36m[Esi:DeviceType:ImageData16x14]\033[0m\n",*ptr++);
				printf("Device Order: \033[0;32m%d \033[0;36m[Esi:DeviceType:Type]\033[0m\n",*ptr++);
				printf("Name Index: \033[0;32m%d \033[0;36m[Esi:DeviceType:Name]\033[0m\n",*ptr++); // TODO print string
				ptr++; // Reserved
				printf("\n");
				uint8_t coe = *ptr++;
				if(coe & 0x1) {
					printf("CoE supported \033[0;36m[Esi:DeviceType:Mailbox:CoE]\033[0m\n");
					if(coe & 0x2)	printf("- Enable SDO Info \033[0;36m[Esi:DeviceType:Mailbox:CoE:SDOInfo]\033[0m\n");
					if(coe & 0x4)	printf("- Enable PDO Assign \033[0;36m[Esi:DeviceType:Mailbox:CoE:PdoAssign]\033[0m\n");
					if(coe & 0x8)	printf("- Enable PDO Configuration \033[0;36m[Esi:DeviceType:Mailbox:CoE:PdoConfig]\033[0m\n");
					if(coe & 0x10)	printf("- Enable PDO Upload \033[0;36m[Esi:DeviceType:Mailbox:CoE:PdoUload]\033[0m\n");
					if(coe & 0x20)	printf("- Enable SDO Complete Access \033[0;36m[Esi:DeviceType:Mailbox:CoE:CompleteAccess]\033[0m\n");
				}
				uint8_t foe = *ptr++;
				if(foe & 0x1) {
					printf("FoE supported \033[0;36m[Esi:DeviceType:Mailbox:FoE]\033[0m\n");
				}
				uint8_t eoe = *ptr++;
				if(eoe & 0x1) {
					printf("EoE supported \033[0;36m[Esi:DeviceType:Mailbox:EoE]\033[0m\n");
				}
				ptr++; // Reserved (SoEChannels)
				ptr++; // Reserved (DS402Channels)
				ptr++; // Reserved (SysmanClass)
				uint8_t flags = *ptr++;
				if(flags) {
					printf("Flags:\n");
					if(flags & 0x1) printf("- Enable SafeOp \033[0;36m[Esi:Info:StateMachine:Behavior:StartToSafeopNoSync]\033[0m\n");
					if(flags & 0x2) printf("- Enable notLRW \033[0;36m[Esi:DeviceType:Type]\033[0m\n");
					if(flags & 0x4) printf("- MboxDataLinkLayer \033[0;36m[Esi:DeviceType:Mailbox:DataLinkLayer]\033[0m\n");
					if(flags & 0x8) printf("- ID selector mirrored in AL Status Code \033[0;36m[ESI:Info:IdentificationReg134]\033[0m\n");
					if(flags & 0x10) printf("- ID selector value mirrored in specific physical memory as denoted by the parameter \"Physical Memory Address\" \033[0;36m[Esi:Info:IdentificationAdo]\033[0m\n");
				}
				uint16_t ebuscurrent = *ptr++;
				ebuscurrent += (*ptr++) << 8;
				printf("\n");
				printf("Ebus current: \033[0;32m%d mA\033[0m \033[0;36m[Esi:Info:Electrical:EBusCurrent]\033[0m\n",ebuscurrent);
				printf("Group index (compatibility duplicate): \033[0;32m0x%.02X\033[0m\n",*ptr++);
				ptr++; // Reserved
				printf("\n");
				uint16_t physicalport = *ptr++;
				physicalport += (*ptr++) << 8;
				printf("Physical port configuration: \033[0;36m[Esi:Device@PhysicsType]\033[0m\n");
				for(int port : {0, 1, 2, 3}) {
					printf("\033[0;33mPort %d\033[0m: \033[0;32m",port);
					switch((physicalport >> (port*4)) & 0xF) {
						case 0x0:
							printf("Not in use");
						break;
						case 0x1:
							printf("MII");
						break;
						case 0x2:
							printf("Reserved");
						break;
						case 0x3:
							printf("EBUS");
						break;
						case 0x4:
							printf("Fast Hot Connect");
						break;
					}
					printf("\033[0m\n");
				}
			}
			break;
			case EEPROMCategoryFMMU: {
				uint16_t fmmucnt = categorySizeW*2;
				printf("%d FMMUs described \033[0;36m[Esi:DeviceType:Fmmu]\033[0m\n\n",fmmucnt);
				for(uint16_t fmmu = 0; fmmu < fmmucnt; ++fmmu) {
					uint8_t fmmuconfig = *(ptr++);
					printf("\033[0;33mFMMU%d\033[0m: \033[0;32m",fmmu);
					switch(fmmuconfig) {
						case 0x0: printf("Not used"); break;
						case 0x1: printf("Used for Outputs"); break;
						case 0x2: printf("Used for Inputs"); break;
						case 0x3: printf("Used for SyncM (Read Mailbox)"); break;
						case 0xFF: printf("Not used"); break;
					}
					printf("\033[0m\n");
				}
			}
			break;
			case EEPROMCategorySyncM: {
				uint16_t smcnt = (categorySizeW*2)/8;
				printf("%d SyncManagers described\n\n",smcnt);
				for(uint16_t sm; sm < smcnt; ++sm) {
					printf("\033[0;33mSyncManager %d\033[0m:\n",sm);
					printf("Physical Start Address: \033[0;32m0x%.04X \033[0;36m[Esi:DeviceType:Sm:StartAddress]\033[0m\n",*(uint16_t*)(ptr));
					ptr += 2;
					printf("Length: \033[0;32m0x%.04X \033[0;36m[Esi:DeviceType:Sm:DefaultSize]\033[0m\n",*(uint16_t*)(ptr));
					ptr += 2;
					printf("Control Register: \033[0;32m0x%.02X \033[0;36m[Esi:DeviceType:Sm:ControlByte]\033[0m\n",*(ptr++));
					printf("Status Register: \033[0;32m0x%.02X \033[0;36m[dont care]\033[0m\n",*(ptr++));
					uint8_t enable = *(ptr++);
					printf("Enable state: \033[0;36m[Esi:DeviceType:Sm:Enable]\033[0m\n");
					if(enable & 0x1) printf("- is enabled\n");
					if(enable & 0x2) printf("- has fixed content\n");
					if(enable & 0x4) printf("- is virtual\n");
					if(enable & 0x8) printf("- is only enabled in Operation state\n");
					uint8_t type = *(ptr++);
					printf("Type: \033[0;32m");
					switch(type) {
						default:
						case 0x0:
							printf("Unused/Unknown");
						break;
						case 0x1:
							printf("Mailbox Out");
						break;
						case 0x2:
							printf("Mailbox In");
						break;
						case 0x3:
							printf("Process data outputs");
						break;
						case 0x4:
							printf("Process data inputs");
						break;
					}
					printf(" \033[0;36m[Esi:DeviceType:Sm]\033[0m\n");
					if(sm < smcnt-1)printf("\n");
				}
			}
			break;
			case EEPROMCategoryDC: {
				uint16_t dccnt = (categorySizeW*2)/24;
				printf("%d clock cycles described\n\n",dccnt);
				for(uint16_t dc = 0; dc < dccnt; ++dc) {
					printf("\033[0;33mDC %d\033[0m:\n",dc);
					printf("CycleTime0: \033[0;32m%u \033[0;36m[Esi:Dc:OpMode:CycleTimeSync0]\033[0m\n",*(uint32_t*)ptr);
					ptr += 4;
					printf("ShiftTime0: \033[0;32m%u \033[0;36m[Esi:Dc:OpMode:ShiftTimeSync0]\033[0m\n",*(uint32_t*)ptr);
					ptr += 4;
					printf("ShiftTime1: \033[0;32m%u \033[0;36m[Esi:Dc:OpMode:ShiftTimeSync1]\033[0m\n",*(uint32_t*)ptr);
					ptr += 4;
					printf("CycleTimeSync1@Factor: \033[0;32m%d \033[0;36m[Esi:Dc:OpMode:CycleTimeSync1@Factor]\033[0m\n",*(int16_t*)ptr);
					ptr += 2;
					printf("AssignActivate: \033[0;32m0x%.04X \033[0;36m[Esi:Dc:OpMode:AssignActivate]\033[0m\n",*(uint16_t*)ptr);
					ptr += 2;
					printf("CycleTimeSync0@Factor: \033[0;32m%d \033[0;36m[Esi:Dc:OpMode:CycleTimeSync0@Factor]\033[0m\n",*(int16_t*)ptr);
					ptr += 2;
					printf("Name index: \033[0;32m%d \033[0;36m[Esi:Dc:OpMode:Name]\033[0m\n",*(ptr++));
					printf("Description index: \033[0;32m%d \033[0;36m[Esi:Dc:OpMode:Desc]\033[0m\n",*(ptr++));
					ptr += 4; // Reserved
					if(dc < dccnt-1)printf("\n");
				}
			}
			break;
			case EEPROMCategoryTXPDO:
			case EEPROMCategoryRXPDO: {
				printf("Index: \033[0;32m0x%.04X\033[0m\n",*(uint16_t*)ptr);
				ptr += 2;
				uint8_t entries = *(ptr++);
				printf("%d entries\n",entries);
				printf("Related to SyncManager #\033[0;32m%d\033[0m\n",*(ptr++));
				printf("Related to DC #\033[0;32m%d\033[0m\n",*(ptr++));
				printf("Name index: \033[0;32m%d\033[0m\n",*(ptr++));
				uint16_t flags = *(uint16_t*)ptr;
				if(flags) {
					printf("Flags:\n");
					if(flags & 0x0001) printf("- PdoMandatory \033[0;36m[Esi:RTxPdo@Mandatory]\033[0m\n");
					if(flags & 0x0002) printf("- PdoDefault \033[0;36m[Esi:RTxPdo@Sm]\033[0m\n");
					if(flags & 0x0004) printf("- Reserved (PdoOversample)\n");
					if(flags & 0x0010) printf("- PdoFixedContent \033[0;36m[Esi:RTxPdo@Fixed]\033[0m\n");
					if(flags & 0x0020) printf("- PdoVirtualContent \033[0;36m[Esi:RTxPdo@Virtual]\033[0m\n");
					if(flags & 0x0040) printf("- Reserved (PdoDownloadAnyway)\n");
					if(flags & 0x0080) printf("- Reserved (PdoFromModule)\n");
					if(flags & 0x0100) printf("- PdoModuleAlign \033[0;36m[Esi:Slots:ModulePdoGroup@Alignment]\033[0m\n");
					if(flags & 0x0200) printf("- PdoDependOnSlot \033[0;36m[Esi:RTxPdo:Index@DependOnSlot]\033[0m\n");
					if(flags & 0x0400) printf("- PdoDependOnSlotGroup \033[0;36m[Esi:RTxPdo:Index@DependOnSlotGroup]\033[0m\n");
					if(flags & 0x0800) printf("- PdoOverwrittenByModule \033[0;36m[Esi:RTxPdo@OverwrittenByModule]\033[0m\n");
					if(flags & 0x1000) printf("- Reserved (PdoConfigurable)\n");
					if(flags & 0x2000) printf("- Reserved (PdoAutoPdoName)\n");
					if(flags & 0x4000) printf("- Reserved (PdoDisAutoExclude)\n");
					if(flags & 0x8000) printf("- Reserved (PdoWritable)\n");
				}
				ptr += 2;
				printf("\n");
				if(verbose) {
					for(uint8_t entry = 0; entry < entries; ++entry) {
						printf("Index: \033[0;32m0x%.04X \033[0;36m[Esi:PdoType:EntryType@Index]\033[0m\n",*(uint16_t*)ptr);
						ptr += 2;
						printf("SubIndex: \033[0;32m0x%.02X \033[0;36m[Esi:PdoType:EntryType@Subindex]\033[0m\n",*(ptr)++);
						printf("Name Index: \033[0;32m0x%.02X \033[0;36m[Esi:PdoType:EntryType@Name]\033[0m\n",*(ptr)++);
						printf("Data Type (CoE index): \033[0;32m0x%.02X \033[0;36m[Esi:PdoType:EntryType@DataType]\033[0m\n",*(ptr)++);
						printf("BitLen: \033[0;32m0x%.02X \033[0;36m[Esi:PdoType:EntryType@DataType]\033[0m\n",*(ptr)++);
						ptr += 2; // Reserved Flags
						//ptr += 8; // 8 bytes per entry
						if(entry < entries-1) printf("\n");
					}
				}
			}
			break;
		}
		ptr = nextCategoryPtr;
		category = (uint16_t*)ptr;
		++categoryNo;
	}
	printf("--------------------------\n");

	close(fd);

	err = munmap((void*)p, statbuf.st_size);

	if(err != 0){
		printf("UnMapping Failed (%d)\n",errno);
		return 1;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	printf("%s v%s\n",APP_NAME,APP_VERSION);
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
		if(0 == strcmp(argv[i],"--verbose") ||
		   0 == strcmp(argv[i],"-v"))
		{
			verbose = true;
		} else
		if(0 == strcmp(argv[i],"--nosii") ||
		   0 == strcmp(argv[i],"-n"))
		{
			nosii = true;
		} else
		if(0 == strcmp(argv[i],"--dictionary") ||
		   0 == strcmp(argv[i],"-d"))
		{
			writeobjectdict = true;
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

	if(encode && nosii && !writeobjectdict) {
		printf("Assuming Object Dictionary should be generated...\n");
		writeobjectdict = true;
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