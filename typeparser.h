#ifndef _TYPEPARSER_H
#define _TYPEPARSER_H

#include <map>
#include <vector>

class HowardType {
	bool isCompound;
	bool stack;
	// Stores the location where this type was found
	// isStack == true  => address of function
	// isStack == false => hash of memory chunk
	std::string typeLocation;
	// The offset can be from the frame pointer, the base pointer of the struct
	int offsetFromStart;
	unsigned int size;
	int end;
	std::string type;
	std::string identifier;
	std::vector<HowardType*> *members;
public:
	HowardType(std::string type, std::string identifier, int offsetFromStart, unsigned int size, bool isCompound);
	HowardType(std::string type, int offsetFromStart, bool isCompound);
	bool getIsCompound();
	void setSizeAndEnd(unsigned int size);
	int getOffset();
	unsigned int getSize();
	std::string getType();
	std::vector<HowardType*>* getMembers();
	void addMember(HowardType *member);
	std::vector<HowardType*>* findTypeForOffset(unsigned long offset);
	void addMembers(std::vector<HowardType*> *newMembers);
	void setIsStack(bool isStack);
	bool getIsStack();
	void setTypeLocation(std::string location);
	std::string getTypeLocation();
};

std::map<std::string, std::vector<HowardType*>* >* createTypeDB(const char* callStackFileName, const char *typeDBFile, std::vector<std::vector<std::string>*> *mergeInfo);
std::map<std::string, std::vector<HowardType*>* >* createStackTypeDB(const char* stackFileName);
std::vector<std::vector<std::string>*>* parseMergeInfo(const char* mergeInfoFileName);

void writeTypeDBToXML(const char* xmlFileName, std::map<std::string, std::vector<HowardType*>* >* typeDB, std::map<std::string, std::vector<HowardType*>* >* stackTypeDB);

#endif
