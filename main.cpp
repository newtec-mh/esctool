/**
 * @file main.cpp
 * 
 * @author Martin Hejnfelt (mh@newtec.dk)
 * @brief EtherCAT Slave Controller tool
 * @version 0.2
 * @date 2025-02-04
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

#include "tinyhttp/http.hpp"

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
bool capitalizeStructMembers = false;
bool indexPostfixStructs = false;

// Decide if input from XML should be treated as LE
bool input_endianness_is_little = false;
std::string catalogFile("");

void printUsage(const char* name) {
	printf("Usage: %s [options] --input/-i <input-file>\n",name);
	printf("Options:\n");
	printf("\t --decode : Decode and print a binary SII file\n");
	printf("\t --verbose/-v : Flood some more information to stdout when applicable\n");
	printf("\t --nosii/-n : Don't generate SII EEPROM binary (only for !--decode)\n");
	printf("\t --dictionary/-d : Generate SSC object dictionary (default if --nosii and !--decode)\n");
	printf("\t --index-postfix-structs/-ips : Append object index to structs, eg. OUTPUTS becomes OUTPUTS0x7000.\n");
	printf("\t --capitalize-struct-members/-csm : Make member names in structs all capitalizes eg. OUTPUTS.outputs0 becomes OUTPUTS.OUTPUTS0.\n");
	printf("\t --encodepdo/-ep : Encode PDOs to SII EEPROM\n");
	printf("\t --output-directory/-odir : Specify output directory (created if non-existant)\n");
	printf("\t --output/-o : Specify output SII filename\n");
	printf("\t --catalog/-c : Specify device catalog file explicitly (default: esctool.json)\n");
	printf("\n");
}

void printObject (Object* o, unsigned int level = 0) {
//	printf("Obj: Index: 0x%.04X, Name: '%s'\n",(NULL != o->index ? EC_SII_HexToUint32(o->index) : 0),o->name);
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("Obj: Index: 0x%.04X, Name: '%s', DefaultData: '%s', BitSize: '%u'\n",o->index,o->name,o->defaultdata,o->bitsize);
	if(very_verbose && o->flags) {
		for(unsigned int l = 0; l < level; ++l) printf("\t");
		printf("Flags: '%s'\n",o->flags->category ? o->flags->category : "(No category)");
		if(o->flags->access) {
			if("Access: '%s'\n",o->flags->access->access ? o->flags->access->access : "(none)");
		} else printf("No access\n");
	}
	for(Object* si : o->subitems) printObject(si,level+1);
};

void printDataType (DataType* dt, unsigned int level = 0) {
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("DataType: Name: '%s', Type: '%s'\n",dt->name,dt->type);
	for(DataType* dsi : dt->subitems) printDataType(dsi,level+1);
};

void printDataTypeVerbose (DataType* dt, unsigned int level = 0) {
	if(level == 0) printf("-----------------\n");
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("DataType: ");
	printf("Name: '%s' ",dt->name);
	printf("Type: '%s' ",dt->type);
	printf("BaseType: '%s' ",dt->basetype);
	printf("BitSize: '%d' ",dt->bitsize);
	printf("BitOffset: '%d' ",dt->bitoffset);
	printf("SubIndex: '%d' ",dt->subindex);
	printf("SubItems: '%lu' ",dt->subitems.size());
	printf("ArrayInfo: '%s'",dt->arrayinfo ? "yes" : "no");
	if(dt->arrayinfo && very_verbose) {
		printf(" [ ");
		printf("Elements: '%d' ",dt->arrayinfo->elements);
		printf("LowerBound: '%d' ",dt->arrayinfo->lowerbound);
		printf("]\n");
	} else {
		printf("\n");
	}
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("Flags: '%s'",dt->flags ? "yes" : "none");
	if(dt->flags && very_verbose) {
		printf(" [ ");
		if(dt->flags->category) printf("Category: '%s' ",dt->flags->category);
		if(dt->flags->pdomapping) printf("PdOMapping: '%s' ",dt->flags->pdomapping);
		if(dt->flags->access) {
			if(dt->flags->access->access) printf("Access: '%s' ",dt->flags->access->access);
		}
		printf("]\n");
	} else {
		printf("\n");
	}
	for(DataType* si : dt->subitems) {
		for(unsigned int l = 0; l < level; ++l) printf("\t");
		printf("SubItem:\n");
		printDataTypeVerbose(si,level+1);
	}
	if(level == 0) printf("-----------------\n");
};

int encodeSII(const std::string& inputfile, std::string output = "", const std::string& outdir = "") {
	ESIXML esixml((verbose ? 0x1 : 0x0) + (very_verbose ? 0x2 : 0x0));
	esixml.parse(inputfile);

	if(!esixml.getDevices().empty()) {

		// TODO check mandatory items
		// Group Name
		// Device Name
		// ...
		Device* dev = esixml.getDevices().front();

		// Create a boilerplate object dictionary if nothing exists and CoE is enabled
		if(writeobjectdict && dev->mailbox && dev->mailbox->coe_sdoinfo)
		{
			printf("Verifying and/or creating minimal object dictionary...\n");

			if(!dev->profile) dev->profile = new Profile;
			if(!dev->profile->dictionary) {
				printf("\033[0;31mWARNING:\033[0m Creating empty dictionary\n");
				dev->profile->dictionary = new Dictionary;
			}

			Dictionary* dict = dev->profile->dictionary;
			auto findDT = [&dict](const char* dtname, uint32_t bitsize) {
				for(DataType* d : dict->datatypes)
					if(0 == strcmp(d->name,dtname)) return d;
				printf("Creating DataType '%s' (%d bits)\n",dtname,bitsize);
				dict->datatypes.push_back(new DataType {
					.name = dtname,
					.bitsize = bitsize
				});
				return dict->datatypes.back();
			};

			DataType* DT_UDINT = findDT(UDINTstr,32);
			DataType* DT_UINT = findDT(UINTstr,16);
			DataType* DT_USINT = findDT(USINTstr,8);
			DataType* DT_DINT = findDT(DINTstr,32);
			DataType* DT_INT = findDT(INTstr,16);
			DataType* DT_SINT = findDT(SINTstr,8);
			DataType* DT_ULINT = findDT(ULINTstr,64);
			
			size_t L = 32;
			char s[L];

			auto createStr = [&s]() {
				char* newStr = new char[strlen(s)+1];
				strcpy(newStr,s);
				m_customStr.push_back(newStr);
				return newStr;
			};

			auto hasObject = [&dict](uint16_t index) {
				for(Object* o : dict->objects) {
					if(o->index == index) return o;
				}
				return (Object*)NULL;
			};

			if(!hasObject(0x1000)) {
				if(dev->profile->channelinfo && dev->profile->channelinfo->profileNo) {
					snprintf(s,L,"%.08u",dev->profile->channelinfo->profileNo);
				} else {
					snprintf(s,L,"%s","00001389"); // Hex representation of 5001
				}

				dict->objects.push_back(new Object {
					.index = 0x1000,
					.name = devTypeStr,
					.datatype = DT_UDINT,
					.defaultdata = createStr()
				});
			}

			if(!hasObject(0x1008)) {
				snprintf(s,L,"STRING(%lu)",strlen(dev->name));

				dict->objects.push_back(new Object {
					.index = 0x1008,
					.name = devNameStr,
					.type = createStr(),
					.defaultstring = dev->name
				});
			}

			// RX/TXPDO mapping
			for(auto pdoList : { dev->rxpdo, dev->txpdo }) {
				int pdoDefNo = 0;
				for(Pdo* pdo : pdoList) {
					Object* pdo_obj = new Object;
					pdo_obj->index = pdo->index;
					if(pdo->index >= 0x1600 && pdo->index < 0x1A00) {
						snprintf(s,L,"RXPDO %.02d",pdoDefNo++);
					} else {
						snprintf(s,L,"TXPDO %.02d",pdoDefNo++);
					}
					pdo_obj->name = createStr();
					uint32_t rxsize_bytes = 0;

					DataType* dt = NULL;
					snprintf(s,L,"DT%.04X",pdo->index);

					for(DataType* d : dict->datatypes) {
						if(0 == strcmp(d->name,s)) {
							if(verbose) printf("Found datatype '%s' in dictionary!\n",s);
							dt = d;
							break;
						}
					}
					if(dt != NULL) continue;
					printf("Generating datatype '%s'\n",s);

					dt = new DataType;
					dt->name = createStr();
					printf("%s\n",dt->name);
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
						numberOfEntries_obj->bitsize = sdt->bitsize;
						dt->bitsize += numberOfEntries_obj->datatype->bitsize;
						dt->bitsize += numberOfEntries_obj->datatype->bitsize; // FIXME this is padding (set to 16 instead?)

						snprintf(s,L,"%.02X",(uint32_t)(pdo->entries.size() & 0xFF));
						char* numberOfEntriesVal = createStr();
						numberOfEntries_obj->defaultdata = numberOfEntriesVal;
						pdo_obj->subitems.push_back(numberOfEntries_obj);

						for(PdoEntry* e : pdo->entries) {
							// Create the DataType subitem for current subindex
							sdt = new DataType;
							Object* pdoEntry_obj = new Object;
							snprintf(s,L,"SubIndex %.03d",e->subindex);
							char* entryName = createStr();
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
							pdoEntry_obj->datatype = DT_UDINT;

							snprintf(s,L,"%.04X%.02X%.02X",e->index,e->subindex,e->bitlen);
							char* defaultData = createStr();
							pdoEntry_obj->defaultdata = defaultData;

							pdo_obj->subitems.push_back(pdoEntry_obj);
							dt->subitems.push_back(sdt);
						}
					}
					dict->datatypes.push_back(dt);
					dict->objects.push_back(pdo_obj);
				}
			}

			auto createArrayDT = [&dict,&createStr,L,&s,&DT_USINT](uint16_t index, const int entries, DataType* entryDT) {
				snprintf(s,L,"DT%.04XARR",index);
				for(DataType* d : dict->datatypes) {
					if(0 == strcmp(d->name,s)) {
						if(verbose) printf("Found datatype '%s' in dictionary!\n",s);
						return d;
					}
				}
				printf("Generating datatype '%s'\n",s);
				DataType* dtARR = new DataType;
				dtARR->name = createStr();
				dtARR->basetype = entryDT->name;
				dtARR->bitsize = entries*(entryDT->bitsize);
				dtARR->arrayinfo = new ArrayInfo;
				dtARR->arrayinfo->elements = entries;
				dtARR->arrayinfo->lowerbound = 1;
				dict->datatypes.push_back(dtARR);
				DataType* dt = new DataType;
				snprintf(s,L,"DT%.04X",index);
				dt->name = createStr();
				dt->bitsize = dtARR->bitsize+DT_USINT->bitsize+8;
				dt->subitems.push_back(
					new DataType {
						.name = subIndex000Str,
						.type = DT_USINT->name,
						.bitsize = DT_USINT->bitsize,
						.subindex = 0 });
				dt->subitems.push_back(
					new DataType {
						.name = "Elements",
						.type = dtARR->name,
						.bitsize = dtARR->bitsize,
						.bitoffset = DT_USINT->bitsize+8 });
				dt->arrayinfo = dtARR->arrayinfo;
				dict->datatypes.push_back(dt);
				return dt;
			};

			if(!hasObject(0x1C00)) {
				// SyncManager types 0x1C00
				DataType* DT1C00 = createArrayDT(0x1C00,dev->syncmanagers.size(),DT_USINT);

				Object* x1C00 = new Object;
				x1C00->index = 0x1C00;
				x1C00->datatype = DT1C00;
				x1C00->bitsize = DT1C00->bitsize;
				x1C00->name = devSMTypeStr;

				snprintf(s,L,"%.02lu",dev->syncmanagers.size());

				x1C00->subitems.push_back(new Object {
						.index = x1C00->index,
						.name = subIndex000Str,
						.datatype = DT_USINT,
						.defaultdata = createStr()});

				uint8_t smno = 0;
				for(SyncManager* sm : dev->syncmanagers) {
					Object* sm_obj = new Object;
					sm_obj->index = x1C00->index;
					sm_obj->datatype = DT_USINT;
					snprintf(s,L,"SM%d type",smno);
					sm_obj->name = createStr();

					if(0 == strcmp(sm->type,"MBoxOut")) {
						snprintf(s,L,"%.02d",1);
					} else if(0 == strcmp(sm->type,"MBoxIn")) {
						snprintf(s,L,"%.02d",2);
					} else if(0 == strcmp(sm->type,"Outputs")) {
						snprintf(s,L,"%.02d",3);
					} else if(0 == strcmp(sm->type,"Inputs")) {
						snprintf(s,L,"%.02d",4);
					}

					sm_obj->defaultdata = createStr();
					sm_obj->bitsize = DT_USINT->bitsize;
					sm_obj->bitoffset = (smno * DT_USINT->bitsize);
					++smno;
					x1C00->subitems.push_back(sm_obj);
				}
				dict->objects.push_back(x1C00);
			}

			// SyncManager mappings 0x1C10-0x1C20
			// Add PDOs to each sync manager mapping, and create each SM's respective
			// objects
			std::vector<std::list<Pdo*> > syncManagerMappings = {{},{},{},{}};

			// TODO: If the device has slots, with predefined modules, with fixed
			// PDOs, we should go through these and add them

			// Go through the device PDOs
			for(auto pdoList : { dev->rxpdo, dev->txpdo }) {
				for(Pdo* pdo : pdoList)
					syncManagerMappings[pdo->syncmanager].push_back(pdo);
			}

			uint8_t smno = 0;
			for(auto pdoList : syncManagerMappings) {

				if(hasObject(0x1C10 + smno)) continue;

				Object* mappingObject = new Object;
				mappingObject->index = 0x1C10 + smno;
				DataType* DTmapping = createArrayDT(mappingObject->index,pdoList.size(),DT_UINT);
				mappingObject->datatype = DTmapping;

				snprintf(s,L,"SM%d mappings",smno);
				mappingObject->name = createStr();
				mappingObject->bitsize = 16; // size + padding

				snprintf(s,L,"%.02lu",pdoList.size());

				mappingObject->subitems.push_back(new Object {
						.index = mappingObject->index,
						.name = subIndex000Str,
						.datatype = DT_USINT,
						.defaultdata = createStr()});

				// The user will have adjust the maxsubindex in the SSC code
				// to deal with which modules are present and reflect
				// this properly
				if(dev->slots && (2 == smno || 3 == smno)) {
					// TODO: here we should really check through the slots to check for fixed PDOs
					ObjectFlags* flags = new ObjectFlags;
					flags->access = new ObjectAccess;
					snprintf(s,L,"%s","rw");
					flags->access->access = createStr();
					snprintf(s,L,"%s","PreOP");
					flags->access->writerestrictions = createStr();
					snprintf(s,L,"%s","Mapped object");
					const char* name = createStr();

					// We're now running with dynamic PDOs so the max subindex should be writeable
					mappingObject->subitems.front()->flags = flags;

					for(uint8_t j = 0; j < dev->slots->maxslotcount; ++j) {
						Object* mappedObj = new Object;
						mappedObj->index = mappingObject->index;
						mappedObj->datatype = DT_UINT;
						snprintf(s,L,"%.04X",0x0);
						mappedObj->defaultdata = createStr();
						mappedObj->name = name;
						mappedObj->bitoffset = mappingObject->bitsize;
						mappingObject->bitsize += DT_UINT->bitsize;
						mappedObj->bitsize = DT_UINT->bitsize;
						mappedObj->flags = flags;
						mappingObject->subitems.push_back(mappedObj);
					}
				}

				for(auto pdo : pdoList) {
					Object* mappedObj = new Object;
					mappedObj->index = mappingObject->index;
					mappedObj->datatype = DT_UINT;
					snprintf(s,L,"%.02X",pdo->index);
					mappedObj->defaultdata = createStr();
					mappedObj->name = pdo->name != NULL ? pdo->name : "Mapped object";
					mappedObj->bitoffset = mappingObject->bitsize;
					mappingObject->bitsize += DT_UINT->bitsize;
					mappedObj->bitsize = DT_UINT->bitsize;
					mappingObject->subitems.push_back(mappedObj);
				}

				dict->objects.push_back(mappingObject);
				++smno;
			}
		}

		auto findDT = [dict=dev->profile->dictionary](const char* dtname) {
			if(NULL == dtname) return (DataType*)NULL;
			for(DataType* d : dict->datatypes)
				if(0 == strcmp(d->name,dtname)) return d;
			printf("findDT: Could not find datatype for '%s'\n",dtname);
			return (DataType*)NULL;
		};

		for(DataType* datatype : dev->profile->dictionary->datatypes) {
			if(!datatype->subitems.empty() || datatype->arrayinfo) {
				uint32_t bitsize = 0;
				if(datatype->arrayinfo && NULL != datatype->basetype) {
					DataType* basedt = findDT(datatype->basetype);
					if(NULL != basedt) {
						bitsize = basedt->bitsize * datatype->arrayinfo->elements;
					}
				} else {
					int siNo = 0;
					for(DataType* dt : datatype->subitems) {
						if(siNo == 0) {
							bitsize += dt->bitsize;
							bitsize += 8; // Padding/16 bit alignment
						} else {
							DataType* basedt = findDT(dt->type);
							if(basedt && basedt->arrayinfo) {
								DataType* basedt = findDT(dt->type);
								if(NULL != basedt) {
									bitsize += basedt->bitsize;
								} else
									printf("\033[0;31mWARNING:\033[0m DataType '%s' Could not find array basetype '%s'\n",datatype->name,dt->type);
							} else
								bitsize += dt->bitsize;
						}
						++siNo;
					}
				}
				bitsize += bitsize%16; // 16 bit alignment
				if(datatype->bitsize != bitsize) {
					printf("\033[0;31mWARNING:\033[0m Bitsize of datatype '%s' seems off (calculated %d vs. parsed %d)\n",datatype->name,bitsize,datatype->bitsize);
					if(verbose) printDataTypeVerbose(datatype);
				}
			}
		}

		if(verbose) {
			printf("Profile: %s\n",dev->profile ? "yes" : "no");
			if(NULL != dev->profile) {
				printf("Dictionary: %s\n",dev->profile->dictionary ? "yes" : "no");
				if(NULL != dev->profile->dictionary && very_verbose) {
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

		// Sort objects by index...
		if(NULL != dev->profile && NULL != dev->profile->dictionary) {
			dev->profile->dictionary->objects.sort([](const Object* objA, const Object* objB) {
				return objA->index < objB->index;
			});
		}

		// Write SII EEPROM file
		if(!nosii) {
			if(0 == output.size())
				output = std::string(basename(inputfile.c_str())) + "_eeprom.bin";

			SII::encodeEEPROMBinary(esixml.getVendorID(),
				dev, encodepdo, inputfile, outdir,
				output, very_verbose);
		}

		// Write slave stack object dictionary
		if(writeobjectdict && NULL != dev->profile &&
		NULL != dev->profile->dictionary)
		{
			SOESConfigWriter sscwriter(outdir,input_endianness_is_little);
			sscwriter.writeSSCFiles(dev,{ .capitalizeStructMembers = capitalizeStructMembers, .appendObjectIndexToStructs = indexPostfixStructs });
		}

		// TODO: Delete it all...
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
	bool daemonize = false;
	std::string inputfile = "";
	std::string outputfile = "";
	std::string outdir = "";

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
		if(0 == strcmp(argv[i],"--capitalize-struct-members") ||
		   0 == strcmp(argv[i],"-csm"))
		{
			capitalizeStructMembers = true;
		} else
		if(0 == strcmp(argv[i],"--index-postfix-structs") ||
		   0 == strcmp(argv[i],"-ips"))
		{
			indexPostfixStructs = true;
		} else
		if(0 == strcmp(argv[i],"--output-directory") ||
		   0 == strcmp(argv[i],"-odir"))
		{
			outdir = argv[++i];
			if(outdir.at(outdir.size()-1) != '/') outdir += '/';
			if(outdir.at(0) != '/') {
				char cwd[PATH_MAX];
				if (getcwd(cwd, sizeof(cwd)) != NULL) {
					std::string outdir2(cwd);
					outdir2 += "/" + outdir;
					outdir = outdir2;
				} else {
					perror("getcwd() error");
					return errno;
				}
			}
			printf("Generating files to '%s'\n",outdir.c_str());
			struct stat st;
			if(stat(outdir.c_str(),&st) == 0) {
				if(!S_ISDIR(st.st_mode)) {
					printf("'%s' is not a directory\n",outdir.c_str());
					return -EINVAL;
				}
			} else {
				printf("Creating empty directory '%s'\n",outdir.c_str());
				if(mkdir(outdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
					printf("Failed creating '%s' (%d)\n",outdir.c_str(),errno);
					return errno;
				}
			}
		} else
		if(0 == strcmp(argv[i],"--output") ||
		   0 == strcmp(argv[i],"-o"))
		{
			outputfile = argv[++i];
		} else
		if(0 == strcmp(argv[i],"--catalog") ||
		   0 == strcmp(argv[i],"-c"))
		{
			catalogFile = argv[++i];
		} else
		if(0 == strcmp(argv[i],"--decode")) {
			decode = true;
			encode = false;
		} else
		if(0 == strcmp(argv[i],"--daemonize") ||
		   0 == strcmp(argv[i],"-D"))
		{
			printf("Daemonizing...\n");
			daemonize = true;
		}
	}
	if(encode && nosii && !writeobjectdict) {
		printf("Assuming Object Dictionary should be generated...\n");
		writeobjectdict = true;
	}
	printf("\n");

	if(daemonize) {
		HttpServer server;
		server.when("/")
			// Answer with a specific file
			->serveFile("./webui/index.html");
		server.whenMatching("/[^/]+")
			// Answer with a file from a folder
			->serveFromFolder("./webui/");

		server.when("/setup")
			// Handle when data is posted here (POST)
			->posted([](const HttpRequest& req) {
				std::string outfileName("");
				if(catalogFile != "") outfileName = catalogFile;
				else {
					outfileName = APP_NAME;
					outfileName += ".json";
				}
				std::ofstream jsonOut;
				jsonOut.open(outfileName.c_str(), std::ios::out | std::ios::trunc);
				jsonOut << req.content();
				jsonOut.close();
				printf("Wrote config to '%s'\n",outfileName.c_str());
				return HttpResponse{200};
			})
			// Handle when data is requested from here (GET)
			->requested([](const HttpRequest& req) {
				std::string inFilename("");
				if(catalogFile != "") inFilename = catalogFile;
				else {
					inFilename = APP_NAME;
					inFilename += ".json";
				}
				std::ifstream ist;
				ist.open(inFilename.c_str(),std::ios::in);
				if(!ist.is_open()) return HttpResponse{404};
				ist.seekg(0);
				std::stringstream setup("");
				setup << ist.rdbuf();
				if(ist.bad()) return HttpResponse{404};
				ist.close();
				return HttpResponse{200, "application/json", setup.str()};
			});

		server.whenMatching("/export/[^/]+")
			// Handle when data is posted here (POST)
			->posted([](const HttpRequest& req) {
				const char* devicename = basename(req.getPath().c_str());
				if(very_verbose) {
					printf("XML document:\n");
					printf("%s\n",req.content().c_str());
				}

				std::string outdir(devicename);
				outdir += "_out";

				if(outdir.at(outdir.size()-1) != '/') outdir += '/';
				if(outdir.at(0) != '/') {
					char cwd[PATH_MAX];
					if (getcwd(cwd, sizeof(cwd)) != NULL) {
						std::string outdir2(cwd);
						outdir2 += "/" + outdir;
						outdir = outdir2;
					} else {
						perror("getcwd() error");
						return HttpResponse{507};
					}
				}

				struct stat st;
				if(stat(outdir.c_str(),&st) == 0) {
					if(!S_ISDIR(st.st_mode)) {
						printf("'%s' is not a directory\n",outdir.c_str());
						return HttpResponse{507};
					}
				} else {
					if(verbose) printf("Creating empty directory '%s'\n",outdir.c_str());
					if(mkdir(outdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) {
						printf("Failed creating '%s' (%d)\n",outdir.c_str(),errno);
						return HttpResponse{507};
					}
				}

				std::ofstream xmlout;
				std::string outfile(outdir);
				outfile += devicename;
				outfile += ".xml";
				remove(outfile.c_str());
				xmlout.open(outfile.c_str(), std::ios::out | std::ios::trunc);
				xmlout << req.content();
				xmlout.close();

				writeobjectdict = true;
				std::string siiFile(devicename);
				siiFile += "_sii.bin";

				encodeSII(outfile,siiFile,outdir);
				printf("Wrote XML to '%s'\n",outfile.c_str());

				return HttpResponse{200};
			});

		printf("Starting server on 5001\n");
		server.startListening(5001);
		printf("Done...\n");
	} else {
		if("" == inputfile) {
			printUsage(argv[0]);
			return -EINVAL;
		}
		if(decode) {
			SII::decodeEEPROMBinary(inputfile,verbose);
		} else if(encode) {
			return encodeSII(inputfile,outputfile,outdir);
		}
	}

	return 0;
}