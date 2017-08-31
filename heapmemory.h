#ifndef _HEAPMEMORY_H
#define _HEAPMEMORY_H

#include <string>
#include "typeparser.h"

class HeapChunk {
	unsigned long startAddr;
	unsigned long endAddr;
	unsigned long size;
	std::string callStack;
	HowardType *type;
public:
	HeapChunk(unsigned long startAddr, unsigned long size, std::string callStack, HowardType *type);
	unsigned long getStartAddr();
	unsigned long getEndAddr();
	unsigned long getSize();
	std::string getCallStack();
	HowardType * getType();
};

class Heap {
	std::vector<HeapChunk*> *heapChunks;
public:
	Heap();
	std::vector<HeapChunk*>::iterator findHeapChunkByAddress(unsigned long address);
	bool addHeapChunk(HeapChunk*chunk);
	bool removeHeapChunkByAddress(unsigned long address);
	void printHeap();
	std::vector<HeapChunk*>::iterator getHeapChunksEndIt();
};

#endif
