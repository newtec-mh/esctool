#include "utilfunc.h"

void printObject (Object* o, unsigned int level) {
//	printf("Obj: Index: 0x%.04X, Name: '%s'\n",(NULL != o->index ? EC_SII_HexToUint32(o->index) : 0),o->name);
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("Obj: Index: 0x%.04X, Name: '%s', Type: '%s', DataType: '%s', DefaultData: '%s', BitSize: '%u'\n",o->index,o->name,o->type,o->datatype?o->datatype->type:"null", o->defaultdata,o->bitsize);
	if(very_verbose && o->flags) {
		for(unsigned int l = 0; l < level; ++l) printf("\t");
		printf("Flags: '%s'\n",o->flags->category ? o->flags->category : "(No category)");
		if(o->flags->access) {
			if("Access: '%s'\n",o->flags->access->access ? o->flags->access->access : "(none)");
		} else printf("No access\n");
	}
	for(Object* si : o->subitems) printObject(si,level+1);
};

void printDataType (DataType* dt, unsigned int level) {
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("DataType: Name: '%s', Type: '%s'\n",dt->name,dt->type);
	for(DataType* dsi : dt->subitems) printDataType(dsi,level+1);
};

void printDataTypeVerbose (DataType* dt, unsigned int level) {
	if(level == 0) printf("-----------------\n");
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("DataType: ");
	printf("Name: '%s' ",dt->name);
	printf("Type: '%s' ",dt->type);
	printf("BaseType: '%s' ",dt->basetype);
	printf("BitSize: '%d' ",dt->bitsize);
	printf("BitOffset: '%d' ",dt->bitoffset);
	printf("SubIndex: '%d' ",dt->subindex);
	printf("SubItems: '%lu' ",dt->subitems.size());
	printf("ArrayInfo: '%s'",dt->arrayinfo ? "yes" : "no");
	if(dt->arrayinfo && very_verbose) {
		printf(" [ ");
		printf("Elements: '%d' ",dt->arrayinfo->elements);
		printf("LowerBound: '%d' ",dt->arrayinfo->lowerbound);
		printf("]\n");
	} else {
		printf("\n");
	}
	for(unsigned int l = 0; l < level; ++l) printf("\t");
	printf("Flags: '%s'",dt->flags ? "yes" : "none");
	if(dt->flags && very_verbose) {
		printf(" [ ");
		if(dt->flags->category) printf("Category: '%s' ",dt->flags->category);
		if(dt->flags->pdomapping) printf("PdOMapping: '%s' ",dt->flags->pdomapping);
		if(dt->flags->access) {
			if(dt->flags->access->access) printf("Access: '%s' ",dt->flags->access->access);
		}
		printf("]\n");
	} else {
		printf("\n");
	}
	for(DataType* si : dt->subitems) {
		for(unsigned int l = 0; l < level; ++l) printf("\t");
		printf("SubItem:\n");
		printDataTypeVerbose(si,level+1);
	}
	if(level == 0) printf("-----------------\n");
};
