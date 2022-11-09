#include "sii.h"

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
#include "esctoolhelpers.h"

void SII::decodeEEPROMBinary(const std::string& file, const bool verbose) {
	int fd = open(file.c_str(), O_RDONLY);
	if(fd < 0){
		printf("Could not open '%s'\n",file.c_str());
		return;
	}
	printf("Decoding SII from '%s'\n",file.c_str());
	struct stat statbuf;
	int err = fstat(fd, &statbuf);
	if(err < 0){
		printf("Could not stat '%s'\n",file.c_str());
		return;
	}

	uint8_t *p = (uint8_t*) mmap(NULL, statbuf.st_size,
		PROT_READ, MAP_PRIVATE, fd, 0);
	uint8_t *ptr = p;
	if(ptr == MAP_FAILED){
		printf("Mapping Failed (%d)\n",errno);
		return;
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

	if(err != 0) printf("UnMapping Failed (%d)\n",errno);
	return;
}