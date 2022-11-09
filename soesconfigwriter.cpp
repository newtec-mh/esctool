#include "soesconfigwriter.h"
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "esctoolhelpers.h"
#include "esctool.h"

#define SOES_DEFAULT_BUFFER_PREALLOC_FACTOR 3
std::string objectdictfile	= "objectlist.c";
std::string utypesfile		= "utypes.h";
std::string ecatconfig		= "ecat_options.h";

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

SOESConfigWriter::SOESConfigWriter(bool input_endianness_is_little) :
	m_input_endianness_is_little(input_endianness_is_little) {};
SOESConfigWriter::~SOESConfigWriter() {};

void SOESConfigWriter::writeSSCFiles(Device* dev) {
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
	if(dev->profile && dev->profile->dictionary) {
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

			auto writeObject = [this,&out](Object* obj, Object* parent, int& subitem, const int nitems, Dictionary* dict = NULL) {
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
							if(m_input_endianness_is_little) {
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
	} else {
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
};