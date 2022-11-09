#ifndef SSCWRITER_H
#define SSCWRITER_H
#include "esctooldefs.h"

class SSCWriter {
public:
	virtual ~SSCWriter() {};
	virtual void writeSSCFiles(Device* dev) = 0;
protected:
	SSCWriter() {};
};

#endif /* SSCWRITER_H */