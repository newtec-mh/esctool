#ifndef SOESCONFIGWRITER_H
#define SOESCONFIGWRITER_H
#include "sscwriter.h"

class SOESConfigWriter : public SSCWriter {
public:
	SOESConfigWriter(bool input_endianness_is_little);
	virtual ~SOESConfigWriter();
	void writeSSCFiles(Device* dev) override;
private:
	bool m_input_endianness_is_little;
};

#endif /* SOESCONFIGWRITER_H */