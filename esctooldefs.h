#ifndef ESCTOOLDEFS_H
#define ESCTOOLDEFS_H
#include <cstdint>
#include <list>
#include <vector>
#include "esidefs.h"
#include <cstddef>

struct Group {
	const char* name = NULL;
	const char* type = NULL;
};

struct DcOpmode {
	const char* name = NULL;
	const char* desc = NULL;
	uint16_t assignactivate = 0x0;
	uint32_t cycletimesync0 = 0;
	uint32_t cycletimesync1 = 0;
	uint32_t shifttimesync0 = 0;
	uint32_t shifttimesync1 = 0;
	int16_t cycletimesync0factor = 0;
	int16_t cycletimesync1factor = 0;
};

struct DistributedClock {
	std::list<DcOpmode*> opmodes;
};

struct SyncManager {
	const char* type = NULL;
	uint16_t minsize = 0;
	uint16_t maxsize = 0;
	uint16_t defaultsize = 0;
	uint16_t startaddress = 0;
	uint8_t controlbyte = 0;
	bool enable = false;
};

struct FMMU {
	const char* type = NULL;
	int32_t syncmanager = -1;
	int32_t syncunit = -1;
};

struct Mailbox {
	bool datalinklayer = false;
	bool aoe = false;
	bool eoe = false;
	bool coe = false;
	bool foe = false;
	bool soe = false;
	bool voe = false;

	bool coe_sdoinfo = false;
	bool coe_pdoassign = false;
	bool coe_pdoconfig = false;
	bool coe_pdoupload = false;
	bool coe_completeaccess = false;
};

struct PdoEntry {
	bool fixed = false;
	uint32_t index = 0;
	uint32_t subindex = 0;
	uint16_t bitlen = 0;
	const char* datatype = NULL;
	const char* name = NULL;
	bool dependonslot = false; // For module PDOs
};

struct Pdo {
	bool fixed = false;
	bool mandatory = false;
	int syncmanager = 0;
	int syncunit = 0;
	uint32_t index = 0;
	const char* name = NULL;
	std::list<PdoEntry*> entries;
	bool dependonslot = false; // For module PDOs
};

struct ChannelInfo {
	uint32_t profileNo = 0;
};

struct ObjectAccess {
	const char* access = NULL;
	const char* readrestrictions = NULL;
	const char* writerestrictions = NULL;
};

struct ObjectFlags {
	const char* category = NULL;
	ObjectAccess* access = NULL;
	const char* pdomapping = NULL;
	const char* sdoaccess = NULL;
};

struct ArrayInfo {
	uint8_t lowerbound = 0;
	uint8_t elements = 0;
};

struct DataType {
	const char* name = NULL;
	const char* type = NULL;
	uint32_t bitsize = 0;
	uint32_t bitoffset = 0;
	const char* basetype = NULL;
	uint8_t subindex = 0;
	ArrayInfo* arrayinfo = NULL;
	std::vector<DataType*> subitems;
	ObjectFlags* flags = NULL;
};

struct Object {
	uint32_t index = 0;
	const char* name = NULL;
	const char* type = NULL;
	DataType* datatype = NULL;
	uint32_t bitsize = 0;
	uint32_t bitoffset = 0;
	const char* defaultdata = NULL;
	const char* defaultstring = NULL;
	ObjectFlags* flags = NULL;
	std::list<Object*> subitems;
	Object* parent = NULL;
};

struct Dictionary {
	std::list<DataType*> datatypes;
	std::list<Object*> objects;
};

struct Profile {
	ChannelInfo* channelinfo = NULL;
	Dictionary* dictionary = NULL;
};

struct SyncUnit {
	bool separate_su = false;
	bool separate_frame = false;
	bool depend_on_input_state = false;
	bool frame_repeat_support = false;
};

struct Slot {
	uint8_t slotno = 0;
	uint8_t slotpdoincrement = 0;
	uint8_t slotindexincrement = 0;
	std::list<uint8_t> moduleidents;
};

struct Slots {
	uint8_t maxslotcount;
	uint8_t slotpdoincrement = 0;
	uint8_t slotindexincrement = 0;
	std::list<Slot*> slots;
};

struct Module {
	uint8_t ident;
	const char* type;
	std::list<Pdo*> txpdo;
	std::list<Pdo*> rxpdo;
};

struct Device {
	uint32_t product_code = 0x0;
	uint32_t revision_no = 0x0;
	const char* name = NULL;
	const char* physics = NULL;
	Group* group = NULL;
	const char* type = NULL;
	std::list<FMMU*> fmmus;
	std::list<SyncManager*> syncmanagers;
	Mailbox* mailbox = NULL;
	DistributedClock* dc = NULL;
	uint32_t eepromsize = 0x0;
	uint8_t configdata[EC_SII_CONFIGDATA_SIZEB];
	std::list<Pdo*> txpdo;
	std::list<Pdo*> rxpdo;
	SyncUnit* syncunit = NULL;
	Profile* profile = NULL;
	std::list<Module*>* modules = NULL;
	Slots* slots = NULL;
};


#endif /* ESCTOOLDEFS_H */