#ifndef _CALLSTACK_H
#define _CALLSTACK_H

#include "pin.H"
#include <string>
#include <fstream>
#include "shadowstack.h"

class CallStack {
	std::vector<IMG> images;
	std::vector<ADDRINT> return_stack;
	std::vector<ADDRINT> call_stack;
	ShadowStack* shadow_stack_for_main;
	std::vector<ShadowStack*> shadow_stack;
	ADDRINT first_fun_start;
	std::vector<ADDRINT> call_stack_fun_start;
	bool addrIsInMainImg(ADDRINT addr);
	std::string findImgName(ADDRINT addr);

public:
	void push_fun_start(ADDRINT fun_start, ADDRINT sp);
	void pop_fun_start();
	std::string getLastFuncStartAddr();
	std::string getLastFuncStartAddrImgOffset();
	unsigned long findLowAddressForAddr(ADDRINT addr);
	void push(IMG img);
	void push(ADDRINT call_addr, ADDRINT return_addr, ADDRINT start_addr, ADDRINT stack_pointer);
	// bool indicates, if the item was present at all.
	// pop() also removes all items in front of 'item'
	// which might be the case with 'fake calls'. This
	// case is also simply reported with a 'true' 
	// return value.
	bool pop(ADDRINT return_addr, int &event_id, std::ofstream &TraceXMLFile, std::map<std::string, std::vector<HowardType*>* >* stackTypeDB);

	StackChunk* findStackChunkByAddress(unsigned long address);
	void addStackChunk(ADDRINT sp, ADDRINT size, std::vector<HowardType*> *types, int &event_id, std::ofstream &TraceXMLFile);
	ShadowStack *getLastShadowStack();
	bool addrInRedZone(unsigned long address);

	std::string print_call(bool checkForMain = true);
	std::string last_callstack_element(bool checkForMain = true);
	std::string print_return();
	std::string print_start();
	void print();
};

#endif
