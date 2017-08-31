#ifndef _SHADOWSTACK_H
#define _SHADOWSTACK_H

#include "pin.H"
#include "stackmemory.h"
#include "typeparser.h"
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

class ShadowStack {
	ADDRINT last_stack_pointer_val;
	ADDRINT cur_stack_pointer_val;
	ADDRINT function_addr;
	StackChunk* stack_chunk;

public:

	ShadowStack(ADDRINT fun_addr, ADDRINT sp_val);
	~ShadowStack();

	void setStackChunk(ADDRINT cur_sp_val, ADDRINT size, std::vector<HowardType *>*types, int &event_id, std::ofstream &TraceXMLFile);

	bool hasStackFrame();
	ADDRINT getLastStackPointerVal();
	ADDRINT getCurStackPointerVal();
	ADDRINT getFunAddr();
	std::string getFunAddrAsStr();
	StackChunk* getStackChunk();

};

#endif
