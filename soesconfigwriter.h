#ifndef SOESCONFIGWRITER_H
#define SOESCONFIGWRITER_H
#include "sscwriter.h"
#include <string>

class SOESConfigWriter : public SSCWriter {
public:
	SOESConfigWriter(const std::string& outdir = "", bool input_endianness_is_little = false);
	virtual ~SOESConfigWriter();
	void writeSSCFiles(Device* dev) override;
private:
	std::string m_outputdir;
	bool m_input_endianness_is_little;
};

#endif /* SOESCONFIGWRITER_H */