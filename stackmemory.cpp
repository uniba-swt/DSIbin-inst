#include <algorithm>
#include <iostream>

#include "stackmemory.h"

StackChunk::StackChunk(unsigned long startAddr, unsigned long size,
		std::string funcStartAddr, std::vector<HowardType *>*types) {
	this->startAddr = startAddr;
	this->setSize(size);
	this->funcStartAddr = funcStartAddr;
	this->types = types;
}
unsigned long StackChunk::getStartAddr() {
	return this->startAddr;
}
unsigned long StackChunk::getEndAddr() {
	return this->endAddr;
}
void StackChunk::setSize(unsigned long size) {
	this->size = size;
	this->endAddr = this->startAddr - size;
}
unsigned long StackChunk::getSize() {
	return this->size;
}
std::string StackChunk::getFuncStartAddr() {
	return this->funcStartAddr;
}
std::vector<HowardType *>* StackChunk::getTypes() {
	return this->types;
}
std::vector<HowardType*>* StackChunk::findTypeForOffset(unsigned long offset) {
	std::cout << "StackChunk::findTypeForOffset: offset: " << offset
			<< std::endl;
	if (this->types == NULL) {
		std::cout << "StackChunk::findTypeForOffset: no types present at all: "
				<< offset << std::endl;
		return NULL;
	}
	std::vector<HowardType*>::iterator it = this->types->begin();
	std::vector<HowardType*>*curTypes = new std::vector<HowardType*>();
	std::cout << "StackChunk::findTypeForOffset: checking types for offset: "
			<< offset << std::endl;
	for (; this->types->end() != it; it++) {
		HowardType *curType = (*it);
		unsigned long curTypeOffset = curType->getOffset();
		unsigned long curTypeSize = curType->getSize();
		std::cout << "StackChunk::findTypeForOffset: testing offset: " << offset
				<< " against curTypeOffset: " << curTypeOffset
				<< " curTypeSize: " << curTypeSize << std::endl;
		if (curType->getIsCompound()) {
			std::cout << "StackChunk::findTypeForOffset: is compound"
					<< std::endl;
			if (curTypeOffset - curTypeSize <= offset
					&& offset <= curTypeOffset) {
				std::cout
						<< "StackChunk::findTypeForOffset: offset inside range"
						<< std::endl;
				return curType->findTypeForOffset(curTypeOffset - offset);
			} else {
				std::cout
						<< "StackChunk::findTypeForOffset: is compound data type -> offset not in range!"
						<< std::endl;
			}
		} else {
			// For primitive types: offsets must match exactly
			if (curTypeOffset == offset) {
				std::cout
						<< "StackChunk::findTypeForOffset: is primitive data type"
						<< std::endl;
				// We have a primitive data type
				curTypes->push_back(curType);
				return curTypes;
			} else {
				std::cout
						<< "StackChunk::findTypeForOffset: is primitive data type -> no exact offset match!"
						<< std::endl;
			}
		}

	}
	std::cout << "StackChunk::findTypeForOffset: nothing found, returning NULL"
			<< std::endl;
	return NULL;
}

/* Stack */
Stack::Stack() {
	this->stackChunks = new std::vector<StackChunk*>();
}

// Functor: Helper to find stack chunks
struct FindStackChunkByAddress: std::unary_function<StackChunk*, bool> {
private:
	unsigned long address;
public:
	FindStackChunkByAddress(const unsigned long addr) :
			address(addr) {
	}
	bool operator()(StackChunk* chunk) {
		// Stack grows downward: startAddr > endAddr
		return chunk->getStartAddr() >= address
				&& address >= chunk->getEndAddr();
	}

};

std::vector<StackChunk*>::iterator Stack::findStackChunkByStartAddress(
		unsigned long address) {
	return std::find_if(this->stackChunks->begin(), this->stackChunks->end(),
			FindStackChunkByAddress(address));
}

bool Stack::popStackChunk() {
	std::cout << "Stack::popStackChunk(): stackChunks->size(): " << std::dec
			<< this->stackChunks->size() << std::endl;
	this->stackChunks->pop_back();
	std::cout << "Stack::popStackChunk(): stackChunks->size() after: "
			<< std::dec << this->stackChunks->size() << std::endl;
	return true;
}

bool Stack::pushStackChunk(StackChunk*chunk) {
	std::cout << "Stack::pushStackChunk(): stackChunks->size(): " << std::dec
			<< this->stackChunks->size() << std::endl;
	this->stackChunks->push_back(chunk);
	std::cout << "Stack::pushStackChunk(): stackChunks->size() after: "
			<< std::dec << this->stackChunks->size() << std::endl;
	return true;
}

StackChunk * Stack::back() {
	return this->stackChunks->back();
}

void Stack::printStack() {
	std::vector<StackChunk*>::iterator it = this->stackChunks->begin();
	std::cout << "Stack:" << std::endl;
	for (; this->stackChunks->end() != it; it++) {
		std::cout << "\tmem chunk:" << std::endl;
		std::cout << "\t\tstartAddr: " << std::hex << (*it)->getStartAddr()
				<< " endAddr: " << std::hex << (*it)->getEndAddr() << " size: "
				<< std::dec << (*it)->getSize() << " types: "
				<< (*it)->getTypes()->size()
				<< " func start addr: " << (*it)->getFuncStartAddr()
				<< std::endl;
	}
}

std::vector<StackChunk*>::iterator Stack::getStackChunksEndIt() {
	return this->stackChunks->end();
}

bool Stack::addrInRedZone(unsigned long address) {
	StackChunk *lastStackFrame = this->stackChunks->back();
	std::cout << "Stack::addrInRedZone: lastStackFrame: " << std::hex
			<< lastStackFrame << std::endl;
	std::cout << "Stack::addrInRedZone: lastStackFrame->getEndAddr: "
			<< std::hex << lastStackFrame->getEndAddr() << std::endl;
	return lastStackFrame->getEndAddr() > address;

}
