#include "esctoolhelpers.h"

uint8_t getCoEDataType(const char* dt) {
	if(0 == strcmp(dt,BOOLstr) || 0 == strcmp(dt,BITstr)) {
		return 0x01;
	} else if(0 == strcmp(dt,SINTstr)) {
		return 0x02;
	} else if(0 == strcmp(dt,INTstr)) {
		return 0x03;
	} else if(0 == strcmp(dt,DINTstr)) {
		return 0x04;
	} else if(0 == strcmp(dt,USINTstr)) {
		return 0x05;
	} else if(0 == strcmp(dt,UINTstr)) {
		return 0x06;
	} else if(0 == strcmp(dt,UDINTstr)) {
		return 0x07;
	} else if(0 == strcmp(dt,REALstr)) {
		return 0x08;
	}
	return 0x0;
};

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
};