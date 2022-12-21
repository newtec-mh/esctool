#ifndef ESIXMLPARSING_H
#define ESIXMLPARSING_H
#include <string>
#include "tinyxml2/tinyxml2.h"
#include "esctooldefs.h"

class ESIXML {
public:
	ESIXML(const int verbosity = 0);
	virtual ~ESIXML();
	void parse(const std::string& file);
	std::list<Device*>& getDevices(void);
	const uint32_t getVendorID(void) const;
	const char* getVendorName(void) const;
private:
	bool verbose;
	bool very_verbose;
	uint32_t vendor_id;
	const char* vendor_name;
	std::list<Module*> modules;
	std::list<Group*> groups;
	std::list<Device*> devices;
	tinyxml2::XMLDocument doc;

	void parseXMLGroup(const tinyxml2::XMLElement* xmlgroup);
	void parseXMLMailbox(const tinyxml2::XMLElement* xmlmailbox,Device* dev);
	void parseXMLPdo(const tinyxml2::XMLElement* xmlpdo, std::list<Pdo*>* pdolist);
	void parseXMLSyncUnit(const tinyxml2::XMLElement* xmlsu, Device* dev);
	void parseXMLDistributedClock(const tinyxml2::XMLElement* xmldc, DistributedClock* dc);
	void parseXMLObject(const tinyxml2::XMLElement* xmlobject, Dictionary* dict, Object* parent = NULL);
	void parseXMLSlots(const tinyxml2::XMLElement* xmlslots, Device *dev);

	void parseXMLDataType(const tinyxml2::XMLElement* xmldatatype, Dictionary* dict = NULL, DataType* parent = NULL);
	void parseXMLProfile(const tinyxml2::XMLElement* xmlprofile, Device *dev);
	void parseXMLDevice(const tinyxml2::XMLElement* xmldevice);

	void parseXMLModule(const tinyxml2::XMLElement* xmlmodule);

	void parseXMLVendor(const tinyxml2::XMLElement* xmlvendor);
	void parseXMLElement(const tinyxml2::XMLElement* element, void* data = NULL);
};

#endif /* ESIXMLPARSING_H */