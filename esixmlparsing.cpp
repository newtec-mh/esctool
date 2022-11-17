
#include "esixmlparsing.h"
#include "esctoolhelpers.h"

ESIXML::ESIXML(const int verbosity) :
	verbose(verbosity != 0), very_verbose(verbosity & 0x2),
	vendor_id(0x0), vendor_name(NULL) {}

ESIXML::~ESIXML() {};

std::list<Device*>& ESIXML::getDevices(void) { return devices; } ;
const uint32_t ESIXML::getVendorID(void) const { return vendor_id; };
const char* ESIXML::getVendorName(void) const { return vendor_name; };

void ESIXML::parse(const std::string& file) {
	if(tinyxml2::XML_SUCCESS != doc.LoadFile( file.c_str() )) {
		printf("Could not open '%s'\n",file.c_str());
		return;
	}
	const tinyxml2::XMLElement* root = doc.RootElement();
	if(NULL != root) {
		if(0 != strcmp(ESI_ROOTNODE_NAME,root->Name())) {
			printf("Document seemingly does not contain EtherCAT information (root node name is not '%s' but '%s')\n",ESI_ROOTNODE_NAME,root->Name());
			return;
		}
		parseXMLElement(root);
		printf("ESIXML: Parsed '%lu' device(s) from vendor 0x%.04X:'%s'\n",devices.size(),vendor_id,vendor_name);
		int devno = 1;
		for(Device* dev : devices) {
			printf("Device %.0d: '%s', Product code: '0x%.08X', %lu TXPDO(s), %lu RXPDO(s)\n",devno++,dev->name,dev->product_code,dev->txpdo.size(),dev->rxpdo.size());
			if(verbose) {
				for(auto pdoList : { dev->txpdo, dev->rxpdo }) {
					for(Pdo* pdo : pdoList) {
						printf("\tPDO: '%s', index: 0x%.04X has %lu entries\n",pdo->name,pdo->index,pdo->entries.size());
						for(PdoEntry* entry : pdo->entries) {
							printf("\t\tEntry: '%s', index: 0x%.04X, subindex: %u, datatype: '%s'\n",entry->name,entry->index,entry->subindex,entry->datatype);
						}
					}
				}
			}
		}
	}
}

void ESIXML::parseXMLGroup(const tinyxml2::XMLElement* xmlgroup) {
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

void ESIXML::parseXMLMailbox(const tinyxml2::XMLElement* xmlmailbox, Device* dev) {
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

void ESIXML::parseXMLPdo(const tinyxml2::XMLElement* xmlpdo, std::list<Pdo*>* pdolist) {
	Pdo* pdo = new Pdo();
	for (const tinyxml2::XMLAttribute* attr = xmlpdo->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		if(0 == strcmp(attr->Name(),"Mandatory")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&pdo->mandatory))
				pdo->mandatory = (attr->IntValue() == 1) ? true : false;
			if(very_verbose) printf("Device/%s/@Mandatory: %s ('%s')\n",xmlpdo->Name(),pdo->mandatory ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"Fixed")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&pdo->fixed))
				pdo->fixed = (attr->IntValue() == 1) ? true : false;
			if(very_verbose) printf("Device/%s/@Fixed: %s ('%s')\n",xmlpdo->Name(),pdo->fixed ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"Sm")) {
			pdo->syncmanager = attr->IntValue();
			if(very_verbose) printf("Device/%s/@Sm: %d\n",xmlpdo->Name(),pdo->syncmanager);
		} else
		if(0 == strcmp(attr->Name(),"Su")) {
			pdo->syncunit = attr->IntValue();
			if(very_verbose) printf("Device/%s/@Su: '%d'\n",xmlpdo->Name(),pdo->syncunit);
		} else
		{
			printf("Unhandled Device/%s Attribute: '%s' = '%s'\n",xmlpdo->Name(),attr->Name(),attr->Value());
		}
	}
	for (const tinyxml2::XMLElement* pdochild = xmlpdo->FirstChildElement();
		pdochild != 0; pdochild = pdochild->NextSiblingElement())
	{
		if(0 == strcmp(pdochild->Name(),"Index")) {
			pdo->index = hexdecstr2uint32(pdochild->GetText());
			if(verbose) printf("Device/%s/Index: '0x%.04X'\n",xmlpdo->Name(),pdo->index);
		} else
		if(0 == strcmp(pdochild->Name(),"Name")) {
			pdo->name = pdochild->GetText();
			if(verbose)printf("Device/%s/Name: '%s'\n",xmlpdo->Name(),pdo->name);
		} else
		if(0 == strcmp(pdochild->Name(),"Entry")) {
			PdoEntry* entry = new PdoEntry();
			for (const tinyxml2::XMLElement* entrychild = pdochild->FirstChildElement();
				entrychild != 0; entrychild = entrychild->NextSiblingElement())
			{
				if(0 == strcmp(entrychild->Name(),"Name")) {
					entry->name = entrychild->GetText();
					if(very_verbose) printf("Device/%s/Entry/Name: '%s'\n",xmlpdo->Name(),entry->name);
				} else
				if(0 == strcmp(entrychild->Name(),"Index")) {
					entry->index = hexdecstr2uint32(entrychild->GetText());
					if(very_verbose) printf("Device/%s/Entry/Index: '0x%.04X'\n",xmlpdo->Name(),entry->index);
				} else
				if(0 == strcmp(entrychild->Name(),"BitLen")) {
					entry->bitlen = entrychild->IntText();
					if(very_verbose) printf("Device/%s/Entry/BitLen: %d\n",xmlpdo->Name(),entry->bitlen);
				} else
				if(0 == strcmp(entrychild->Name(),"SubIndex")) {
					//entry->subindex = entrychild->IntText(); // TODO: HexDec
					entry->subindex = hexdecstr2uint32(entrychild->GetText());
					if(very_verbose) printf("Device/%s/Entry/SubIndex: %d\n",xmlpdo->Name(),entry->subindex);
				} else
				if(0 == strcmp(entrychild->Name(),"DataType")) {
					entry->datatype = entrychild->GetText();
					if(very_verbose) printf("Device/%s/Entry/DataType: '%s'\n",xmlpdo->Name(),entry->datatype);
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

void ESIXML::parseXMLSyncUnit(const tinyxml2::XMLElement* xmlsu, Device* dev) {
	SyncUnit* su = new SyncUnit();
	for (const tinyxml2::XMLAttribute* attr = xmlsu->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		if(0 == strcmp(attr->Name(),"SeparateSu")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->separate_su))
				su->separate_su = (attr->IntValue() == 1) ? true : false;
			if(very_verbose) printf("Device/Su/@SeparateSu: %s ('%s')\n",su->separate_su ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"SeparateFrame")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->separate_frame))
				su->separate_frame = (attr->IntValue() == 1) ? true : false;
			if(very_verbose) printf("Device/Su/@SeparateFrame: %s ('%s')\n",su->separate_frame ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"DependOnInputState")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->depend_on_input_state))
				su->depend_on_input_state = (attr->IntValue() == 1) ? true : false;
			if(very_verbose) printf("Device/Su/@DependOnInputState: %s ('%s')\n",su->depend_on_input_state ? "yes" : "no",attr->Value());
		} else
		if(0 == strcmp(attr->Name(),"FrameRepeatSupport")) {
			if(tinyxml2::XML_SUCCESS != attr->QueryBoolValue(&su->frame_repeat_support))
				su->frame_repeat_support = (attr->IntValue() == 1) ? true : false;
			if(very_verbose) printf("Device/Su/@FrameRepeatSupport: %s ('%s')\n",su->frame_repeat_support ? "yes" : "no",attr->Value());
		} else
		{
			printf("Unhandled Device/%s Attribute: '%s' = '%s'\n",xmlsu->Name(),attr->Name(),attr->Value());
		}
	}
	dev->syncunit = su;
}

void ESIXML::parseXMLDistributedClock(const tinyxml2::XMLElement* xmldc, DistributedClock* dc) {
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
					if(verbose) printf("Device/Dc/Opmode/Name: %s\n",opmode->name);
				} else
				if(0 == strcmp(dcopmodechild->Name(),"Desc")) {
					opmode->desc = dcopmodechild->GetText();
					if(very_verbose) printf("Device/Dc/Opmode/Desc: %s\n",opmode->desc);
				} else
				if(0 == strcmp(dcopmodechild->Name(),"CycleTimeSync0")) {
					opmode->cycletimesync0 = dcopmodechild->UnsignedText();
					if(very_verbose) printf("Device/Dc/Opmode/CycleTimeSync0: %u\n",opmode->cycletimesync0);
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
					if(very_verbose) printf("Device/Dc/Opmode/CycleTimeSync1: %u\n",opmode->cycletimesync1);
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
					if(very_verbose) printf("Device/Dc/Opmode/ShiftTimeSync0: %u\n",opmode->shifttimesync0);
					for (const tinyxml2::XMLAttribute* sts0attr = dcopmodechild->FirstAttribute();
						sts0attr != 0; sts0attr = sts0attr->Next())
					{
						printf("Unhandled Device/Dc/Opmode/ShiftTimeSync0 attribute: '%s' = '%s'\n",sts0attr->Name(),sts0attr->Value());
					}
				} else
				if(0 == strcmp(dcopmodechild->Name(),"ShiftTimeSync1")) {
					opmode->shifttimesync1 = dcopmodechild->UnsignedText();
					if(very_verbose) printf("Device/Dc/Opmode/ShiftTimeSync1: %u\n",opmode->shifttimesync1);
					for (const tinyxml2::XMLAttribute* sts1attr = dcopmodechild->FirstAttribute();
						sts1attr != 0; sts1attr = sts1attr->Next())
					{
						printf("Unhandled Device/Dc/Opmode/ShiftTimeSync1 attribute: '%s' = '%s'\n",sts1attr->Name(),sts1attr->Value());
					}
				} else
				if(0 == strcmp(dcopmodechild->Name(),"AssignActivate")) { // HexDecInt
					opmode->assignactivate = (EC_SII_HexToUint32(dcopmodechild->GetText()) & 0xFFFF);
					if(verbose) printf("Device/Dc/Opmode/AssignActivate: 0x%.04X\n",opmode->assignactivate);
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

void ESIXML::parseXMLObject(const tinyxml2::XMLElement* xmlobject, Dictionary* dict, Object* parent) {
	Object* obj = new Object;
	for (const tinyxml2::XMLElement* objchild = xmlobject->FirstChildElement();
		objchild != 0; objchild = objchild->NextSiblingElement())
	{
		if(0 == strcmp(objchild->Name(),"Index")) {
			obj->index = hexdecstr2uint32(objchild->GetText());
			if(verbose) printf("Object Index: 0x%.04X\n",obj->index);
		} else
		if(0 == strcmp(objchild->Name(),"Name")) {
			obj->name = objchild->GetText();
			if(verbose) printf("Object Name: '%s'\n",obj->name);
		} else
		if(0 == strcmp(objchild->Name(),"Type")) {
			obj->type = objchild->GetText();
			if(very_verbose) printf("Object Type: '%s'\n",obj->type);
		} else
		if(0 == strcmp(objchild->Name(),"BitSize")) {
			obj->bitsize = objchild->IntText();
			if(very_verbose) printf("Object BitSize: '%.02d'\n",obj->bitsize);
		} else
		if(0 == strcmp(objchild->Name(),"BitOffs")) {
			obj->bitoffset = objchild->IntText();
			if(very_verbose) printf("Object BitOffset: '%.02d'\n",obj->bitoffset);
		} else
		if(0 == strcmp(objchild->Name(),"Info")) {
			for (const tinyxml2::XMLElement* infochild = objchild->FirstChildElement();
				infochild != 0; infochild = infochild->NextSiblingElement())
			{
				if(0 == strcmp(infochild->Name(),"DefaultData")) {
					obj->defaultdata = infochild->GetText();
					if(verbose) printf("Object DefaultData: '%s'\n",obj->defaultdata);
				} else
				if(0 == strcmp(infochild->Name(),"DefaultString")) {
					obj->defaultstring = infochild->GetText();
					if(verbose) printf("Object DefaultData: '%s'\n",obj->defaultstring);
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
		obj->parent = parent;
		if(0 == obj->index) obj->index = parent->index;
		if(NULL == obj->datatype) obj->datatype = parent->datatype;
		if(NULL == obj->type) obj->type = parent->type;
		if(NULL == obj->flags) obj->flags = parent->flags;
		parent->subitems.push_back(obj);
	} else dict->objects.push_back(obj);
/*
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
	}*/
}

void ESIXML::parseXMLDataType(const tinyxml2::XMLElement* xmldatatype, Dictionary* dict, DataType* parent) {
	DataType* datatype = new DataType;
	for (const tinyxml2::XMLElement* dtchild = xmldatatype->FirstChildElement();
		dtchild != 0; dtchild = dtchild->NextSiblingElement())
	{
		if(0 == strcmp(dtchild->Name(),"Name")) {
			datatype->name = dtchild->GetText();
			if(verbose) printf("DataType/Name: '%s'\n",datatype->name);
		} else
		if(0 == strcmp(dtchild->Name(),"Type")) {
			datatype->type = dtchild->GetText();
			if(verbose) printf("DataType/Type: '%s'\n",datatype->type);
		} else
		if(0 == strcmp(dtchild->Name(),"SubIdx")) {
//			datatype->subindex = dtchild->IntText();
			datatype->subindex = (hexdecstr2uint32(dtchild->GetText()) & 0xFF);
			if(very_verbose) printf("DataType/SubIdx: '%d'\n",datatype->subindex);
		} else
		if(0 == strcmp(dtchild->Name(),"BitSize")) {
			datatype->bitsize = dtchild->IntText();
			if(very_verbose) printf("DataType/BitSize: '%d'\n",datatype->bitsize);
		} else
		if(0 == strcmp(dtchild->Name(),"BitOffs")) {
			datatype->bitoffset = dtchild->IntText();
			if(very_verbose) printf("DataType/BitOffs: '%d'\n",datatype->bitoffset);
		} else
		if(0 == strcmp(dtchild->Name(),"BaseType")) {
			datatype->basetype = dtchild->GetText();
			if(very_verbose) printf("DataType/BaseType: '%s'\n",datatype->basetype);
		} else
		if(0 == strcmp(dtchild->Name(),"ArrayInfo")) {
			ArrayInfo* arrinfo = new ArrayInfo;
			for (const tinyxml2::XMLElement* arrchild = dtchild->FirstChildElement();
				arrchild != 0; arrchild = arrchild->NextSiblingElement())
			{
				if(0 == strcmp(arrchild->Name(),"LBound")) {
					arrinfo->lowerbound = arrchild->IntText();
					if(very_verbose) printf("DataType/ArrayInfo/LBound: '%d'\n",arrinfo->lowerbound);
				} else
				if(0 == strcmp(arrchild->Name(),"Elements")) {
					arrinfo->elements = arrchild->IntText();
					if(very_verbose) printf("DataType/ArrayInfo/Elements: '%d'\n",arrinfo->elements);
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
			printf("Unhandled Device/Profile/DataTypes element: '%s' = '%s'\n",dtchild->Name(),dtchild->GetText());
		}
	}

	// TODO: Improve the var/record/array stuff...
	if(NULL != parent) {
/*		if(NULL != datatype->type) {
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
		if(NULL == datatype->type) datatype->type = parent->type;*/
		if(NULL == datatype->flags) datatype->flags = parent->flags;
		parent->subitems.push_back(datatype);
	} else {
		dict->datatypes.push_back(datatype);
	}
}

void ESIXML::parseXMLProfile(const tinyxml2::XMLElement* xmlprofile, Device *dev) {
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

void ESIXML::parseXMLDevice(const tinyxml2::XMLElement* xmldevice) {
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
			if(verbose) printf("Device/Name: '%s'\n",dev->name);
		} else
		if(0 == strcmp(child->Name(),"Type")) {
			dev->type = child->GetText();
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"ProductCode")) {
					dev->product_code = EC_SII_HexToUint32(attr->Value());
					if(verbose) printf("Device/Type/@ProductCode: 0x%.08X\n",dev->product_code);
				} else
				if(0 == strcmp(attr->Name(),"RevisionNo")) {
					dev->revision_no = EC_SII_HexToUint32(attr->Value());
					if(very_verbose) printf("Device/Type/@RevisionNo: 0x%.08X\n",dev->revision_no);
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
			if(very_verbose) printf("Device/Fmmu: %s\n",fmmu->type);
			for (const tinyxml2::XMLAttribute* attr = child->FirstAttribute();
				attr != 0; attr = attr->Next())
			{
				if(0 == strcmp(attr->Name(),"Sm")) {
//					fmmu->syncmanager = attr->IntValue();
					fmmu->syncmanager = (int32_t)hexdecstr2uint32(attr->Value());
				} else
				if(0 == strcmp(attr->Name(),"Su")) {
//					fmmu->syncunit = attr->IntValue();
					fmmu->syncunit = (int32_t)hexdecstr2uint32(attr->Value());
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
					if(very_verbose) {
						printf("Device/Eeprom/ConfigData: ");
						for(uint8_t i = 0; i < EC_SII_CONFIGDATA_SIZEB; ++i) {
							if(i == EC_SII_CONFIGDATA_SIZEB-1) printf("%.02X",dev->configdata[i]);
							else  printf("%.02X ",dev->configdata[i]);
						}
						printf("\n");
					}
				} else
				if(0 == strcmp(eepchild->Name(),"ByteSize")) {
					dev->eepromsize = eepchild->UnsignedText();
					if(very_verbose) printf("Device/Eeprom/ByteSize: %u\n",dev->eepromsize);
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

void ESIXML::parseXMLVendor(const tinyxml2::XMLElement* xmlvendor) {
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

void ESIXML::parseXMLElement(const tinyxml2::XMLElement* element, void* data) {
	if(NULL == element) return;
/*
	printf("Element name: '%s'\n",element->Name());
	if(element->GetText()) printf("Element text: '%s'\n",element->GetText());
	for (const tinyxml2::XMLAttribute* attr = element->FirstAttribute();
		attr != 0; attr = attr->Next())
	{
		printf("Attribute: '%s' = '%s'\n",attr->Name(),attr->Value());
	}
*/
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
			if(!child->NoChildren()) parseXMLElement(child);
			else printf("Unhandled element '%s'\n",child->Name());
		}
	}
	return;
}