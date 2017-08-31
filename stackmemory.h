#ifndef _STACKMEMORY_H
#define _STACKMEMORY_H

#include <string>
#include "typeparser.h"

class StackChunk {
	unsigned long startAddr;
	unsigned long endAddr;
	unsigned long size;
	std::string funcStartAddr;
	// The stack contains a list of types
	std::vector<HowardType *>*types;
public:
	StackChunk(unsigned long startAddr, unsigned long size, std::string funcStartAddr, std::vector<HowardType *>*types);
	unsigned long getStartAddr();
	unsigned long getEndAddr();
	unsigned long getSize();
	void setSize(unsigned long size);
	std::string getFuncStartAddr();
	std::vector<HowardType *>*getTypes();
	std::vector<HowardType*>* findTypeForOffset(unsigned long offset);
};

class Stack {
	std::vector<StackChunk*> *stackChunks;
public:
	Stack();

	bool pushStackChunk(StackChunk*chunk);
	StackChunk* back();
	std::vector<StackChunk*>::iterator getStackChunksEndIt();
	bool popStackChunk();

	std::vector<StackChunk*>::iterator findStackChunkByStartAddress(unsigned long address);
	void printStack();

	bool addrInRedZone(unsigned long address);

};

#endif
