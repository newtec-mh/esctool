#ifndef ESCTOOLHELPERS_H
#define ESCTOOLHELPERS_H
#include <cstdint>
#include <cstring>
#include "esidefs.h"

const char BOOLstr[]		= "BOOL";
const char BITstr[]		= "BOOL";
const char SINTstr[]		= "SINT";
const char INTstr[]		= "INT";
const char DINTstr[]		= "DINT";
const char USINTstr[]		= "USINT";
const char UINTstr[]		= "UINT";
const char UDINTstr[]		= "UDINT";
const char REALstr[]		= "REAL";
const char STRINGstr[]		= "STRING";
const char ULINTstr[]		= "ULINT";

const char devTypeStr[]		= "Device type";
const char devNameStr[]		= "Device name";
const char devHWVerStr[]	= "Hardware version";
const char devSWVerStr[]	= "Software version";
const char devIdentityStr[]	= "Identity";
const char devVendorIDStr[]	= "Vendor";
const char devProductCodeStr[]	= "Product code";
const char devRevisionStr[]	= "Revision";
const char devSerialNoStr[]	= "Serial number";
const char devSMTypeStr[]	= "SyncManager type";
const char numberOfEntriesStr[]	= "Number of entries";
const char subIndex000Str[]	= "SubIndex 000";

uint8_t getCoEDataType(const char* dt);
const char* getCategoryString(const uint16_t category);
unsigned char crc8(unsigned char* ptr, unsigned char len);

uint32_t hexdecstr2uint32(const char* s);
uint32_t EC_SII_HexToUint32(const char* s);

#endif /* ESCTOOLHELPERS_H */