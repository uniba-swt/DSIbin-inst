#ifndef OBJDUMPPARSER_H
#define OBJDUMPPARSER_H

#include "pin.H"
#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <stdlib.h>

class ObjdumpParser {
	const char *objdumpFile;
public:
	ObjdumpParser(const char* objdumpFile);
	void parseObjdump(std::vector<ADDRINT> &pltFunctions);
};

#endif
