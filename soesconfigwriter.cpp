#include "soesconfigwriter.h"
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "esctoolhelpers.h"
#include "esctool.h"
#include "esctooldefs.h"

#define SOES_DEFAULT_BUFFER_PREALLOC_FACTOR 3
std::string objectdictfile	= "objectlist.c";
std::string utypesfile		= "utypes.h";
std::string ecatconfig		= "ecat_options.h";
std::string sm2mappings_str	= "SM2_MAPPINGS";
std::string sm3mappings_str	= "SM3_MAPPINGS";

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

void printFlags (uint16_t index, uint8_t subindex, const ObjectFlags* f) {
	printf("0x%.4X:%.2X Flags: '%s'\n",index,subindex,f->category ? f->category : "(No category)");
	if(f->access) {
		if("Access: '%s'\n",f->access->access ? f->access->access : "(none)");
	}
};

SOESConfigWriter::SOESConfigWriter(const std::string& outdir, bool input_endianness_is_little) :
	m_outputdir(outdir),
	m_input_endianness_is_little(input_endianness_is_little) {};

SOESConfigWriter::~SOESConfigWriter() {};

void SOESConfigWriter::writeSSCFiles(Device* dev, OutputParams params) {
	uint16_t dynrxpdo = 0;
	uint16_t dyntxpdo = 0;
	for(Pdo* pdo : dev->rxpdo) if(!pdo->fixed) ++dynrxpdo;
	for(Pdo* pdo : dev->txpdo) if(!pdo->fixed) ++dyntxpdo;

	// TODO: If slots are predefined and fixed, they're not dynamic...
	if(NULL != dev->slots) dynrxpdo += dev->slots->maxslotcount;
	if(NULL != dev->slots) dyntxpdo += dev->slots->maxslotcount;

	std::ofstream configout;
	configout.open((m_outputdir + ecatconfig).c_str(), std::ios::out | std::ios::trunc);
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

		auto calculatePDOSize = [] (std::list<Pdo*>& pdoList, const int syncmanager) {
			uint16_t pdoSize = 0;
			for(Pdo* pdo : pdoList) {
				if(syncmanager == pdo->syncmanager) {
					for(PdoEntry* entry : pdo->entries) {
						pdoSize += entry->bitlen;
					}
				}
			}
			return (pdoSize & 0xF ? 1 : 0) + (pdoSize >> 3); // Divide bitsize by 8 + 1 for remainder
		};

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
				uint16_t calculatedSize = calculatePDOSize(dev->rxpdo,2);
				if(NULL != dev->slots) {
					if(dev->modules != NULL) {
						// TODO, go through each slot (if in the list) and check for supported ModuleIdents
						int largest = 0;
						for(auto m : *(dev->modules)) {
							largest = std::max(calculatePDOSize(m->rxpdo,2),largest);
						}
						printf("Largest module RXPDO is '%d' bytes\n",largest);
						calculatedSize += dev->slots->maxslotcount * largest;
					}
				}
				if(0 == sm->defaultsize) {
					printf("\033[0;32mCalculated size of RXPDO\033[0m: %d bytes\n",calculatedSize);
					sm->defaultsize = calculatedSize;
				} else if(sm->defaultsize != calculatedSize) {
					printf("\033[0;31mWARNING\033[0m: Calculated PDO output size %d does not match decoded size %d\n",calculatedSize,sm->defaultsize);
				}
				configout << "#define SM2_sma          " << "0x" << std::hex << sm->startaddress << "\n";
				configout << "#define SM2_smc          " << "0x" << std::hex << (uint32_t) sm->controlbyte << "\n";
				configout << "#define SM2_act          " << (sm->enable ? 1 : 0) << "\n";
				configout << "#define MAX_RXPDO_SIZE   " << std::dec << sm->defaultsize << "\n";
				configout << "#define MAX_MAPPINGS_SM2 " << std::dec << dynrxpdo << "\n";
				configout << "\n";
			} else
			if(0 == strcmp(sm->type,"Inputs")) { // TODO verify that the actual assigned SyncManager *is* 3
				uint16_t calculatedSize = calculatePDOSize(dev->txpdo,3);
				if(NULL != dev->slots) {
					if(dev->modules != NULL) {
						// TODO, go through each slot (if in the list) and check for supported ModuleIdents
						int largest = 0;
						for(auto m : *(dev->modules)) {
							largest = std::max(calculatePDOSize(m->txpdo,3),largest);
						}
						printf("Largest module TXPDO is '%d' bytes\n",largest);
						calculatedSize += dev->slots->maxslotcount * largest;
					}
				}
				if(0 == sm->defaultsize) {
					sm->defaultsize = calculatedSize;
					printf("\033[0;32mCalculated size of TXPDO\033[0m: %d bytes\n",calculatedSize);
				} else if(sm->defaultsize != calculatedSize) {
					printf("\033[0;31mWARNING\033[0m: Calculated PDO output size %d does not match decoded size %d\n",calculatedSize,sm->defaultsize);
				}
				configout << "#define SM3_sma          " << "0x" << std::hex << sm->startaddress << "\n";
				configout << "#define SM3_smc          " << "0x" << std::hex << (uint32_t) sm->controlbyte << "\n";
				configout << "#define SM3_act          " << (sm->enable ? 1 : 0) << "\n";
				configout << "#define MAX_TXPDO_SIZE   " << std::dec << sm->defaultsize << "\n";
				configout << "#define MAX_MAPPINGS_SM3 " << std::dec << dyntxpdo << "\n";
				configout << "\n";
			}
		}

		configout << "\n";
		configout << "#endif /* __ECAT_OPTIONS_H__ */\n";
		configout.sync_with_stdio();
		configout.close();
	}

	auto findDT = [dict=dev->profile->dictionary](const char* dtname) {
		if(NULL == dtname) return (DataType*)NULL;
		for(DataType* d : dict->datatypes) {
//			if(0 == strcmp(UDINTstr,dtname)) { printf("CHecking UDINT vs '%s'\n",d->name); }
			if(0 == strcmp(d->name,dtname)) return d;
		}
		return (DataType*)NULL;
	};

	auto deduceDT = [dict=dev->profile->dictionary,&findDT](Object* obj, const int subitemNo) {
//		printf("DeduceDT: %.04X:%.02X type: '%s', datatype: '%s'\n",
//			obj->index,subitemNo,obj->type?obj->type:"(null)",obj->datatype?obj->datatype->type:"(null)");
		const char* type = NULL;
		DataType* dt = obj->datatype ? obj->datatype : findDT(obj->type);
		if(dt != NULL) type = dt->type;

		if(dt != NULL && (dt->subitems.size() > 1 && dt->subitems[1]->subindex == 0))
		{
//			printf("DeduceDT: %.04X:%.02X is an array\n", obj->index,subitemNo);
			// DataType is an array
			dt = findDT(dt->subitems[1]->type);
			if(NULL != dt && dt->arrayinfo) {
				type = dt->basetype;
				dt = findDT(type);
				if(!dt) {
					printf("\033[0;31mWARNING:\033[0m DataType of object '0x%.04X' subitem '%u' seems to be array, but basetype DataType was not found\n",obj->index,subitemNo);
				}
			} else {
				printf("\033[0;31mWARNING:\033[0m DataType of object '0x%.04X' subitem '%u' seems to be array, but no arrayinfo found\n",obj->index,subitemNo);
			}
		}

		if(NULL == dt) {
			dt = findDT(obj->type != NULL ? obj->type : (obj->parent ? obj->parent->type : NULL));
		}
		if(NULL == type && NULL != dt) {
			try {
				dt = dt->subitems.at(subitemNo);
				type = dt->type;
			} catch(const std::out_of_range&) {
				type = NULL;
			}
		}
		if(NULL == type && NULL != dt) {
			if(!dt->type) type = dt->name;
			else type = dt->type;
		}
		if(NULL == type) type = obj->type; // Fallback
		return dt;
	};

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
		if(0 == strcmp(type,ULINTstr)) {
			return "uint64_t";
		}
		printf("Warning: Unable to find C-type for '%s'\n",type);
		return (const char*)NULL;
	};

	auto isArray = [&](Object* o) {
		DataType* dt = o->datatype;
		if(dt == NULL) {
			dt = findDT(o->type);
			if(dt->subitems.size() > 0) {
				if(dt->subitems[1]->subindex == 0) {
					return true;
				}
			}
		}
		return dt->arrayinfo != NULL;
	};

	if(dev->profile && dev->profile->dictionary) {
		std::ofstream typesout;
		typesout.open((m_outputdir + utypesfile).c_str(), std::ios::out | std::ios::trunc);
		if(!typesout.fail()) {
			printf("Writing SOES compatible type definitions to '%s'\n",utypesfile.c_str());
			typesout << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n\n";
			typesout << "#ifndef __UTYPES_H__\n";
			typesout << "#define __UTYPES_H__\n\n";
			typesout << "#include <stdint.h>\n";
			typesout << "\n";

			/** Write struct(s) to hold the mapped object "references"*/
			if(dynrxpdo) {
				typesout << "/** When using dynamic RXPDOs remember to initialize max_subindex and so on manually */\n";
				typesout << "typedef struct {\n";
				typesout << "\tuint8_t max_subindex;\n";
				typesout << "\tuint16_t subindex[" << std::dec << (int)dynrxpdo << "];" << " /* 0x1600-0x" << std::hex << std::uppercase << (0x1600 + dynrxpdo) << " */\n";
				typesout << "} _" << sm2mappings_str << ";\n\n";
				typesout << "extern _" <<  sm2mappings_str << " " << sm2mappings_str << ";\n\n";
			}

			if(dyntxpdo) {
				typesout << "/** When using dynamic TXPDOs remember to initialize max_subindex and so on manually */\n";
				typesout << "typedef struct {\n";
				typesout << "\tuint8_t max_subindex;\n";
				typesout << "\tuint16_t subindex[" << std::dec << (int)dyntxpdo << "];" << " /* 0x1A00-0x" << std::hex << std::uppercase << (0x1A00 + dyntxpdo) << " */\n";
				typesout << "} _" << sm3mappings_str << ";\n\n";
				typesout << "extern _" <<  sm3mappings_str << " " << sm3mappings_str << ";\n\n";
			}

			for(Object* o : dev->profile->dictionary->objects) {
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
						DataType* dt = deduceDT(si,subitem);
						const char* type = dt->type;
						if(NULL == type) {
							printf("WARNING: Could not determine C-datatype for '%s':'%s' ('%s')\n",o->name,si->name,(si->datatype?si->datatype->name:o->type));
							continue;
						}
						typesout << "\t";
						typesout << getCType(type);
						typesout << " ";
						typesout << CNameify(si->name,params.capitalizeStructMembers);
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
						typesout << " */\n";
						++subitem;
					}
					typesout << "} _" << CNameify(o->name,true) << ";\n\n";
					typesout << "extern _";
					typesout << CNameify(o->name,true);
					typesout << " ";
					typesout << CNameify(o->name,true);
					if(params.appendObjectIndexToStructs) {
						typesout << "0x";
						typesout << std::uppercase
							 << std::hex
							 << std::setfill('0')
							 << std::setw(4)
							 << index;
					}	
					typesout << ";\n\n";
				} else {
					const char* type = o->datatype ? 
						(o->datatype->type ? o->datatype->type :
							o->datatype->name) :
						o->type;
					typesout << "extern";
					typesout << " ";
					typesout << getCType(type);
					typesout << " ";
					typesout << CNameify(o->name,params.capitalizeStructMembers);
					if(params.appendObjectIndexToStructs) {
						typesout << "0x";
						typesout << std::uppercase
							 << std::hex
							 << std::setfill('0')
							 << std::setw(4)
							 << index;
					}	
					typesout << ";\n\n";
				}
			}
			typesout << "#endif /* UTYPES_H */\n";
			typesout.sync_with_stdio();
			typesout.close();
		} else {
			printf("Couln't open '%s' for writing\n",utypesfile.c_str());
		}

		if(dev->modules) {
			std::string modulesfile = "modules.h";
			std::ofstream out;
			out.open((m_outputdir + modulesfile).c_str(), std::ios::out | std::ios::trunc);
			printf("Writing module type definitions to '%s'\n",modulesfile.c_str());
			out << "/** Autogenerated by " << APP_NAME << " v" << APP_VERSION << " */\n"
			    << "#ifndef __" << CNameify(dev->name,true) << "_MODULES_H__\n"
			    << "#define __" << CNameify(dev->name,true) << "_MODULES_H__\n"
			    << "#include <stddef.h>\n"
			    << "#include <stdint.h>\n"
			    << "\n";

			out << "#define MODULE_SLOT_INDEX_INCREMENT\t\t(" << (int)(dev->slots->slotindexincrement) << ")\n"
			    << "#define MODULE_SLOT_PDO_INCREMENT\t\t(" << (int)(dev->slots->slotpdoincrement) << ")\n"
			    << "\n";

			for(Module* mod : *(dev->modules)) {
				out << "#define " << CNameify(mod->type,true) << "_IDENT" << "\t\t(" << (int)(mod->ident) << ")\n";
				out << "typedef struct " << CNameify(mod->type,params.capitalizeStructMembers) << " {\n";
				out << std::hex;
				for(Pdo* p : mod->rxpdo) {
					out << "\t/* RXPDO @ 0x" << std::uppercase << p->index << " */\n";
					if(p->dependonslot) {
						out << "\t/* Index is slot dependent: real-index = index + (slot * MODULE_SLOT_INDEX_INCREMENT) */\n";
					}
					for(PdoEntry* e : p->entries) {
						out << "\t"
						    << getCType(e->datatype)
						    << " "
						    << CNameify(e->name,params.capitalizeStructMembers)
						    << "; /* "
						    << std::setfill('0')
						    << std::setw(4)
						    << p->index
						    << "."
						    << std::setw(2)
						    << e->subindex
						    << " */\n";
					}
					if(p != mod->rxpdo.back()) out << "\n";
				}
				if(!mod->rxpdo.empty() && !mod->txpdo.empty()) out << "\n";
				for(Pdo* p : mod->txpdo) {
					out << "\t/* TXPDO @ 0x" << std::uppercase << p->index << " */\n";
					if(p->dependonslot) {
						out << "\t/* Index is slot dependent: real-index = index + (slot * MODULE_SLOT_INDEX_INCREMENT) */\n";
					}
					for(PdoEntry* e : p->entries) {
						out << "\t"
						    << getCType(e->datatype)
						    << " "
						    << CNameify(e->name,params.capitalizeStructMembers)
						    << "; /* "
						    << std::setfill('0')
						    << std::setw(4)
						    << p->index
						    << "."
						    << std::setw(2)
						    << e->subindex
						    << " */\n";
					}
					if(p != mod->txpdo.back()) out << "\n";
				}
				out << std::dec;
				out << "} " << CNameify(mod->type,params.capitalizeStructMembers) << "_t;\n\n";
			}
			out << "#endif /* __" << CNameify(dev->name,true) << "_MODULES_H__ */\n";
			out.close();
		}

		std::ofstream out;
		out.open((m_outputdir + objectdictfile).c_str(), std::ios::out | std::ios::trunc);
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

			auto writeObject = [this,&out,&findDT,&deduceDT,&dynrxpdo,&dyntxpdo,&params]
				(Object* obj, Object* parent, int& subitem, const int nitems, Dictionary* dict = NULL)
			{
				bool objref = false;
				out << "{ 0x"
					<< std::setw(2)
					<< subitem
					<< ", ";

				uint16_t index = obj->index & 0xFFFF;

				DataType* datatype = obj->datatype ? obj->datatype : deduceDT(obj,subitem);

				const char* type = datatype ? datatype->type ? datatype->type : datatype->name : NULL;

				if(!datatype && subitem == 0) type = USINTstr; // TODO: FIXME?

				const ObjectFlags* flags = obj->flags ? obj->flags : (datatype ? datatype->flags : NULL);

				uint32_t bitsize = obj->bitsize ? obj->bitsize : (datatype ? datatype->bitsize : 0);

				if(NULL == type) {
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
					if(0 == strcmp(type,ULINTstr)) {
						out << "DTYPE_UNSIGNED64";
						bitsize = 64;
					} else
					{ // TODO handle more types?
						printf("\033[0;31mWARNING:\033[0m %.04X:%.02X Unhandled Datatype '%s'\n",
							obj->index,subitem,obj->type);
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
					if(flags->access->writerestrictions &&
						0 == strcmp(flags->access->writerestrictions,"PreOP"))
					{
						out << "pre";
					}
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
						if(0x1c12 == index && 0 != dynrxpdo) {
							if(0 == subitem) {
								out << "0x00, "
								    << "&(" << sm2mappings_str << ".max_subindex) }";
							} else {
								out << "0x0000, "
								    << "&(" << sm2mappings_str << ".subindex["
								    << std::dec
								    << (subitem - 1)
								    << "]) }";
							}
						} else if(0x1c13 == index && 0 != dyntxpdo) {
							if(0 == subitem) {
								out << "0x00, "
								    << "&(" << sm3mappings_str << ".max_subindex) }";
							} else {
								out << "0x0000, "
								    << "&(" << sm3mappings_str << ".subindex["
								    << std::dec
								    << (subitem - 1)
								    << "]) }";
							}
						} else if(NULL != obj->defaultdata) {
							// TODO: can we assume data is hex or smth? HexDecStr...
							if(strncmp(obj->defaultdata,"0x",2))
								out << "0x";
							out << std::hex;
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
							out << CNameify(obj->name,params.capitalizeStructMembers);
							if(params.appendObjectIndexToStructs) {
								out << "0x"
								<< std::hex
								<< std::setfill('0')
								<< std::setw(4)
								<< std::uppercase
								<< index;
							}
							out << " }";
						} else {
							out << "0, ";
							out << "&(";
							out << CNameify(parent->name,true);
							if(params.appendObjectIndexToStructs) {
								out << "0x"
								<< std::hex
								<< std::setfill('0')
								<< std::setw(4)
								<< std::uppercase
								<< index;
							}
							out << ".";
							out << CNameify(obj->name,params.capitalizeStructMembers);
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
				out << "[] = {\n";
				if(0x1C12 == o->index && dev->slots) {
					// If we have slots, we will (most likely?) have dynamic mappable PDOs
					// According to ETG5001 the module PDOs for modules should be in 0x1600-0x16FF
					// Thus we assume any device specific PDOs will be in 0x1700 and above
					// Above we made the SM[2/3]_MAPPING types. We will refer them here.
				}
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

			out << "const _objectlist objlist_end = { 0xFFFF, 0xFF, 0xFF, 0xFF, NULL, NULL };\n";
			out << "\n";
			out << "const _objectlist SDOobjects[] = {\n";
			for(Object* o : dev->profile->dictionary->objects) {
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
					if(isArray(o)) {
						printf("%04X is OTYPE_ARRAY ('%s')\n",index,o->type);
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
			out << "objlist_end };\n";
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
			typesout << "#ifndef __UTYPES_H__\n";
			typesout << "#define __UTYPES_H__\n\n";
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