#include <algorithm>
#include <iostream>

#include "heapmemory.h"

HeapChunk::HeapChunk(unsigned long startAddr, unsigned long size,
		std::string callStack, HowardType *type) {
	this->startAddr = startAddr;
	this->size = size;
	this->endAddr = startAddr + size;
	this->callStack = callStack;
	this->type = type;
}
unsigned long HeapChunk::getStartAddr() {
	return this->startAddr;
}
unsigned long HeapChunk::getEndAddr() {
	return this->endAddr;
}
unsigned long HeapChunk::getSize() {
	return this->size;
}
std::string HeapChunk::getCallStack() {
	return this->callStack;
}
HowardType * HeapChunk::getType() {
	return this->type;
}

/* Heap */
Heap::Heap() {
	this->heapChunks = new std::vector<HeapChunk*>();
}
//
// Functor: Helper to find heap chunks
struct FindHeapChunkByAddress: std::unary_function<HeapChunk*, bool> {
private:
	unsigned long address;
public:
	FindHeapChunkByAddress(const unsigned long addr) :
			address(addr) {
	}
	bool operator()(HeapChunk* chunk) {
		return chunk->getStartAddr() <= address
				&& address <= chunk->getEndAddr();
	}

};

std::vector<HeapChunk*>::iterator Heap::findHeapChunkByAddress(
		unsigned long address) {
	return std::find_if(this->heapChunks->begin(), this->heapChunks->end(),
			FindHeapChunkByAddress(address));
}

bool Heap::addHeapChunk(HeapChunk*chunk) {
	this->heapChunks->push_back(chunk);
	return true;
}

void Heap::printHeap() {
	std::vector<HeapChunk*>::iterator it = this->heapChunks->begin();
	std::cout << "Heap:" << std::endl;
	for (; this->heapChunks->end() != it; it++) {
		std::cout << "\tmem chunk:" << std::endl;
		std::cout << "\t\tstartAddr: " << std::hex << (*it)->getStartAddr()
				<< " endAddr: " << std::hex << (*it)->getEndAddr() << " size: "
				<< std::dec << (*it)->getSize() << " type: "
				<< (*it)->getType()->getType() << " member: "
				<< (*it)->getType()->getMembers()->size() << " cs: "
				<< (*it)->getCallStack() << std::endl;
	}
}

std::vector<HeapChunk*>::iterator Heap::getHeapChunksEndIt() {
	return this->heapChunks->end();
}

bool Heap::removeHeapChunkByAddress(unsigned long address) {
	std::vector<HeapChunk*>::iterator it = this->findHeapChunkByAddress(
			address);
	if (this->getHeapChunksEndIt() != it) {
		HeapChunk *chunk = (*it);
		delete chunk;
		this->heapChunks->erase(it);
		return true;
	} else {
		return false;
	}
}
