#ifndef SSCWRITER_H
#define SSCWRITER_H
#include "esctooldefs.h"

class SSCWriter {
public:
	struct OutputParams {
		bool capitalizeStructMembers = false;
		bool appendObjectIndexToStructs = false;
	};

	virtual ~SSCWriter() {};
	virtual void writeSSCFiles(Device* dev, OutputParams params) = 0;
protected:
	SSCWriter() {};
};

#endif /* SSCWRITER_H */