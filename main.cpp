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
#include "esctool.h"
#include "esidefs.h"
#include "esctooldefs.h"
#include "esctoolhelpers.h"
#include "siiencode.h"

std::vector<char*> m_customStr;

bool verbose = false;
bool very_verbose = false;
bool writeobjectdict = false;
bool writeconfig = true;
bool nosii = false;
bool encodepdo = false; // Put PDOs in SII EEPROM

// Decide if input from XML should be treated as LE
bool input_endianness_is_little = false;

#define SOES_DEFAULT_BUFFER_PREALLOC_FACTOR 3
std::string objectdictfile	= "objectlist.c";
std::string utypesfile		= "utypes.h";
std::string ecatconfig		= "ecat_options.h";

uint32_t vendor_id = 0x0;
const char* vendor_name = NULL;

std::list<Group*> groups;
std::list<Device*> devices;

void printUsage(const char* name) {
	printf("Usage: %s [options] --input/-i <input-file>\n",name);
	printf("Options:\n");
	printf("\t --decode : Decode and print a binary SII file\n");
	printf("\t --verbose/-v : Flood some more information to stdout when applicable\n");
	printf("\t --nosii/-n : Don't generate SII EEPROM binary (only for !--decode)\n");
	printf("\t --dictionary/-d : Generate object dictionary (default if --nosii and !--decode)\n");
	printf("\t --encodepdo/-ep : Encode PDOs to SII EEPROM\n");
	printf("\n");
}

uint32_t hexdecstr2uint32(const char* s) {
	uint32_t r = 0;
	if(s[0] == '#' && (s[1] == 'x' || s[1] == 'X')) {
		char c[strlen(s)+1]; // Wooooo lord stringlengths and input sanitizing
		strcpy(c,s);
		c[0] = '0'; // Make it easy for sscanf
		if(1 != sscanf(c,"%x",&r)) r = 0;
	} else if (s[0] == 'x' || s[0] == 'X') {
		char c[strlen(s)+2]; // Wooooo lord stringlengths and input sanitizing
		strcpy(c+1,s);
		c[0] = '0'; // Make it easy for sscanf
		if(1 != sscanf(c,"%x",&r)) r = 0;
	} else {
		if(1 != sscanf(s,"%u",&r)) r = 0;
	}
	return r;
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

std::string CNameify(const char* str, bool capitalize = false) {
	std::string r(str);
	for(uint8_t i = 0; i < r.size(); ++i) {
		if(capitalize) {
			r[i] = std::toupper(r[i]);
		} else r[i] = std::tolower(r[i]);
		if(r[i] == ' ') r[i] = '_';
	}
	return r;
};

void printObject (Object* o) {
//	printf("Obj: Index: 0x%.04X, Name: '%s'\n",(NULL != o->index ? EC_SII_HexToUint32(o->index) : 0),o->name);
	printf("Obj: Index: 0x%.04X, Name: '%s', DefaultData: '%s', BitSize: '%u'\n",o->index,o->name,o->defaultdata,o->bitsize);
	for(Object* si : o->subitems) printObject(si);
};

void printDataType (DataType* dt) {
	printf("DataType: Name: '%s', Type: '%s'\n",dt->name,dt->type);
	for(DataType* dsi : dt->subitems) printDataType(dsi);
};

void printDataTypeVerbose (DataType* dt) {
	printf("-----------------:\n");
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
	printf("-----------------:\n");
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
			//pdo->index = pdochild->GetText();
			pdo->index = hexdecstr2uint32(pdochild->GetText());
//			printf("Device/%s/Index: '%s'\n",xmlpdo->Name(),pdo->index);
			printf("Device/%s/Index: '0x%.04X'\n",xmlpdo->Name(),pdo->index);
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
					//entry->index = entrychild->GetText();
					entry->index = hexdecstr2uint32(entrychild->GetText());
//					if(verbose) printf("Device/%s/Entry/Index: '%s'\n",xmlpdo->Name(),entry->index);
					if(verbose) printf("Device/%s/Entry/Index: '0x%.04X'\n",xmlpdo->Name(),entry->index);
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

void parseXMLSyncUnit(const tinyxml2::XMLElement* xmlsu, Device* dev) {
	SyncUnit* su = new SyncUnit();
	for (const tinyxml2::XMLAttribute* attr = xmlsu->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		if(0 == strcmp(attr->Name(),"SeparateSu")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->separate_su))
				su->separate_su = (attr->IntValue() == 1) ? true : false;
			printf("Device/Su/@SeparateSu: %s ('%s')\n",su->separate_su ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"SeparateFrame")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->separate_frame))
				su->separate_frame = (attr->IntValue() == 1) ? true : false;
			printf("Device/Su/@SeparateFrame: %s ('%s')\n",su->separate_frame ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"DependOnInputState")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->depend_on_input_state))
				su->depend_on_input_state = (attr->IntValue() == 1) ? true : false;
			printf("Device/Su/@DependOnInputState: %s ('%s')\n",su->depend_on_input_state ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"FrameRepeatSupport")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->frame_repeat_support))
				su->frame_repeat_support = (attr->IntValue() == 1) ? true : false;
			printf("Device/Su/@FrameRepeatSupport: %s ('%s')\n",su->frame_repeat_support ? "yes" : "no",attr->Value());
		} else
		{
			printf("Unhandled Device/%s Attribute: '%s' = '%s'\n",xmlsu->Name(),attr->Name(),attr->Value());
		}
	}
	dev->syncunit = su;
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
//			obj->index = objchild->GetText();
			obj->index = hexdecstr2uint32(objchild->GetText());
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
				if(0 == strcmp(infochild->Name(),"DefaultString")) {
					obj->defaultstring = infochild->GetText();
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
//		if(NULL == obj->index) obj->index = parent->index;
		if(0 == obj->index) obj->index = parent->index;
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
		if(0 == strcmp(child->Name(),"Su")) {
			parseXMLSyncUnit(child,dev);
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
			// Create a boilerplate object dictionary if nothing exists and CoE is enabled
			// For now we assume either everything or nothing is there...
			// Later we should check the individual objects and check the needed ones
			// are there.
			if(writeobjectdict && (NULL == dev->profile || NULL == dev->profile->dictionary) && 
				dev->mailbox && dev->mailbox->coe_sdoinfo)
			{
				printf("Creating minimal object dictionary...\n");

				if(!dev->profile) dev->profile = new Profile;
				Dictionary* dict = new Dictionary;
				dev->profile->dictionary = dict;

				// Create a rudimentary datatype collection
				DataType* DT_UDINT = new DataType;
				DT_UDINT->bitsize = 32;
				DT_UDINT->name = UDINTstr;
				dev->profile->dictionary->datatypes.push_back(DT_UDINT);

				DataType* DT_UINT = new DataType;
				DT_UINT->bitsize = 16;
				DT_UINT->name = UINTstr;
				dev->profile->dictionary->datatypes.push_back(DT_UINT);

				DataType* DT_USINT = new DataType;
				DT_USINT->bitsize = 8;
				DT_USINT->name = USINTstr;
				dev->profile->dictionary->datatypes.push_back(DT_USINT);

				DataType* DT_DINT = new DataType;
				DT_DINT->bitsize = 32;
				DT_DINT->name = DINTstr;
				dev->profile->dictionary->datatypes.push_back(DT_DINT);

				DataType* DT_INT = new DataType;
				DT_INT->bitsize = 16;
				DT_INT->name = INTstr;
				dev->profile->dictionary->datatypes.push_back(DT_INT);

				DataType* DT_SINT = new DataType;
				DT_SINT->bitsize = 8;
				DT_SINT->name = USINTstr;
				dev->profile->dictionary->datatypes.push_back(DT_SINT);

				const char devtype[] = "00001389"; // Hex representation of 5001

				Object* x1000 = new Object;
				x1000->index = 0x1000;
				x1000->datatype = DT_UDINT;
				x1000->name = devTypeStr;
				x1000->defaultdata = devtype;
				dict->objects.push_back(x1000);

				size_t L = 32;
				char s[L];
				snprintf(s,L,"STRING(%lu)",strlen(dev->name));
				char* dt_typename = new char[sizeof(s)];
				strcpy(dt_typename,s);
				m_customStr.push_back(dt_typename);

				Object* x1008 = new Object;
				x1008->index = 0x1008;
				x1008->type = dt_typename;
				x1008->name = devNameStr;
				x1008->defaultstring = dev->name;
				dict->objects.push_back(x1008);

				// RX/TXPDO mapping
				for(auto pdoList : { dev->rxpdo, dev->txpdo }) {
					for(Pdo* pdo : pdoList) {
						Object* pdo_obj = new Object;
						pdo_obj->index = pdo->index;
						pdo_obj->name = pdo->name;
						uint32_t rxsize_bytes = 0;

						DataType* dt = new DataType;
						snprintf(s,L,"DT%.04X",pdo->index);
						dt_typename = new char[sizeof(s)];
						strcpy(dt_typename,s);
						m_customStr.push_back(dt_typename);
						dt->name = dt_typename;

						pdo_obj->datatype = dt;

						if(pdo->entries.size() > 0) {
							Object* numberOfEntries_obj = new Object;
							numberOfEntries_obj->name = numberOfEntriesStr;
							// Create the DataType subitem for the first subindex (USINT)
							DataType* sdt = new DataType;
							sdt->name = subIndex000Str;
							sdt->type = DT_USINT->name;
							sdt->bitsize = DT_USINT->bitsize;
							sdt->bitoffset = 0;
							sdt->subindex = 0;
							dt->subitems.push_back(sdt);
							numberOfEntries_obj->index = pdo->index;
							numberOfEntries_obj->datatype = sdt;
							dt->bitsize += numberOfEntries_obj->datatype->bitsize;
							dt->bitsize += numberOfEntries_obj->datatype->bitsize; // FIXME padding

							snprintf(s,L,"%.02X",(uint32_t)(pdo->entries.size() & 0xFF));
							char* numberOfEntriesVal = new char[sizeof(s)];
							strcpy(numberOfEntriesVal,s);
							m_customStr.push_back(numberOfEntriesVal);
							numberOfEntries_obj->defaultdata = numberOfEntriesVal;
							pdo_obj->subitems.push_back(numberOfEntries_obj);

							for(PdoEntry* e : pdo->entries) {
								// Create the DataType subitem for current subindex
								sdt = new DataType;
								Object* pdoEntry_obj = new Object;
								snprintf(s,L,"SubIndex %.03d",e->subindex);
								char* entryName = new char[sizeof(s)];
								strcpy(entryName,s);
								m_customStr.push_back(entryName);
								sdt->name = entryName;
								sdt->subindex = e->subindex;
								sdt->type = e->datatype;
								// Set the offset of the "new" datatype subitem
								sdt->bitoffset = dt->bitsize;
								sdt->bitsize = e->bitlen;
								// Increase the bitsize
								dt->bitsize += e->bitlen;
								// TODO Flags etc.
								pdoEntry_obj->index = pdo->index;
								pdoEntry_obj->name = entryName;
								pdoEntry_obj->datatype = sdt;

								snprintf(s,L,"%.04X%.02X%.02X",e->index,e->subindex,e->bitlen);
								char* defaultData = new char[sizeof(s)];
								strcpy(defaultData,s);
								m_customStr.push_back(defaultData);
								pdoEntry_obj->defaultdata = defaultData;

								pdo_obj->subitems.push_back(pdoEntry_obj);
								dt->subitems.push_back(sdt);
							}
						}
						dict->datatypes.push_back(dt);
						dict->objects.push_back(pdo_obj);
					}
				}

				auto createArrayDT = [&dict](uint16_t index, const int entries, DataType* entryDT) {
					DataType* dtARR = new DataType;
					size_t L = 32;
					char s[L];
					snprintf(s,L,"DT%.04XARR",index);
					char* arrName = new char[sizeof(s)];
					strcpy(arrName,s);
					m_customStr.push_back(arrName);
					dtARR->name = arrName;
					dtARR->basetype = entryDT->name;
					dtARR->bitsize = entries*(entryDT->bitsize);
					dtARR->arrayinfo = new ArrayInfo;
					dtARR->arrayinfo->elements = entries;
					dtARR->arrayinfo->lowerbound = 1;

					dict->datatypes.push_back(dtARR);

					DataType* dt = new DataType;
					snprintf(s,L,"DT%.04X",index);
					char* dtName = new char[sizeof(s)];
					strcpy(dtName,s);
					m_customStr.push_back(dtName);
					dt->name = dtName;
					dt->bitsize = (entries*(entryDT->bitsize))+16;
					dt->subitems.push_back(
						new DataType {
							.name = subIndex000Str,
							.type = USINTstr,
							.bitsize = 8,
							.subindex = 0 });
					dt->subitems.push_back(
						new DataType {
							.name = "Elements",
							.type = arrName,
							.bitsize = dtARR->bitsize,
							.bitoffset = 16 });
					dt->arrayinfo = dtARR->arrayinfo;
					dict->datatypes.push_back(dt);
					return dt;
				};

				// SyncManager types 0x1C00
				DataType* DT1C00 = createArrayDT(0x1C00,dev->syncmanagers.size(),DT_USINT);

				Object* x1C00 = new Object;
				x1C00->index = 0x1C00;
				x1C00->datatype = DT1C00;
				x1C00->bitsize = DT1C00->bitsize;
				x1C00->name = devSMTypeStr;

				snprintf(s,L,"%.02lu",dev->syncmanagers.size());
				char* x1C00entries = new char[sizeof(s)];
				strcpy(x1C00entries,s);
				m_customStr.push_back(x1C00entries);

				x1C00->subitems.push_back(new Object {
						.index = x1C00->index,
						.name = subIndex000Str,
						.datatype = DT_USINT,
						.defaultdata = x1C00entries});

				uint8_t smno = 0;
				for(SyncManager* sm : dev->syncmanagers) {
					Object* sm_obj = new Object;
					sm_obj->index = x1C00->index;
					sm_obj->datatype = DT_USINT;
					snprintf(s,L,"SM%d type",smno);
					char* objname = new char[sizeof(s)];
					strcpy(objname,s);
					m_customStr.push_back(objname);
					sm_obj->name = objname;

					if(0 == strcmp(sm->type,"MBoxOut")) {
						snprintf(s,L,"%.02d",1);
					} else if(0 == strcmp(sm->type,"MBoxIn")) {
						snprintf(s,L,"%.02d",2);
					} else if(0 == strcmp(sm->type,"Outputs")) {
						snprintf(s,L,"%.02d",3);
					} else if(0 == strcmp(sm->type,"Inputs")) {
						snprintf(s,L,"%.02d",4);
					}
					char* val = new char[sizeof(s)];
					strcpy(val,s);
					m_customStr.push_back(val);
					sm_obj->defaultdata = val;
					sm_obj->bitsize = DT_USINT->bitsize;
					sm_obj->bitoffset = (smno * DT_USINT->bitsize);
					++smno;
					x1C00->subitems.push_back(sm_obj);
				}
				dict->objects.push_back(x1C00);

				// SyncManager mappings 0x1C10-0x1C20
				std::vector<std::list<Pdo*> > syncManagerMappings = {{},{},{},{}};
				for(auto pdoList : { dev->rxpdo, dev->txpdo }) {
					for(Pdo* pdo : pdoList)
						syncManagerMappings[pdo->syncmanager].push_back(pdo);
				}
				smno = 0;
				for(auto pdoList : syncManagerMappings) {
					Object* mappingObject = new Object;
					mappingObject->index = 0x1C10 + smno;
					DataType* DTmapping = createArrayDT(mappingObject->index,pdoList.size(),DT_UINT);
					mappingObject->datatype = DTmapping;

					snprintf(s,L,"SM%d mappings",smno);
					char* objname = new char[sizeof(s)];
					strcpy(objname,s);
					m_customStr.push_back(objname);
					mappingObject->name = objname;
					mappingObject->bitsize = 16; // size + padding

					snprintf(s,L,"%.02lu",pdoList.size());
					char* entries = new char[sizeof(s)];
					strcpy(entries,s);
					m_customStr.push_back(entries);

					mappingObject->subitems.push_back(new Object {
							.index = mappingObject->index,
							.name = subIndex000Str,
							.datatype = DT_USINT,
							.defaultdata = entries});

					for(auto pdo : pdoList) {
						Object* mappedObj = new Object;
						mappedObj->index = mappingObject->index;
						mappedObj->datatype = DT_UINT;
						snprintf(s,L,"%.02X",pdo->index);
						char* val = new char[sizeof(s)];
						strcpy(val,s);
						m_customStr.push_back(val);
						mappedObj->defaultdata = val;
						mappedObj->name = pdo->name != NULL ? pdo->name : "Mapped object";
						mappedObj->bitoffset = mappingObject->bitsize;
						mappingObject->bitsize += DT_UINT->bitsize;
						mappedObj->bitsize = DT_UINT->bitsize;
						mappingObject->subitems.push_back(mappedObj);
					}

					dict->objects.push_back(mappingObject);
					++smno;
				}

				for(auto pdoList : { dev->txpdo, dev->rxpdo }) {
					for(Pdo* pdo : pdoList) {
						Object* obj = NULL;
						for(PdoEntry* entry : pdo->entries) {
							if(NULL == obj || obj->index != entry->index) {
								if(NULL != obj) {
									// bitsize, subindex000 and bitlength
									dict->objects.push_back(obj);
								}
								obj = new Object;
								obj->index = entry->index;
								obj->name = entry->name;
							}
							snprintf(s,L,"%.02lu",pdo->entries.size());
							char* entries = new char[sizeof(s)];
							strcpy(entries,s);
							m_customStr.push_back(entries);

							obj->subitems.push_back(new Object {
								.index = obj->index,
								.name = subIndex000Str,
								.datatype = DT_USINT,
								.defaultdata = entries});

							obj->bitsize = 16;
							DataType* dt = NULL;
							for(DataType* d : dict->datatypes) {
								if(0 == strcmp(d->name,entry->datatype)) {
									dt = d;
									break;
								}
							}
							if(NULL == dt) printf("\n\nWARNING: DataType is NULL!\n\n");
							obj->subitems.push_back(new Object {
								.index = entry->index,
								.name = entry->name,
								.datatype = dt,
								.bitsize = dt->bitsize,
								.bitoffset = obj->bitsize
							});
							obj->bitsize += dt->bitsize;
						}
						dict->objects.push_back(obj);
					}
				}
			}

			if(verbose) {
				printf("Profile: %s\n",dev->profile ? "yes" : "no");
				if(NULL != dev->profile) {
					printf("Dictionary: %s\n",dev->profile->dictionary ? "yes" : "no");
					if(NULL != dev->profile->dictionary) {
						printf("Objects: %lu\n",dev->profile->dictionary->objects.size());
						for(Object* o : dev->profile->dictionary->objects) {
							printObject(o);
						}
						printf("DataTypes: %lu\n",dev->profile->dictionary->datatypes.size());
						for(DataType* dt : dev->profile->dictionary->datatypes) {
							printDataTypeVerbose(dt);
						}
					}
				}
				printf("Distributed Clock (DC): %s\n",dev->dc ? "yes" : "no");
			}

			// Write SII EEPROM file
			if(!nosii) {
				SIIEncode::encodeSII(vendor_id, dev,encodepdo,file,output,very_verbose);
			}

			if(writeconfig) {
				std::ofstream configout;
				configout.open(ecatconfig.c_str(), std::ios::out | std::ios::trunc);
				if(!configout.fail()) {
					printf("Writing SOES compatible configuration to '%s'\n",ecatconfig.c_str());
					configout << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n\n";
					configout << "#ifndef __ECAT_OPTIONS_H__\n";
					configout << "#define __ECAT_OPTIONS_H__\n\n";
					configout << "#include \"cc.h\"\n\n";

					uint8_t coedetails = 0x0;
					if(dev->mailbox) {
						configout << "#define USE_FOE          " << std::dec << (dev->mailbox->foe ? 1 : 0) << "\n";
						configout << "#define USE_EOE          " << std::dec << (dev->mailbox->eoe ? 1 : 0) << "\n";
						configout << "\n";
					} else {
						configout << "#define USE_FOE          0\n";
						configout << "#define USE_EOE          0\n";
						configout << "\n";
					}

					uint16_t defaultmbxsz = 128;
					for(SyncManager* sm : dev->syncmanagers) {
						if(0 == strcmp(sm->type,"MBoxOut") || 0 == strcmp(sm->type,"MBoxIn")) {
							defaultmbxsz = sm->defaultsize;
							if(sm->defaultsize != 0) {
								configout << "#define MBXSIZE            " << std::dec << sm->defaultsize << "\n";
								configout << "#define MBXSIZEBOOT        " << std::dec << sm->defaultsize << "\n";
								if(dev->mailbox && dev->mailbox->coe_completeaccess) {
									uint16_t bufsz = SOES_DEFAULT_BUFFER_PREALLOC_FACTOR*defaultmbxsz;
									uint16_t maxbufsz = bufsz;
									for(SyncManager* sm : dev->syncmanagers) {
										if(0 == strcmp(sm->type,"Outputs") ||
										   0 == strcmp(sm->type,"Inputs"))
										{
											maxbufsz = std::max(maxbufsz,sm->defaultsize);
										}
									}
									uint8_t prealloc_factor = SOES_DEFAULT_BUFFER_PREALLOC_FACTOR;
									while(bufsz < maxbufsz) {
										++prealloc_factor;
										bufsz = defaultmbxsz*prealloc_factor;
									}
									configout << "#define PREALLOC_FACTOR    " << std::dec << (uint32_t) prealloc_factor << "\n";
								}
								configout << "\n";
								break;
							}
						}
					}

					for(SyncManager* sm : dev->syncmanagers) {
						if(0 == strcmp(sm->type,"MBoxOut")) {
							configout << "#define MBX0_sma         " << "0x" << std::hex << sm->startaddress << "\n";
							configout << "#define MBX0_sml         " << std::dec << sm->defaultsize << "\n";
							configout << "#define MBX0_sme         MBX0_sma+MBX0_sml-1\n";
							configout << "#define MBX0_smc         " << "0x" << std::hex << (uint32_t) sm->controlbyte << "\n";

							configout << "#define MBX0_sma_b       " << "0x" << std::hex << sm->startaddress << "\n";
							configout << "#define MBX0_sml_b       " << std::dec << sm->defaultsize << "\n";
							configout << "#define MBX0_sme_b       MBX0_sma_b+MBX0_sml_b-1\n";
							configout << "#define MBX0_smc_B       " << "0x" << std::hex << (uint32_t) sm->controlbyte <<"\n";
							configout << "\n";
						} else
						if(0 == strcmp(sm->type,"MBoxIn")) {
							configout << "#define MBX1_sma         " << "0x" << std::hex << sm->startaddress << "\n";
							configout << "#define MBX1_sml         " << std::dec << sm->defaultsize << "\n";
							configout << "#define MBX1_sme         MBX1_sma+MBX1_sml-1\n";
							configout << "#define MBX1_smc         " << "0x" << std::hex << (uint32_t) sm->controlbyte <<"\n";

							configout << "#define MBX1_sma_b       " << "0x" << std::hex << sm->startaddress << "\n";
							configout << "#define MBX1_sml_b       " << std::dec << sm->defaultsize << "\n";
							configout << "#define MBX1_sme_b       MBX1_sma_b+MBX1_sml_b-1\n";
							configout << "#define MBX1_smc_b       " << "0x" << std::hex << (uint32_t) sm->controlbyte <<"\n";
							configout << "\n";
						} else
						if(0 == strcmp(sm->type,"Outputs")) { // TODO verify that the actual assigned SyncManager *is* 2
							configout << "#define SM2_sma          " << "0x" << std::hex << sm->startaddress << "\n";
							configout << "#define SM2_smc          " << "0x" << std::hex << (uint32_t) sm->controlbyte << "\n";
							configout << "#define SM2_act          " << (sm->enable ? 1 : 0) << "\n";
							configout << "#define MAX_RXPDO_SIZE   " << std::dec << sm->defaultsize << "\n";
							configout << "\n";
						} else
						if(0 == strcmp(sm->type,"Inputs")) { // TODO verify that the actual assigned SyncManager *is* 3
							configout << "#define SM3_sma          " << "0x" << std::hex << sm->startaddress << "\n";
							configout << "#define SM3_smc          " << "0x" << std::hex << (uint32_t) sm->controlbyte << "\n";
							configout << "#define SM3_act          " << (sm->enable ? 1 : 0) << "\n";
							configout << "#define MAX_TXPDO_SIZE   " << std::dec << sm->defaultsize << "\n";
							configout << "\n";
						}
					}
					uint16_t dynrxpdo = 0;
					for(Pdo* pdo : dev->rxpdo) {
						if(!pdo->fixed) ++dynrxpdo;
					}
					uint16_t dyntxpdo = 0;
					for(Pdo* pdo : dev->txpdo) {
						if(!pdo->fixed) ++dyntxpdo;
					}
					configout << "#define MAX_MAPPINGS_SM2      " << std::dec << dynrxpdo << "\n";
					configout << "#define MAX_MAPPINGS_SM3      " << std::dec << dyntxpdo << "\n";

					configout << "\n";
					configout << "#endif /* __ECAT_OPTIONS_H__ */\n";
					configout.sync_with_stdio();
					configout.close();
				}
			}

			// Write slave stack object dictionary
			if(writeobjectdict && NULL != dev->profile && NULL != dev->profile->dictionary) {
				std::ofstream typesout;
				typesout.open(utypesfile.c_str(), std::ios::out | std::ios::trunc);
				if(!typesout.fail()) {
					printf("Writing SOES compatible type definitions to '%s'\n",utypesfile.c_str());
					typesout << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n\n";
					typesout << "#ifndef UTYPES_H\n";
					typesout << "#define UTYPES_H\n\n";
					typesout << "#include <stdint.h>\n";
					typesout << "\n";

					auto getCType = [](const char* type) {
						if(0 == strncmp(type,BOOLstr,4) || 0 == strcmp(type,BITstr)) {
							return "bool";
						} else
						if(0 == strcmp(type,SINTstr)) {
							return "int8_t";
						} else
						if(0 == strcmp(type,INTstr)) {
							return "int16_t";
						} else
						if(0 == strcmp(type,DINTstr)) {
							return "int32_t";
						} else
						if(0 == strcmp(type,USINTstr)) {
							return "uint8_t";
						} else
						if(0 == strcmp(type,UINTstr)) {
							return "uint16_t";
						} else
						if(0 == strcmp(type,UDINTstr)) {
							return "uint32_t";
						}
						printf("Warning: Unable to find C-type for '%s'\n",type);
						return (const char*)NULL;
					};

					for(Object* o : dev->profile->dictionary->objects) {
//						uint16_t index = EC_SII_HexToUint32(o->index);
						uint16_t index = o->index & 0xFFFF;
						if(index < 0x2000) continue;

						if(!o->subitems.empty()) {
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
								typesout << getCType(type);
								typesout << " ";
								typesout << CNameify(si->name);
								typesout << ";";
								typesout << " /* "; 
								typesout << std::uppercase
									<< std::hex
									<< std::setfill('0')
									<< std::setw(4)
									<< si->index
									<< "."
									<< std::setw(2)
									<< subitem;
								typesout << "*/\n";
								++subitem;
							}
							typesout << "} _" << CNameify(o->name,true) << ";\n\n";
							typesout << "extern _"
								<< CNameify(o->name,true)
								<< " "
								<< CNameify(o->name,true)
								<< ";\n\n";
						} else {
							const char* type = o->datatype ? 
								(o->datatype->type ? o->datatype->type :
									o->datatype->name) :
								o->type;
							typesout << "extern"
								 << " "
								 << getCType(type)
								 << " "
								 << CNameify(o->name)
								 << ";\n\n";
						}
					}
					typesout << "#endif /* UTYPES_H */\n";
					typesout.sync_with_stdio();
					typesout.close();
				} else {
					printf("Couln't open '%s' for writing\n",utypesfile.c_str());
				}

				std::ofstream out;
				out.open(objectdictfile.c_str(), std::ios::out | std::ios::trunc);
				if(!out.fail()) {
					printf("Writing SOES compatible object dictionary to '%s'\n",objectdictfile.c_str());
					out << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n"
					<< "#include \"esc_coe.h\"\n"
					<< "#include \"" << utypesfile << "\"\n"
					<< "#include <stddef.h>\n"
					<< "\n";

					// Generate string objects
					for(Object* o : dev->profile->dictionary->objects) {
						out << "static const char acName"
						    << std::hex
						    << std::setfill('0')
						    << std::setw(4)
						    << std::uppercase
						    << (o->index & 0xFFFF);
//						    << EC_SII_HexToUint32(o->index);
						out << "[] = \"" << o->name << "\";\n";
						// TODO handle several levels?
						int subitem = 0;
						for(Object* si : o->subitems) {
							out << "static const char acName"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << (si->index & 0xFFFF);
							    //<< EC_SII_HexToUint32(si->index);
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
							    << (o->index & 0xFFFF);
//							    << EC_SII_HexToUint32(o->index);
							out << "_00[] = \"";
							if(NULL != o->defaultstring) {
								out << o->defaultstring;
							} else
							if(NULL != o->defaultdata) {
								// Can we assume strings set in DefaultData are
								// hex encoded byte values?
								std::string str("");
								for(size_t i = 0; i < strlen(o->defaultdata); i+=2) {
									char s[3];
									strncpy(s,&(o->defaultdata[i]),2);
									s[2] = '\0';
									str += (char)(strtol(s,NULL,16));
								}
								out << str;
							} else {
								out << "(null)";
							}
							out << "\";\n";
						}
					}
					out << "\n";

					auto writeObject = [&out](Object* obj, Object* parent, int& subitem, const int nitems, Dictionary* dict = NULL) {
						bool objref = false;
						out << "{ 0x"
						    << std::setw(2)
						    << subitem
						    << ", ";

//						uint16_t index = EC_SII_HexToUint32(obj->index);
						uint16_t index = obj->index & 0xFFFF;

						DataType* datatype = obj->datatype;
						const char* type = datatype ? (datatype->type ? datatype->type : datatype->name) : obj->type;
						const ObjectFlags* flags = obj->flags ? obj->flags : (datatype ? datatype->flags : NULL);
						uint32_t bitsize = obj->bitsize ? obj->bitsize : (datatype ? datatype->bitsize : 0);
						if(NULL == type) {
//							printf("\033[0;31mWARNING:\033[0m DataType of object '%s' subitem '%d' is \033[0;33mNULL\033[0m\n\n\n",obj->index,subitem);
							printf("\033[0;31mWARNING:\033[0m DataType of object '0x%.04X' subitem '%u' is \033[0;33mNULL\033[0m\n\n\n",obj->index,subitem);
						} else
						if(0 == strncmp(type,"STRING",5)) {
							out << "DTYPE_VISIBLE_STRING" << ", ";
							out << "sizeof(";
							out << "acValue"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << index;
							out << "_00) << 3";
							objref = true;
						} else  // capitalization of all these strings?
						{
						if(0 == strncmp(type,BOOLstr,4) || 0 == strcmp(type,BITstr)) {
							out << "DTYPE_BOOLEAN";
							bitsize = 1;
						} else
						if(0 == strcmp(type,SINTstr)) {
							out << "DTYPE_INTEGER8";
							bitsize = 8;
						} else
						if(0 == strcmp(type,INTstr)) {
							out << "DTYPE_INTEGER16";
							bitsize = 16;
						} else
						if(0 == strcmp(type,DINTstr)) {
							out << "DTYPE_INTEGER32";
							bitsize = 32;
						} else
						if(0 == strcmp(type,USINTstr)) {
							out << "DTYPE_UNSIGNED8";
							bitsize = 8;
						} else
						if(0 == strcmp(type,UINTstr)) {
							out << "DTYPE_UNSIGNED16";
							bitsize = 16;
						} else
						if(0 == strcmp(type,UDINTstr)) {
							out << "DTYPE_UNSIGNED32";
							bitsize = 32;
						} else
						{ // TODO handle more types?
							printf("\033[0;31mWARNING:\033[0m Unhandled Datatype '%s'\n",obj->type);
							out << "DTYPE_UNSIGNED32"; // Default
							bitsize = 32;
						}
						out << ", ";

						out << std::dec << (bitsize == 0 ? obj->bitsize : bitsize);
						}
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
						    << index;
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
							if((index <= 0x2000 || (0 == subitem || NULL == parent) && (0 != nitems))) {
								if(NULL != obj->defaultdata) {
									// TODO: can we assume data is hex or smth? HexDecStr...
									out << "0x";
									if(input_endianness_is_little) {
										for(size_t i = strlen(obj->defaultdata); i > 0; i-=2) {
											char s[3];
											strncpy(s,&(obj->defaultdata[i-2]),2);
											s[2] = '\0';
											out << s;
										}
									} else {
										out << obj->defaultdata;
									}
									out << ", NULL }";
								} else {
									out << 0
									<< ", NULL }";
								}
							} else { // Reference the objects from utypes.h
								if(0 == nitems) {
									out << "0, ";
									out << "&";
									out << CNameify(obj->name);
									out << " }";
								} else {
									out << "0, ";
									out << "&("
									<< CNameify(parent->name,true)
									<< "."
									<< CNameify(obj->name);
									out << ") }";
								}
							}
						} else {
							out << "0, ";
							out << "&acValue"
							    << std::hex
							    << std::setfill('0')
							    << std::setw(4)
							    << std::uppercase
							    << index;
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
						    << (o->index & 0xFFFF);
//						    << EC_SII_HexToUint32(o->index);
						out << "[] = {\n";
						if(o->subitems.empty()) {
							writeObject(o, NULL, subitem, 0, dev->profile->dictionary);
						} else {
							// TODO handle several levels?
							for(Object* si : o->subitems) {
								writeObject(si,o,subitem,o->subitems.size(),dev->profile->dictionary);
							}
						}
						out << " };\n\n";
					}

					out << "const _objectlist SDOobjects[] = {\n";
					for(Object* o : dev->profile->dictionary->objects) {
//						uint16_t index = EC_SII_HexToUint32(o->index);
						uint16_t index = o->index & 0xFFFF;
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
				printf("No dictionary could be parsed, writing boilerplate '%s' and '%s'\n",utypesfile.c_str(),objectdictfile.c_str());
				std::ofstream typesout;
				typesout.open(utypesfile.c_str(), std::ios::out | std::ios::trunc);
				if(!typesout.fail()) {
					typesout << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n\n";
					typesout << "#ifndef UTYPES_H\n";
					typesout << "#define UTYPES_H\n\n";
					typesout << "#include <stdint.h>\n";
					typesout << "\n";
					typesout << "#endif /* UTYPES_H */\n";
					typesout.sync_with_stdio();
					typesout.close();
				} else {
					printf("Couldn't open '%s' for writing\n",utypesfile.c_str());
				}
				std::ofstream objout;
				objout.open(objectdictfile.c_str(), std::ios::out | std::ios::trunc);
				if(!objout.fail()) {
					objout << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n\n";
					objout << "#include \"esc_coe.h\"\n";
					objout << "const _objectlist SDOobjects[] = {\n";
					objout << "{ 0xFFFF, 0xFF, 0xFF, 0xFF, NULL, NULL } };\n";
					objout << "\n\n";
					objout.sync_with_stdio();
					objout.close();
				} else {
					printf("Couldn't open '%s' for writing\n",objectdictfile.c_str());
				}
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
	ptr = p + EC_SII_EEPROM_FIRST_CAT_HDR_OFFSET_BYTE-4;
	printf("Size: \033[0;32m0x%.02X \033[0;36m[Esi:DeviceType:Eeprom:ByteSize]\033[0m\n",*(uint16_t*)ptr);
	ptr += 2;
	printf("Version: \033[0;32m0x%.02X\033[0m\n",*(uint16_t*)ptr);

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
					if(0x0 == enable) printf("- isn't enabled\n");
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
			printf("Verbose mode: ON\n");
			verbose = true;
		} else
		if(0 == strcmp(argv[i],"-vv"))
		{
			printf("Very verbose mode: ON\n");
			verbose = true;
			very_verbose = true;
		} else
		if(0 == strcmp(argv[i],"--nosii") ||
		   0 == strcmp(argv[i],"-n"))
		{
			printf("Not generating SII EEPROM binary\n");
			nosii = true;
		} else
		if(0 == strcmp(argv[i],"--encodepdo") ||
		   0 == strcmp(argv[i],"-ep"))
		{
			printf("Not generating SII EEPROM binary\n");
			encodepdo = true;
		} else
		if(0 == strcmp(argv[i],"--bigendian") ||
		   0 == strcmp(argv[i],"-be"))
		{
			input_endianness_is_little = false;
		} else
		if(0 == strcmp(argv[i],"--littleendian") ||
		   0 == strcmp(argv[i],"-le"))
		{
			input_endianness_is_little = true;
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
	printf("\n");
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