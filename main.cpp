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
#include "esixmlparsing.h"

std::vector<char*> m_customStr;

bool verbose = false;
bool very_verbose = false;
bool writeobjectdict = false;
bool nosii = false;
bool encodepdo = false; // Put PDOs in SII EEPROM

// Decide if input from XML should be treated as LE
bool input_endianness_is_little = false;

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

int encodeSII(const std::string& file, std::string output = "") {
	ESIXML esixml(verbose);
	esixml.parse(file);

	if(!esixml.getDevices().empty()) {
		// TODO check mandatory items
		// Group Name
		// Device Name
		// ...
		Device* dev = esixml.getDevices().front();
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
					printf("PDO Index: 0x%.04X has %lu entries\n",pdo->index,pdo->entries.size());
					for(PdoEntry* entry : pdo->entries) {
						printf("Entry: '%s', index: 0x%.04X, subindex: %u, datatype: '%s'\n",entry->name,entry->index,entry->subindex,entry->datatype);
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
						if(NULL == dt) printf("\n\nWARNING: DataType for entry '%s' (0x%.04X) is NULL (type: '%s')!\n\n",entry->name,entry->index,entry->datatype);
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
			SII::encodeEEPROMBinary(esixml.getVendorID(),
				dev, encodepdo, file, output, very_verbose);
		}

		// Write slave stack object dictionary
		if(writeobjectdict && NULL != dev->profile &&
		NULL != dev->profile->dictionary)
		{
			SOESConfigWriter sscwriter(input_endianness_is_little);
			sscwriter.writeSSCFiles(dev);
		}
	} else {
		printf("No devices could be parsed\n");
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