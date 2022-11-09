#ifndef ESIDEFS_H
#define ESIDEFS_H
#include <cstdint>

#define EC_SII_VERSION				(1)

#define ESI_ROOTNODE_NAME			"EtherCATInfo"
#define ESI_VENDOR_NAME				"Vendor"
#define ESI_ID_NAME				"Id"
#define ESI_DEVICE_TYPE_NAME			"Type"
#define ESI_DEVICE_PRODUCTCODE_ATTR_NAME	"ProductCode"
#define ESI_DEVICE_REVISIONNO_ATTR_NAME		"RevisionNo"

#define EC_SII_EEPROM_VENDOR_OFFSET_BYTE	(0x08 * 2)
#define EC_SII_EEPROM_MAILBOX_OUT_OFFSET_BYTE	(0x18 * 2)
#define EC_SII_EEPROM_MAILBOX_IN_OFFSET_BYTE	(0x1A * 2)
#define EC_SII_EEPROM_MAILBOX_PROTO_OFFSET_BYTE	(0x1C * 2)
#define EC_SII_EEPROM_SIZE_OFFSET_BYTE		(0x3E * 2)
#define EC_SII_EEPROM_VERSION_OFFSET_BYTE	(0x3F * 2)
#define EC_SII_EEPROM_FIRST_CAT_HDR_OFFSET_BYTE	(0x40 * 2)
#define EC_SII_CONFIGDATA_SIZEB			(16)

// SII / EEPROM Category definitions, note these are (mostly) in decimal
#define EEPROMCategoryNOP		(0)
// Device specific	01-09
#define EEPROMCategorySTRINGS		(10)
#define EEPROMCategoryDataTypes		(20)
#define EEPROMCategoryGeneral		(30)
#define EEPROMCategoryFMMU		(40)
#define EEPROMCategorySyncM		(41)
#define EEPROMCategoryFMMUX		(42)
#define EEPROMCategorySyncUnit		(43)
#define EEPROMCategoryTXPDO		(50)
#define EEPROMCategoryRXPDO		(51)
#define EEPROMCategoryDC		(60)
#define EEPROMCategoryTimeouts		(70)
#define EEPROMCategoryDictionary	(80)
#define EEPROMCategoryHardware		(90)
#define EEPROMCategoryVendorInformation	(100)
#define EEPROMCategoryImages		(110)
// Vendor specific			0x0800-0x1FFF
// Application specific			0x2000-0x2FFF
// Vendor specific			0x3000-0xFFFE
// End					0xFFFF

#endif /* ESIDEFS_H */