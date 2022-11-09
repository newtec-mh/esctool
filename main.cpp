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
#include "sii.h"
#include "soesconfigwriter.h"

std::vector<char*> m_customStr;

bool verbose = false;
bool very_verbose = false;
bool writeobjectdict = false;
bool nosii = false;
bool encodepdo = false; // Put PDOs in SII EEPROM

// Decide if input from XML should be treated as LE
bool input_endianness_is_little = false;

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
	printf("\t --dictionary/-d : Generate SSC object dictionary (default if --nosii and !--decode)\n");
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
				SII::encodeEEPROMBinary(vendor_id, dev,encodepdo,file,output,very_verbose);
			}

			// Write slave stack object dictionary
			if(writeobjectdict && NULL != dev->profile &&
			   NULL != dev->profile->dictionary)
			{
				SOESConfigWriter sscwriter(input_endianness_is_little);
				sscwriter.writeSSCFiles(dev);
			}
		} else {
			printf("No 'Devices' nodes could be parsed\n");
		}
	} else {
		printf("Document has no root node!\n");
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
			printf("Encoding PDOs to SII EEPROM binary\n");
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
		SII::decodeEEPROMBinary(inputfile,verbose);
	} else if(encode) {
		return encodeSII(inputfile,outputfile);
	}
	return 0;
}