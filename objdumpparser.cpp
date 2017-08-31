#include "objdumpparser.h"
ObjdumpParser::ObjdumpParser(const char* objdumpFile) {
	this->objdumpFile = objdumpFile;
}

enum ObjdumpParseState {
	searchPLT, searchPLTAddress
};

void ObjdumpParser::parseObjdump(std::vector<ADDRINT> &pltFunctions) {

	std::ifstream csFile(this->objdumpFile);
	std::string line;

	std::string pltSection = "Disassembly of section .plt:";
	std::string nextSection = "Disassembly of section";
	std::string malloc = "malloc";
	std::string cppNew = "Znwm";

	ObjdumpParseState curState = searchPLT;

	while (std::getline(csFile, line)) {
		if (curState == searchPLT) {
			if (!line.compare(0, pltSection.size(), pltSection)) {
				std::cout << "parseObjdump: found plt section: " << line
						<< std::endl;
				curState = searchPLTAddress;
			}
		} else if (curState == searchPLTAddress) {
			if (!line.compare(0, nextSection.size(), nextSection)) {
				std::cout << "parseObjdump: found end of plt section: " << line
						<< std::endl;
				return;
				// We need to skip the malloc entry, as this is an allowed plt call
			} else if (line.find("@") != string::npos
					&& line.find(malloc) == string::npos
					&& line.find("@") != string::npos
					&& line.find(cppNew) == string::npos) {
				std::cout << "parseObjdump: found address: " << line
						<< std::endl;
				std::size_t found = line.find(" ");
				std::size_t start = 0;
				std::string address = line.substr(start, found);
				std::cout << "\t: extracted address: " << address << std::endl;
				unsigned long addressLong = strtol(address.c_str(), NULL, 16)
						- 0x400000;
				std::cout << "\t: converted address: " << std::hex
						<< addressLong << std::endl;
				pltFunctions.push_back(addressLong);

			} else {
				std::cout << "parseObjdump: skipping line: " << line
						<< std::endl;
			}

		}
	}

}
