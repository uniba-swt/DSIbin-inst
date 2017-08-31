#include <stdio.h>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <iostream>
#include <stdlib.h>

#include "typeparser.h"

HowardType::HowardType(std::string type, std::string identifier,
		int offsetFromStart, unsigned int size, bool isCompound) {
	this->stack = false;
	this->isCompound = isCompound;
	this->identifier = identifier;
	this->type = type;
	this->offsetFromStart = offsetFromStart;
	this->size = size;
	this->end = offsetFromStart + size;
	this->members = new std::vector<HowardType*>();
}
HowardType::HowardType(std::string type, int offsetFromStart, bool isCompound) {
	this->stack = false;
	this->isCompound = isCompound;
	this->type = type;
	this->offsetFromStart = offsetFromStart;
	this->members = new std::vector<HowardType*>();
}
void HowardType::setSizeAndEnd(unsigned int size) {
	this->size = size;
	this->end = offsetFromStart + size;
}
int HowardType::getOffset() {
	return this->offsetFromStart;
}
unsigned int HowardType::getSize() {
	return this->size;
}
std::string HowardType::getType() {
	return this->type;
}

void HowardType::setTypeLocation(std::string location) {
	this->typeLocation = location;
}

std::string HowardType::getTypeLocation() {
	return this->typeLocation;
}

std::vector<HowardType*>* HowardType::getMembers() {
	return this->members;
}
void HowardType::addMember(HowardType *member) {
	this->members->push_back(member);
}
void HowardType::addMembers(std::vector<HowardType*> *newMembers) {
	this->members->insert(this->members->end(), newMembers->begin(),
			newMembers->end());
}
bool HowardType::getIsCompound() {
	return this->isCompound;
}

void HowardType::setIsStack(bool isStack) {
	this->stack = isStack;
}
bool HowardType::getIsStack() {
	return this->stack;
}

std::vector<HowardType*>* findTypeForOffsetRec(unsigned long offset,
		unsigned long last_offset, std::vector<HowardType*>* members) {
	std::vector<HowardType*>::iterator it = members->begin();
	std::vector<HowardType*>*types = new std::vector<HowardType*>();
	std::cout << "findTypeForOffsetRec: " << offset << std::endl;
	// Process all members of this compound type
	for (; members->end() != it; it++) {
		HowardType *cur = (*it);
		unsigned long curOffset = cur->getOffset();
		std::cout << "\tchecking item: " << cur->getType() << " cur offset: "
				<< curOffset << " cur size: " << cur->getSize() << std::endl;
		// Direct match on non compound type
		if (!cur->getIsCompound() && curOffset == offset) {
			std::cout
					<< "\tcur is no compound && curOffset == offset: curOffset: "
					<< curOffset << " offset: " << offset << std::endl;
			types->push_back(cur);
			return types;
			// Current object is compound and offsets either match directly, or offset lies within the range of the compound
		} else if (cur->getIsCompound()
				&& ((curOffset == offset)
						|| (curOffset < offset
								&& offset < curOffset + cur->getSize()))) {
			types->push_back(cur);
			std::cout << "\tcur is compound && curOffset == offset: curOffset: "
					<< curOffset << " offset: " << offset << std::endl;
			std::cout
					<< "\t\t (curOffset <= curOffset + offset && curOffset + offset < curOffset + cur->getSize()): "
					<< curOffset << " offset: " << offset << " cur->getSize(): "
					<< cur->getSize() << std::endl;
			// Now one needs to recurse to find the next deeper level
			unsigned long rec_offset = offset;
			// Only subtract, if we have now seen an offset change: prevents multiple subtraction of
			// the same offset in case of multiple nested structs at the head:
			// struct {
			//  struct {
			//  	struct {
			//  	}
			//  }
			// }
			if (last_offset != curOffset) {
				rec_offset -= curOffset;
			}
			std::vector<HowardType*>*tmpTypes = findTypeForOffsetRec(rec_offset,
					curOffset, cur->getMembers());
			types->insert(types->end(), tmpTypes->begin(), tmpTypes->end());
			delete tmpTypes;
			return types;
		}
	}
	return types;
}

std::vector<HowardType*>* HowardType::findTypeForOffset(unsigned long offset) {
	std::vector<HowardType*>*types = new std::vector<HowardType*>();
	std::cout << "findTypeForOffset: " << offset << std::endl;
	if (offset < 0 || offset >= this->getSize()) {
		std::cout << "findTypeForOffset: offset sanity check failed: "
				<< this->getType() << std::endl;
		std::cout << "\tsize: " << this->getSize() << std::endl;
		return types;
	}

	std::cout << "passed sanity check" << std::endl;

	// Always add the current object
	types->push_back(this);

	// If current object is not a compound, we are done
	if (!this->getIsCompound())
		return types;

	std::cout << "current type is compound " << std::endl;

	std::vector<HowardType*> *ret = findTypeForOffsetRec(offset, 0,
			this->getMembers());
	types->insert(types->end(), ret->begin(), ret->end());
	delete ret;
	return types;
}

std::string extractMD5sum(std::string line,
		std::map<std::string, std::vector<std::string>*> &md5ToCallStack) {
	std::cout << "Found MD5: " << line << std::endl;
	// Cut off the md5 part from the front
	std::string md5sum = line.substr(6);
	std::cout << "\tExtracted MD5 sum: " << md5sum << std::endl;
	md5ToCallStack.insert(
			std::pair<std::string, std::vector<std::string>*>(md5sum,
					new std::vector<std::string>));
	return md5sum;
}

enum CallStackState {
	searchMD5, searchEnd
};
void parseCallStackFile(const char *callStackFileName,
		std::map<std::string, std::vector<std::string>*> &md5ToCallStack) {
	printf("parseCallStack called: %s\n", callStackFileName);

	std::ifstream csFile(callStackFileName);
	std::string line;

	std::string md5("MD5");
	std::string libc("libc");
	std::string libcpp("libstdc++");
	std::string lastMD5sum = "";

	CallStackState curState = searchMD5;
	while (std::getline(csFile, line)) {
		std::cout << "Parsed line: " << line << std::endl;
		if (curState == searchMD5) {
			if (!line.compare(0, md5.size(), md5)) {
				lastMD5sum = extractMD5sum(line, md5ToCallStack);
				curState = searchEnd;
			}
		} else if (curState == searchEnd) {
			if (!line.compare(0, md5.size(), md5)) {
				lastMD5sum = extractMD5sum(line, md5ToCallStack);
			} else if (line.find(libc) == std::string::npos
					&& line.find(libcpp) == std::string::npos) {
				std::cout << "Found call stack element: " << line << std::endl;
				// Format: index:instrPtr,image
				// First: cut off index:
				std::size_t found = line.find(":");
				std::size_t start = 0;
				std::string index = line.substr(start, found);
				// Skip the ":"
				start = found + 1;
				// Second: find instrPtr
				found = line.find(",");
				std::string instPtr = line.substr(start, found - start);
				std::cout << "\t: extracted ip: " << instPtr << std::endl;
				std::map<std::string, std::vector<std::string>*>::iterator it =
						md5ToCallStack.find(lastMD5sum);
				if (it != md5ToCallStack.end()) {
					std::vector<std::string>::iterator front =
							it->second->begin();
					it->second->insert(front, instPtr);
				}
			}
		}
	}
}

std::string createCallStackString(std::vector<std::string>*callStack) {
	std::vector<std::string>::iterator it = callStack->begin();
	std::string callStackStr = "";
	std::stringstream csStream;
	for (; callStack->end() != it; it++) {
		csStream << *it << ",";
	}
	return csStream.str();
}

std::vector<std::string>* findAllMergedMD5Sums(
		std::vector<std::vector<std::string>*> *mergeInfo, std::string md5sum) {
	std::cout << "findAllMergedMD5Sums" << std::endl;
	for (std::vector<std::vector<std::string>*>::iterator it =
			mergeInfo->begin(); it != mergeInfo->end(); ++it) {
		std::cout << "mergeInfo: found items: " << std::endl;
		for (std::vector<std::string>::iterator splittedIt = (*it)->begin();
				splittedIt != (*it)->end(); ++splittedIt) {
			if ((*splittedIt).compare(md5sum) == 0) {
				std::cout << "\tsplittedIt: found md5sum:" << md5sum
						<< ". Returning merge vector" << std::endl;
				// Return the complete vector
				return *it;
			}
		}
	}
	// Just return an empty vector 
	return new std::vector<std::string>;
}

std::string addCSForMD5(std::string md5sum,
		std::map<std::string, std::vector<HowardType*>*> *typeDB,
		std::map<std::string, std::vector<std::string>*> &md5ToCallStack) {
	std::cout << "addCSForMD5 called: md5sum: " << md5sum << std::endl;
	std::map<std::string, std::vector<std::string>*>::iterator md5toCS =
			md5ToCallStack.find(md5sum);
	if (md5toCS == md5ToCallStack.end()) {
		std::cerr << "addCSForMD5: MD5 " << md5sum
				<< " not found in dictionary." << std::endl;
	}

	std::vector<std::string> *cs = md5toCS->second;
	std::string csStr = createCallStackString(cs);
	if (typeDB->find(csStr) == typeDB->end()) {
		std::cout << "adding csStr: " << csStr << std::endl;
		typeDB->insert(
				std::pair<std::string, std::vector<HowardType*>*>(csStr,
						new std::vector<HowardType*>));
		return csStr;
	} else {
		std::cout << "csStr: " << csStr << " already present." << std::endl;
		return "";
	}
}

std::vector<std::string>* addAllMergedMD5sums(std::string md5sum,
		std::map<std::string, std::vector<HowardType*>*> *typeDB,
		std::map<std::string, std::vector<std::string>*> &md5ToCallStack,
		std::vector<std::vector<std::string>*> *mergeInfo) {
	std::cout << "addAllMergedMD5sums: for initial md5: " << md5sum
			<< std::endl;
	std::vector<std::string> *allCS = new std::vector<std::string>();
	std::vector<std::string>* mergedMD5s = findAllMergedMD5Sums(mergeInfo,
			md5sum);
	// In case there was nothing to merge for Howard, just add the current md5sum
	// to keep the algorithm uniform for merge/no-merge cases
	for (std::vector<std::string>::iterator it = mergedMD5s->begin();
			it != mergedMD5s->end(); ++it) {
		std::cout << "adding md5sum: " << *it << std::endl;
		allCS->push_back(addCSForMD5(*it, typeDB, md5ToCallStack));
	}
	return allCS;
}

std::vector<std::string>* extractMD5sumForFunction(std::string line,
		std::map<std::string, std::vector<HowardType*>*> *typeDB,
		std::map<std::string, std::vector<std::string>*> &md5ToCallStack,
		std::vector<std::vector<std::string>*> *mergeInfo) {
	std::cout << "Found Function with MD5: " << line << std::endl;
	// Cut off the md5 part from the front
	std::string md5sum = line.substr(11);
	std::cout << "\tExtracted MD5 sum: " << md5sum << std::endl;
	// First add the current md5 sum and store the call stack associated with it
	std::string csStr = addCSForMD5(md5sum, typeDB, md5ToCallStack);
	std::vector<std::string>* allCS = addAllMergedMD5sums(md5sum, typeDB,
			md5ToCallStack, mergeInfo);

	// initial cs for processed md5 is always last
	allCS->push_back(csStr);

	return allCS;
}

std::string uniqueTypeNameGenerator(int potentialDsiID) {
	static int id = 0;
	std::stringstream ss;
	ss << "type_" << (potentialDsiID >= 0 ? potentialDsiID : id++);
	return ss.str();
}

int getTypeSizeInBytes(std::string type) {
	std::cout << "getTypeSizeInBytes: " << type << std::endl;
	if (type.compare("INT32") == 0) {
		std::cout << "INT32" << std::endl;
		return 4;
	} else if (type.compare("INT16") == 0) {
		std::cout << "INT16" << std::endl;
		return 2;
	} else if (type.compare("INT8") == 0) {
		std::cout << "INT8" << std::endl;
		return 1;
	} else if (type.compare("INT64") == 0) {
		std::cout << "INT64" << std::endl;
		return 8;
	} else if (type.find("*") != std::string::npos) {
		std::cout << "Pointer" << std::endl;
		return 8;
	}
	std::cout << "Fall through" << std::endl;
	return 0;
}

std::pair<std::string, unsigned long>* getTypeAndOffset(std::string line) {
	// Primitive type found, add immediately
	// Fetch the offset first
	std::pair<std::string, unsigned long> *ret = new std::pair<std::string,
			unsigned long>();
	std::size_t found = line.find(":");
	std::size_t start = 0;
	std::string offsetInStruct = line.substr(start, found);
	// Then the type: skip the ': ' <- not empty space
	start = found + 2;
	found = line.find(";");
	std::string typeOfMember = line.substr(start, found - start);
	std::cout << "\tadding new member: " << line << " offset: "
			<< offsetInStruct << " type: ." << typeOfMember << "." << std::endl;
	std::cout << "\toffset (converted): "
			<< strtol(offsetInStruct.c_str(), NULL, 16) << " size: "
			<< getTypeSizeInBytes(typeOfMember) << "." << std::endl;
	ret->first = typeOfMember;
	ret->second = strtol(offsetInStruct.c_str(), NULL, 16);
	return ret;
}

HowardType* createPrimitiveType(std::string line) {
	// Primitive type found, add immediately
	// Fetch the offset first
	std::size_t found = line.find(":");
	std::size_t start = 0;
	std::string offsetInStruct = line.substr(start, found);
	HowardType *memberType;
	// Then the type: skip the ': ' <- not empty space
	start = found + 2;
	found = line.find(";");
	std::string typeOfMember = line.substr(start, found - start);
	std::cout << "\tadding new member: " << line << " offset: "
			<< offsetInStruct << " type: ." << typeOfMember << "." << std::endl;
	std::cout << "\toffset (converted): "
			<< labs(strtol(offsetInStruct.c_str(), NULL, 16)) << " size: "
			<< getTypeSizeInBytes(typeOfMember) << "." << std::endl;
	memberType = new HowardType(typeOfMember,
			labs(strtol(offsetInStruct.c_str(), NULL, 16)), false);
	memberType->setSizeAndEnd(getTypeSizeInBytes(typeOfMember));
	return memberType;
}

int findPotentialDSIType(std::string line) {
	std::string funSignature = "findPotentialDSDIType: ";
	std::cout << funSignature << "processing line: " << line << std::endl;
	std::size_t structPos = line.find("struct");
	int lengthOfStructStr = 6;
	char openCurly = '{';
	int ret = -1;
	if (line[structPos + lengthOfStructStr] != openCurly) {
		std::cout << funSignature << "found open curly" << line << std::endl;
		std::size_t curlyPos = line.find(openCurly);
		// increment one position ahead to skip the empty space
		lengthOfStructStr += structPos + 1;
		if (curlyPos != std::string::npos) {
			std::cout << funSignature << "at position: " << curlyPos
					<< " curlyPos - lengthOfStructStr: "
					<< (curlyPos - lengthOfStructStr) << std::endl;
			std::string dsiID = line.substr(lengthOfStructStr,
					curlyPos - lengthOfStructStr);
			ret = atoi(dsiID.c_str());
			std::cout << funSignature << "dsiID: " << dsiID << " ret: " << ret
					<< std::endl;
		}
	}
	return ret;
}

enum HeapTypeState {
	searchFunction, searchTypeBegin, searchTypeEnd, searchHTSEnd
};
HowardType* parseHeapTypeSubsection(std::ifstream &csFile, std::string typeStr,
		std::string typeLocation, int nestingLvl, unsigned long enclosingOffset,
		bool isStack, int potentialDsiID) {
	std::string line;
	std::string structBegin = "struct";
	printf("parseHeapTypeSubsection\n");
	// When ever this method is called we have an aggregate type
	// We need to set the offset to the enclosing type
	HowardType *type = new HowardType(uniqueTypeNameGenerator(potentialDsiID),
			enclosingOffset, true);
	// Always record the MD5sum where the type belongs to
	type->setTypeLocation(typeLocation);
	type->setIsStack(isStack);
	HowardType *memberType;
	while (std::getline(csFile, line)) {
		std::cout << "\tprocessing line: " << line << std::endl;
		if (line.find(structBegin) != std::string::npos) {
			int potentialDsiIDRec = findPotentialDSIType(line);
			// Compound type found, recurse
			std::cout << "\trecursing on: " << line << std::endl;
			std::pair<std::string, unsigned long>* typeAndOffset =
					getTypeAndOffset(line);
			std::cout << "\ttype: " << typeAndOffset->first << std::endl;
			std::cout << "\toffset: " << typeAndOffset->second << std::endl;
			memberType = parseHeapTypeSubsection(csFile, typeStr, typeLocation,
					nestingLvl + 1, typeAndOffset->second, isStack,
					potentialDsiIDRec);
			type->addMember(memberType);
			delete typeAndOffset;
		} else if (line.find("}") != std::string::npos) {
			std::cout << "\tfound end of struct type: " << line << std::endl;
			int summedSize = 0;
			unsigned long lastOffset = 0;
			for (std::vector<HowardType*>::iterator it =
					type->getMembers()->begin();
					it != type->getMembers()->end(); it++) {
				std::cout << "\tmember: " << (*it)->getType() << " : size: "
						<< (*it)->getSize() << " : offset: "
						<< (*it)->getOffset() << std::endl;
				summedSize += (*it)->getOffset() - lastOffset;
				lastOffset = (*it)->getOffset();
			}
			// Add the size of the last element
			summedSize += type->getMembers()->back()->getSize();
			type->setSizeAndEnd(summedSize);
			return type;
		} else {
			// Primitive type found, add immediately
			// Fetch the offset first
			type->addMember(createPrimitiveType(line));
		}
	}
	// Should actually never be reached
	return type;
}

void parseHeapTypeInformation(const char* heapTypesFileName,
		std::map<std::string, std::vector<HowardType*>*> *typeDB,
		std::map<std::string, std::vector<std::string>*> &md5ToCallStack,
		std::vector<std::vector<std::string>*> *mergeInfo) {
	printf("parseHeapTypeInformation called: %s\n", heapTypesFileName);
	std::ifstream csFile(heapTypesFileName);
	std::string line;

	std::string function = "Function";
	std::string structBegin = "struct";
	std::string lastCS = "";
	std::string lastMD5sum = "";
	std::vector<std::string>*allCS;
	HeapTypeState curState = searchFunction;
	int nestingLevel;
	while (std::getline(csFile, line)) {
		std::cout << "Parsed line: " << line << std::endl;
		if (curState == searchFunction) {
			if (!line.compare(0, function.size(), function)) {
				allCS = extractMD5sumForFunction(line, typeDB, md5ToCallStack,
						mergeInfo);
				lastMD5sum = line.substr(11);
				curState = searchTypeBegin;
				nestingLevel = 0;
			}
		} else if (curState == searchTypeBegin) {
			if (!line.compare(0, function.size(), function)) {
				if (nestingLevel != 0) {
					std::cerr
							<< "Parsing file failed: New function found, but current type not properly parsed."
							<< std::endl;
					exit(1);
				}
				lastMD5sum = line.substr(11);
				allCS = extractMD5sumForFunction(line, typeDB, md5ToCallStack,
						mergeInfo);
				curState = searchTypeBegin;
			} else if (line.find(structBegin) != std::string::npos) {
				// Check, if we got a merged type from DSI-TypeRefiner
				int potentialDsiID = findPotentialDSIType(line);
				// The last element added is also the last element in the cs vector
				lastCS = allCS->back();
				std::cout
						<< "Fetching the parsed cs element (lastCS regarding file parsing logic): "
						<< lastCS << std::endl;
				HowardType *type = parseHeapTypeSubsection(csFile, lastCS,
						lastMD5sum, 0, 0, false /* indicate heap */,
						potentialDsiID);
				std::cout << "Iterating over all found cs" << std::endl;
				for (std::vector<std::string>::iterator csIt = allCS->begin();
						csIt != allCS->end(); csIt++) {
					lastCS = *csIt;
					std::cout << "Processing element: " << lastCS << std::endl;
					std::map<std::string, std::vector<HowardType*>*>::iterator it =
							typeDB->find(lastCS);
					if (it != typeDB->end()) {
						std::vector<HowardType*>* types = it->second;
						types->push_back(type);
						std::cout << " added " << type->getType() << " to "
								<< lastCS << std::endl;
						std::vector<HowardType*>::iterator typesIt =
								types->begin();
						for (; types->end() != typesIt; typesIt++) {
							std::cout << "\ttop level: "
									<< (*typesIt)->getType() << std::endl;
							std::vector<HowardType*>::iterator membersIt =
									(*typesIt)->getMembers()->begin();
							for (; (*typesIt)->getMembers()->end() != membersIt;
									membersIt++) {
								std::cout << "\tmember: "
										<< (*membersIt)->getType() << std::endl;
							}
						}
					} else {
						std::cerr << "did not find " << lastCS << std::endl;
					}
				}
			} else {
				std::cout << "skipping line: " << line << std::endl;
			}
		}
	}
}

// Taken from: http://stackoverflow.com/questions/236129/split-a-string-in-c
void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
}

std::vector<std::vector<std::string>*>* parseMergeInfo(
		const char* mergeInfoFileName) {
	printf("parseMergeInfo called: %s\n", mergeInfoFileName);
	std::vector<std::vector<std::string>*>* mergeInfo = new std::vector<
			std::vector<std::string>*>();

	std::ifstream csFile(mergeInfoFileName);
	std::string line;

	std::string md5("MD5");
	std::string libc("libc");
	std::string libcpp("libstdc++");
	std::string lastMD5sum = "";

	while (std::getline(csFile, line)) {
		std::vector<std::string> splitted;
		split(line, ',', splitted);

		std::cout << "Found MD5 sums:" << std::endl;
		std::vector<std::string>* splittedItems = new std::vector<std::string>;
		for (std::vector<std::string>::iterator it = splitted.begin();
				it != splitted.end(); ++it) {
			std::cout << "\t:" << *it << std::endl;
			splittedItems->push_back(*it);
		}
		std::cout << std::endl;
		mergeInfo->push_back(splittedItems);
	}
	return mergeInfo;
}

std::map<std::string, std::vector<HowardType*>*>* createTypeDB(
		const char* callStackFileName, const char *typeDBFile,
		std::vector<std::vector<std::string>*> *mergeInfo) {
	// The type db is a map, which takes a call stack as a key and 
	// associates a list of HowardTypes with it. This is needed, 
	// because the call stack will also type the local stack which
	// is a list of types.
	std::map<std::string, std::vector<HowardType*>*> *typeDB = new std::map<
			std::string, std::vector<HowardType*>*>;
	// Intermediate lookup to create the typeDB, as the call stack and
	// the type information are spread accross files.
	std::map<std::string, std::vector<std::string>*> md5ToCallStack;

	printf("createTypeDB called\n");
	parseCallStackFile(callStackFileName, md5ToCallStack);

	printf("printing map\n");
	for (std::map<std::string, std::vector<std::string>*>::const_iterator it =
			md5ToCallStack.begin(); it != md5ToCallStack.end(); ++it) {
		printf("printing...\n");
		std::cout << it->first << " " << std::endl;
		for (std::vector<std::string>::const_iterator itStr =
				it->second->begin(); itStr != it->second->end(); ++itStr) {
			std::cout << "\t" << *itStr << std::endl;
		}
	}

	parseHeapTypeInformation(typeDBFile, typeDB, md5ToCallStack, mergeInfo);

	return typeDB;
}

void collectTypesRec(std::map<std::string, std::string> *strTypes,
		std::vector<HowardType*>* types) {
	std::vector<HowardType*>::iterator it = types->begin();
	for (; types->end() != it; it++) {
		HowardType *curType = (*it);
		std::string typeName = curType->getType();
		std::map<std::string, std::string>::iterator typePresent =
				strTypes->find(typeName);
		// Only process this type, if it is not present yet
		if (strTypes->end() == typePresent) {
			// Currently we are only interested in compounds!
			if (curType->getIsCompound()) {
				std::stringstream typeInfo;
				typeInfo << "<compound mode=\"struct\">" << std::endl;
				typeInfo << "<isStack>" << curType->getIsStack() << "</isStack>"
						<< std::endl;
				typeInfo << "<offset>" << std::dec << curType->getOffset()
						<< std::dec << "</offset>" << std::endl;
				typeInfo << "<typeID>" << curType->getTypeLocation()
						<< "</typeID>" << std::endl;
				typeInfo << "<tagName>" << curType->getType() << "</tagName>"
						<< std::endl;
				typeInfo << "<sizeInBytes>" << curType->getSize()
						<< "</sizeInBytes>" << std::endl;
				typeInfo << "<location file=\"-\" line=\"0\" col=\"0\"/>"
						<< std::endl;
				typeInfo << "<fields>" << std::endl;
				std::vector<HowardType*> *members = curType->getMembers();
				std::vector<HowardType*>::iterator membersIt = members->begin();
				int index = 0;
				for (; members->end() != membersIt; membersIt++) {
					HowardType* member = (*membersIt);
					typeInfo << "<field index=\"" << index++ << "\">"
							<< std::endl;
					typeInfo << "<name>" << "dont_care" << "</name>"
							<< std::endl;
					std::string typeName = member->getType();
					std::size_t pos = typeName.find("*");
					if (pos != std::string::npos)
						typeName.insert(pos, " ");

					std::string isStruct = "";
					if (member->getIsCompound())
						isStruct = "struct ";
					typeInfo << "<sugaredType>" << isStruct << typeName
							<< "</sugaredType>" << std::endl;
					typeInfo << "<desugaredType>" << isStruct << typeName
							<< "</desugaredType>" << std::endl;
					typeInfo << "<sizeInBits>" << (member->getSize() * 8)
							<< "</sizeInBits>" << std::endl;
					typeInfo << "<offsetInBytes>" << member->getOffset()
							<< "</offsetInBytes>" << std::endl;
					typeInfo << "<location file=\"-\" line=\"0\" col=\"0\"/>"
							<< std::endl;
					typeInfo << "</field>" << std::endl;
				}
				typeInfo << "</fields>" << std::endl;
				typeInfo << "</compound>" << std::endl;
				strTypes->insert(
						std::pair<std::string, std::string>(typeName,
								typeInfo.str()));
				// Recures on the members
				collectTypesRec(strTypes, members);
			}
		}
	}
}

std::map<std::string, std::string>* collectTypes(
		std::map<std::string, std::vector<HowardType*>*>* typeDB) {
	std::map<std::string, std::vector<HowardType*>*>::iterator it =
			typeDB->begin();
	std::map<std::string, std::string> *strTypes = new std::map<std::string,
			std::string>();
	for (; typeDB->end() != it; it++) {
		collectTypesRec(strTypes, it->second);
	}
	return strTypes;
}

void printTypes(std::ofstream &typeDBXMLFile,
		std::map<std::string, std::vector<HowardType*>*>* typeDB) {
	std::map<std::string, std::string> *strTypes = collectTypes(typeDB);
	std::map<std::string, std::string>::iterator strIt = strTypes->begin();
	for (; strIt != strTypes->end(); strIt++) {
		typeDBXMLFile << strIt->second;
	}
}

void writeTypeDBToXML(const char* xmlFileName,
		std::map<std::string, std::vector<HowardType*>*>* typeDB,
		std::map<std::string, std::vector<HowardType*>*>* stackTypeDB) {
	std::ofstream typeDBXMLFile;
	typeDBXMLFile.open(xmlFileName);
	typeDBXMLFile << "<types arch=\"64\">" << std::endl;

	printTypes(typeDBXMLFile, typeDB);
	printTypes(typeDBXMLFile, stackTypeDB);

	typeDBXMLFile << "</types>" << std::endl;
	typeDBXMLFile.close();
}

/*********** Stack ******************/
std::string extractStartAddrOfFunction(std::string line,
		std::map<std::string, std::vector<HowardType*>*> *typeDB) {
	std::cout << "Found Function: " << line << std::endl;
	// Cut off the md5 part from the front
	std::string addr = line.substr(11);
	std::cout << "\tExtracted address: " << addr << "." << std::endl;
	typeDB->insert(
			std::pair<std::string, std::vector<HowardType*>*>(addr,
					new std::vector<HowardType*>));
	return addr;
}

void addTypeToStackDB(std::string key, HowardType *type,
		std::map<std::string, std::vector<HowardType*>*> *typeDB) {
	std::cout << "calling addTypeToStackDB with key " << key << std::endl;
	std::map<std::string, std::vector<HowardType*>*>::iterator it =
			typeDB->find(key);
	if (it != typeDB->end()) {
		std::cout << "adding type to " << key << std::endl;
		std::vector<HowardType*>* types = it->second;
		types->push_back(type);
	} else {
		std::cerr << "did not find " << key << std::endl;
	}
}

void parseStackTypeInformation(const char* stackTypesFileName,
		std::map<std::string, std::vector<HowardType*>*> *typeDB) {
	printf("parseStackTypeInformation called: %s\n", stackTypesFileName);
	std::ifstream csFile(stackTypesFileName);
	std::string line;

	std::string function = "Function";
	std::string structBegin = "struct";
	std::string funStartAddr = "";
	HeapTypeState curState = searchFunction;
	while (std::getline(csFile, line)) {
		std::cout << "Parsed line: " << line << std::endl;
		if (curState == searchFunction) {
			if (!line.compare(0, function.size(), function)) {
				funStartAddr = extractStartAddrOfFunction(line, typeDB);
				curState = searchTypeBegin;
			}
		} else if (curState == searchTypeBegin) {
			if (line.size() == 0) {
				std::cout << "End of local stack found for function: "
						<< funStartAddr << std::endl;
				curState = searchFunction;
			} else if (line.find(structBegin) == std::string::npos) {
				std::cout << "Found primitive type: " << line << std::endl;
				addTypeToStackDB(funStartAddr, createPrimitiveType(line),
						typeDB);
			} else if (line.find(structBegin) != std::string::npos) {
				std::cout << "Found compound type: " << line << std::endl;
				int potentialDsiID = findPotentialDSIType(line);
				// Add the offset
				std::size_t found = line.find(":");
				std::size_t start = 0;
				std::string offsetInStruct = line.substr(start, found);
				HowardType *type = parseHeapTypeSubsection(csFile, funStartAddr,
						funStartAddr, 0,
						labs(strtol(offsetInStruct.c_str(), NULL, 16)),
						true /* indicate stack */, potentialDsiID);
				addTypeToStackDB(funStartAddr, type, typeDB);
				std::cout << " added " << type->getType() << " to "
						<< funStartAddr << std::endl;
			} else {
				std::cout << "skipping line: " << line << std::endl;
			}
		}
	}
}

std::map<std::string, std::vector<HowardType*>*>* createStackTypeDB(
		const char* stackFileName) {
	// Stack type db. Key = function start address, Value = list of HowardTypes
	std::map<std::string, std::vector<HowardType*>*> *typeDB = new std::map<
			std::string, std::vector<HowardType*>*>;
	parseStackTypeInformation(stackFileName, typeDB);
	return typeDB;
}
