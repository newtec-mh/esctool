#ifndef SIIENCODE_H
#define SIIENCODE_H
#include <string>
#include "esctooldefs.h"

namespace SII {
	void encodeEEPROMBinary(uint32_t vendor_id, Device* dev, bool encodepdo, const std::string& file,
		const std::string& output, const bool verbose = false);
	void decodeEEPROMBinary(const std::string& file, const bool verbose = false);
};

#endif /* SIIENCODE_H */