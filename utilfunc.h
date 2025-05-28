#ifndef UTILFUNC_H
#define UTILFUNC_H
#include "esctooldefs.h"
#include <cstdio>

static bool verbose = false;
static bool very_verbose = false;

void printObject (Object* o, unsigned int level = 0);

void printDataType (DataType* dt, unsigned int level = 0);

void printDataTypeVerbose (DataType* dt, unsigned int level = 0);

#endif /* UTILFUNC_H */