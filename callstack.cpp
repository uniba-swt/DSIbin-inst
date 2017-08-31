#include <algorithm>
#include <iostream>

#include <vector>
#include "callstack.h"
#include <sstream>

void CallStack::push(ADDRINT call_addr, ADDRINT return_addr, ADDRINT start_addr,
		ADDRINT stack_pointer) {
	std::cout << "CallStack::push: entered" << std::endl;
	this->call_stack.push_back(call_addr);
	this->return_stack.push_back(return_addr);
	this->call_stack_fun_start.push_back(start_addr);
	ShadowStack* shadow = new ShadowStack(start_addr, stack_pointer);
	this->shadow_stack.push_back(shadow);
}

void CallStack::push(IMG img) {
	std::cout << "CallStack::push(IMG): entered" << std::endl;
	this->images.push_back(img);
}

void CallStack::push_fun_start(ADDRINT fun_start, ADDRINT sp) {
	this->shadow_stack_for_main = new ShadowStack(fun_start, sp);
}

void CallStack::pop_fun_start() {
	std::cout << "CallStack::pop_fun_start: removing shadow stack for main. "
			<< std::endl;
	delete this->shadow_stack_for_main;
	this->shadow_stack_for_main = NULL;
}

std::string CallStack::getLastFuncStartAddr() {
	if (call_stack_fun_start.size() > 0) {
		return this->shadow_stack.back()->getFunAddrAsStr();
	} else {
		return this->shadow_stack_for_main->getFunAddrAsStr();
	}
}

bool CallStack::addrInRedZone(unsigned long address) {
	std::cout << "CllStack::addrInRedZone: address: " << std::hex << address
			<< std::endl;
	ShadowStack *shadow_chunk;
	if (this->shadow_stack.size() > 0) {
		std::cout
				<< "CllStack::addrInRedZone: checking shadow_stack.back() address: "
				<< std::hex << address << std::endl;
		shadow_chunk = this->shadow_stack.back();
	} else {
		std::cout
				<< "CllStack::addrInRedZone: checking shadow_stack_for_main address: "
				<< std::hex << address << std::endl;
		shadow_chunk = this->shadow_stack_for_main;
	}

	if (shadow_chunk->hasStackFrame()
			&& shadow_chunk->getStackChunk()->getEndAddr() > address) {
		std::cout << "CllStack::addrInRedZone: with stack frame: " << std::endl;
		return true;
	} else if (!shadow_chunk->hasStackFrame()
			&& shadow_chunk->getLastStackPointerVal() > address) {
		std::cout << "CllStack::addrInRedZone: without stack frame: "
				<< std::endl;
		return true;
	}

	std::cout << "CllStack::addrInRedZone: no red zone: " << std::endl;
	return false;
}

bool compareStackChunkAgainstAddr(StackChunk *stack_chunk,
		unsigned long address) {
	return (stack_chunk->getStartAddr() >= address
			&& address >= stack_chunk->getEndAddr());
}

ShadowStack *CallStack::getLastShadowStack() {
	ShadowStack *shadow_chunk = NULL;

	if (this->shadow_stack.size() > 0) {
		shadow_chunk = this->shadow_stack.back();

	} else {
		shadow_chunk = this->shadow_stack_for_main;
	}
	
	return shadow_chunk;
}

StackChunk* CallStack::findStackChunkByAddress(unsigned long address) {

	std::cout << "CallStack::findStackChunkByAddress: address: " << std::hex
			<< address << std::endl;

	if (this->shadow_stack_for_main != NULL
			&& this->shadow_stack_for_main->hasStackFrame()
			&& compareStackChunkAgainstAddr(
					this->shadow_stack_for_main->getStackChunk(), address)) {
		std::cout
				<< "CallStack::findStackChunkByAddress: found in main: address: "
				<< std::hex << address << std::endl;
		return this->shadow_stack_for_main->getStackChunk();
	}

	std::cout
			<< "CallStack::findStackChunkByAddress: not found in main: address: "
			<< std::hex << address << std::endl;
	std::vector<ShadowStack*>::iterator it = this->shadow_stack.begin();
	for (; this->shadow_stack.end() != it; it++) {
		ShadowStack *shadow_chunk = *it;
		if (shadow_chunk->hasStackFrame()) {
			StackChunk *stack_chunk = shadow_chunk->getStackChunk();
			if (compareStackChunkAgainstAddr(stack_chunk, address)) {
				std::cout
						<< "CallStack::findStackChunkByAddress: found address: "
						<< std::hex << address << std::endl;
				return stack_chunk;
			}
		}
	}
	std::cout << "CallStack::findStackChunkByAddress: not found address: "
			<< std::hex << address << std::endl;

	return NULL;
}

std::string CallStack::getLastFuncStartAddrImgOffset() {
	ADDRINT funAddr;
	if (call_stack_fun_start.size() > 0) {
		funAddr = this->shadow_stack.back()->getFunAddr();
	} else {
		funAddr = this->shadow_stack_for_main->getFunAddr();
	}
	unsigned long imgLowAddr = this->findLowAddressForAddr(funAddr);
	stringstream ss;
	ss << std::hex << (funAddr - imgLowAddr);
	return ss.str();
}

unsigned long CallStack::findLowAddressForAddr(ADDRINT addr) {
	std::vector<IMG>::iterator it = images.begin();
	for (; images.end() != it; it++) {
		IMG tmpImg = *it;
		if (IMG_HighAddress(tmpImg) >= addr && IMG_LowAddress(tmpImg) <= addr) {
			return IMG_LowAddress(tmpImg);
		}
	}
	std::cerr << "Not Found Image for addr: " << std::hex << addr << endl;
	return 0;
}

bool CallStack::addrIsInMainImg(ADDRINT addr) {
	PIN_LockClient();
	IMG tmpImg = IMG_FindByAddress(addr);
	PIN_UnlockClient();
	return IMG_IsMainExecutable(tmpImg);
}

std::string CallStack::findImgName(ADDRINT addr) {
	PIN_LockClient();
	IMG tmpImg = IMG_FindByAddress(addr);
	PIN_UnlockClient();
	std::stringstream ss;
	if (IMG_Valid(tmpImg)) {
		ss << "(" << IMG_IsMainExecutable(tmpImg) << ")";
		return ss.str();
	}
	return "noImgFound";
}

bool CallStack::pop(ADDRINT return_addr, int &event_id,
		std::ofstream &TraceXMLFile,
		std::map<std::string, std::vector<HowardType*>*>* stackTypeDB) {
	std::cout << "CallStack::pop: entered" << std::endl;

	if (this->call_stack.size() != this->return_stack.size()
			|| this->call_stack.size() != this->call_stack_fun_start.size()
			|| this->return_stack.size() != this->call_stack_fun_start.size()) {
		std::cerr << "CallStack::pop: size missmatch" << std::endl;
		std::cerr << "call_stack.size(): " << this->call_stack.size()
				<< std::endl;
		std::cerr << "call_stack_fun_start.size(): "
				<< this->call_stack_fun_start.size() << std::endl;
		std::cerr << "return_stack.size(): " << this->return_stack.size()
				<< std::endl;
		exit(1);
	}

	std::vector<ADDRINT>::reverse_iterator return_rit =
			this->return_stack.rbegin();
	std::vector<ADDRINT>::reverse_iterator call_rit = this->call_stack.rbegin();
	std::vector<ADDRINT>::reverse_iterator start_rit =
			this->call_stack_fun_start.rbegin();
	std::vector<ShadowStack*>::reverse_iterator shadow_rit =
			this->shadow_stack.rbegin();
	for (; this->return_stack.rend() != return_rit;
			return_rit++, start_rit++, call_rit++) {
		ADDRINT curItem = (*return_rit);
		if (curItem == return_addr) {
			std::cout << "CallStack::pop: found match: " << std::hex
					<< return_addr << " == " << std::hex << curItem
					<< std::endl;
			std::vector<ADDRINT>::iterator temp1 = return_rit.base();
			temp1--;
			this->return_stack.erase(temp1, this->return_stack.end());
			std::vector<ADDRINT>::iterator temp2 = call_rit.base();
			temp2--;
			this->call_stack.erase(temp2, this->call_stack.end());
			std::vector<ADDRINT>::iterator temp3 = start_rit.base();
			temp3--;
			this->call_stack_fun_start.erase(temp3,
					this->call_stack_fun_start.end());

			// First we need to iterate over all recorded stack frames 
			// to create variable leaving scope events
			std::vector<ShadowStack*>::iterator temp4 = shadow_rit.base();
			temp4--;
			std::vector<ShadowStack*>::iterator shadow_it = temp4;
			for (; this->shadow_stack.end() != shadow_it; shadow_it++) {
				ShadowStack *shadow = (*shadow_it);
				std::cout << "CallStack::pop: processing shadow: " << std::hex
						<< shadow->getFunAddr() << " has stack: "
						<< shadow->hasStackFrame() << std::endl;
				if (shadow->hasStackFrame()) {

					StackChunk *chunk = shadow->getStackChunk();
					std::cout
							<< "CallStack::pop: shadow->hasStackFrame: checking func start addr: "
							<< std::hex << chunk->getFuncStartAddr()
							<< std::endl;
					unsigned long spAddr = chunk->getStartAddr();
					std::vector<HowardType *>* types = chunk->getTypes();
					if (types != NULL) {
						std::cout << "CallStack::pop: stackTypeDB found chunk: "
								<< std::endl;
						std::vector<HowardType *>::iterator typesIt =
								types->begin();
						for (; types->end() != typesIt; typesIt++) {
							event_id++;
							TraceXMLFile << "<event id=\"" << std::dec
									<< event_id << "\">" << std::endl;
							TraceXMLFile << "<variable-left-scope>"
									<< std::endl;
							TraceXMLFile << "<name>dontcare</name>"
									<< std::endl;
							TraceXMLFile << "<address>" << std::hex
									<< (spAddr - (*typesIt)->getOffset())
									<< "</address>" << std::endl;
							TraceXMLFile << "</variable-left-scope>"
									<< std::endl;
							TraceXMLFile << "</event>" << std::endl;
						}
					}
					std::cout << "CallStack::pop: after stackTypeDB chunk: "
							<< std::endl;
				}
				std::cout << "CallStack::pop: after shadow->hasStackFrame "
						<< std::endl;
			}
			std::vector<ShadowStack*>::iterator temp5 = shadow_rit.base();
			temp5--;
			this->shadow_stack.erase(temp5, this->shadow_stack.end());

			return true;
		}
	}

	return false;
}

void CallStack::addStackChunk(ADDRINT sp, ADDRINT size,
		std::vector<HowardType*> *types, int &event_id,
		std::ofstream &TraceXMLFile) {
	if (this->shadow_stack.size() > 0) {
		this->shadow_stack.back()->setStackChunk(sp, size, types, event_id,
				TraceXMLFile);
	} else {
		this->shadow_stack_for_main->setStackChunk(sp, size, types, event_id,
				TraceXMLFile);
	}
}

std::string CallStack::last_callstack_element(bool checkForMain) {
	std::stringstream ss;
	if (this->call_stack.size() > 0) {
		std::string full_call_stack = this->print_call();
		std::size_t start_last_elem = full_call_stack.rfind(",",
				full_call_stack.length() - 2);
		if (start_last_elem != std::string::npos) {
			ss << full_call_stack.substr(start_last_elem + 1);
		} else {
			ss << full_call_stack;
		}
	}
	return ss.str();
}

std::string CallStack::print_call(bool checkForMain) {

	std::stringstream ss;
	std::vector<ADDRINT>::iterator it = this->call_stack.begin();
	for (; this->call_stack.end() != it; it++) {
		unsigned long addr = *it;
		unsigned long imgLowAddr = findLowAddressForAddr(addr);
		// When using this method to fetch type information via call stack
		// non main functions need to be filtered out. For simple printing 
		// everything should be considered
		if (checkForMain && addrIsInMainImg(addr)) {
			ss << std::hex << ((*it) - imgLowAddr) << ",";
		} else if (!checkForMain) {
			ss << std::hex << ((*it) - imgLowAddr) << ",";
		}
	}
	return ss.str();
}

std::string formatStackChunk(StackChunk *stackChunk) {
	std::vector<HowardType*> *types = stackChunk->getTypes();
	stringstream stack_chunks;
	stack_chunks << "\t\tstartAddr: " << std::hex << stackChunk->getStartAddr()
			<< " endAddr: " << std::hex << stackChunk->getEndAddr() << " size: "
			<< std::hex << stackChunk->getSize() << "(dec: " << std::dec
			<< stackChunk->getSize() << ") " << " types: "
			<< (types == NULL ? 0 : types->size())
			<< " func start addr: " << stackChunk->getFuncStartAddr()
			<< std::endl;
	return stack_chunks.str();
}

std::string CallStack::print_start() {

	std::stringstream ss;
	std::stringstream stack_chunks;
	std::vector<ShadowStack*>::iterator it = this->shadow_stack.begin();

	stack_chunks << "\tmem chunk:" << std::endl;

	if (this->shadow_stack_for_main != NULL
			&& this->shadow_stack_for_main->hasStackFrame()) {
		stack_chunks
				<< formatStackChunk(
						this->shadow_stack_for_main->getStackChunk());
	}

	for (; this->shadow_stack.end() != it; it++) {
		unsigned long lowAddr = findLowAddressForAddr((*it)->getFunAddr());
		ss << std::hex << ((*it)->getFunAddr() - lowAddr) << ",";

		if ((*it)->getStackChunk() != NULL) {
			stack_chunks << formatStackChunk((*it)->getStackChunk());
		} else {
			stack_chunks << "\t no stack chunk stored, yet: last sp: "
					<< std::hex << (*it)->getLastStackPointerVal()
					<< " cur sp: " << std::hex << (*it)->getCurStackPointerVal()
					<< " func start addr: " << (*it)->getFunAddr() << std::endl;
		}
	}
	ss << std::endl << stack_chunks.str();
	return ss.str();
}

std::string CallStack::print_return() {

	std::stringstream ss;
	std::vector<ADDRINT>::iterator it = this->return_stack.begin();
	for (; this->return_stack.end() != it; it++) {
		unsigned long lowAddr = findLowAddressForAddr(*it);
		ss << std::hex << ((*it) - lowAddr) << ",";
	}
	return ss.str();
}

void CallStack::print() {
	std::cout << "CallStack::print: call_stack          : "
			<< this->print_call(false) << std::endl;
	std::cout << "CallStack::print: return_stack        : "
			<< this->print_return() << std::endl;
	std::cout << "CallStack::print: call_stack_fun_start: "
			<< this->print_start() << std::endl;
}
