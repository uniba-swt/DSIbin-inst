/*BEGIN_LEGAL 
 Intel Open Source License

 Copyright (c) 2002-2015 Intel Corporation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met:

 Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.  Redistributions
 in binary form must reproduce the above copyright notice, this list of
 conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.  Neither the name of
 the Intel Corporation nor the names of its contributors may be used to
 endorse or promote products derived from this software without
 specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
 ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 END_LEGAL */

#include "pin.H"
#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <vector>
#include <map>
#include "typeparser.h"
#include "heapmemory.h"
#include "stackmemory.h"
#include "objdumpparser.h"
#include "typerefiner.h"
#include "callstack.h"

/* ===================================================================== */
/* Names of malloc and free */
/* ===================================================================== */
#if defined(TARGET_MAC)
#define MALLOC "_malloc"
#define FREE "_free"
#else
#define MALLOC "malloc"
#define FREE "free"
#endif

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

std::ofstream TraceFile;
std::ofstream TraceXMLFile;

std::map<std::string, std::vector<HowardType*>*>* typeDB;
std::map<std::string, std::vector<HowardType*>*>* stackTypeDB;

ADDRINT last_size;
ADDRINT last_ret;
int event_id = 0;

// Flag to indicate that main was called and that the
// tracing should start
int start_trace = 0;
Heap *heap = new Heap();
Stack *stack = new Stack();

CallStack *callStackReturnAddr = new CallStack();
/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o",
		"malloctrace.out", "specify trace file name");
KNOB<string> KnobInputFile(KNOB_MODE_WRITEONCE, "pintool", "i", "-",
		"specify file name for input files");
KNOB<string> KnobPath(KNOB_MODE_WRITEONCE, "pintool", "p", "-",
		"specify absolut path of project");
KNOB<string> KnobCallStackPath(KNOB_MODE_WRITEONCE, "pintool", "c", "-",
		"specify absolut path of callstack file");
KNOB<string> KnobWritePath(KNOB_MODE_WRITEONCE, "pintool", "w", "-",
		"specify absolut path for writing xml files. Can end with the beginning of a file name, as trace/types.xml will be simply attached to string.");

/* ===================================================================== */

/* ===================================================================== */
/* Forward declerations */
/* ===================================================================== */

/* ===================================================================== */
/* ===================================================================== */
/* Analysis routines                                                     */
/* ===================================================================== */

VOID Arg1Before(CHAR * name, ADDRINT size, ADDRINT sp, ADDRINT funcStartAddr,
		ADDRINT addr) {
	if (start_trace) {
		std::cout << " malloc called " << name << " (" << size << ")" << endl;
		TraceFile << name << "(" << size << ")" << endl;
		last_size = size;
		last_ret = 0;
		TraceFile << "Arg1Before: address: " << std::hex << addr
				<< " funcStartAddr: " << std::hex << funcStartAddr
				<< " stack pointer: " << std::hex << sp << endl;
		/*call_stack.push_back(addr);
		 std::cout << "\tstack after push: " << std::endl;
		 printStack();*/
	} else {
		TraceFile << "  malloc not enabled yet " << endl;
	}
}

VOID FreeArg1Before(CHAR * name, ADDRINT addr) {
	TraceFile << name << "(" << std::hex << addr << ")" << endl;
	event_id++;
	TraceXMLFile << "<event id=\"" << std::dec << event_id << "\">" << endl;
	TraceXMLFile << "<free>" << endl;
	TraceXMLFile << "<sourceLocation>" << endl;
	TraceXMLFile << "<file>dontcare</file>" << endl;
	TraceXMLFile << "<line>0</line>" << endl;
	TraceXMLFile << "<column>0</column>" << endl;
	TraceXMLFile << "</sourceLocation>" << endl;
	TraceXMLFile << "<argCodeFragment>dontcare</argCodeFragment>" << endl;
	TraceXMLFile << "<argValue>" << std::hex << addr << "</argValue>" << endl;
	TraceXMLFile << "</free>" << endl;
	TraceXMLFile << "</event>" << endl;
	std::cout << "Freeing heap memory chunk..." << std::endl;
	std::cout << "Heap before free:" << std::endl;
	heap->printHeap();
	heap->removeHeapChunkByAddress(addr);
	std::cout << "Heap after free:" << std::endl;
	heap->printHeap();
}

VOID MallocAfter(ADDRINT ret) {
	TraceFile << "  returns " << std::hex << ret << endl;
	event_id++;
	std::string full_stack = callStackReturnAddr->print_call();
	// Here one can switch between merging the call stack based on the 
	// last element (BUT you need to prepare the howard files accordingly)
	//std::string stack = callStackReturnAddr->last_callstack_element();
	std::string stack = callStackReturnAddr->print_call();
	TraceFile << "checking call stack: " << stack << " full call stack: "
			<< full_stack << std::endl;
	std::map<std::string, std::vector<HowardType*>*>::iterator csTypeIt =
			typeDB->find(stack);
	std::string mallocedType = "";
	if (typeDB->end() != csTypeIt) {
		HowardType *foundType = (csTypeIt->second)->front();
		mallocedType = foundType->getType();
		if (foundType->getSize() < last_size) {
			TraceFile << "size of type " << mallocedType
					<< " is smaller than malloced size, expanding: type size: "
					<< foundType->getSize() << " malloced size: " << last_size
					<< std::endl;
			std::cout << "size of type " << mallocedType
					<< " is smaller than malloced size, expanding: type size: "
					<< foundType->getSize() << " malloced size: " << last_size
					<< std::endl;
			foundType->setSizeAndEnd(last_size);
			std::cout << "new size of type " << mallocedType << " is: "
					<< foundType->getSize() << std::endl;
		}
		TraceFile << "call stack: " << stack << " has type: " << mallocedType
				<< std::endl;
		HeapChunk *heapChunk = new HeapChunk((unsigned long int) ret,
				(unsigned long int) last_size, stack,
				(csTypeIt->second)->front());
		heap->addHeapChunk(heapChunk);
		heap->printHeap();
		TraceXMLFile << "<!-- call stack found: " << stack << " -->\n"
				<< std::endl;
	} else {
		mallocedType = "error";
		TraceFile << "call stack not found: ." << stack << "." << std::endl;
		std::cout << "call stack not found: ." << stack << "." << std::endl;
		TraceXMLFile << "<!-- call stack not found: ." << stack << ". -->\n"
				<< std::endl;
	}

	// Need to simulate the return, as malloc is not tracked via callInstruction/returnInstruction
	//returnInstruction();

	TraceXMLFile << "<event id=\"" << std::dec << event_id << "\">" << endl;
	TraceXMLFile
			<< "<memory-write>\n\
		<sourceLocation>\n\
		<file>sll-with-slls-same-type.c</file>\n\
		<line>14</line>\n\
		<column>0</column>\n\
		</sourceLocation>\n\
		<lval>\n\
		<address>"
			<< "0x08" << "</address>\n\
		<type>struct " << mallocedType
			<< " *</type>\n\
		<codeFragment>statically chosen 0x08 as address</codeFragment>\n\
		</lval>\n\
		<content>\n\
		<content>"
			<< std::hex << ret << "</content>\n\
		<lvalDerefType>struct "
			<< mallocedType
			<< "</lvalDerefType>\n\
		<rhsCodeFragment>malloc</rhsCodeFragment>\n\
		</content>\n\
		<memory-allocation>\n\
		<malloc>\n\
		<argCodeFragment>sizeof(*(*start))</argCodeFragment>\n\
		<argValue>"
			<< std::dec << last_size
			<< "</argValue>\n\
		</malloc>\n\
		</memory-allocation>\n\
		</memory-write>\n\
		</event>"
			<< endl;

}

/* ===================================================================== */
/* Instrumentation routines                                              */
/* ===================================================================== */
unsigned long offsetInImage(ADDRINT addr) {
	unsigned long lowImgAddr = callStackReturnAddr->findLowAddressForAddr(addr);
	return addr - lowImgAddr;
}

VOID startTrace(ADDRINT sp, ADDRINT funcStartAddr) {
	unsigned long imgOffset = offsetInImage(funcStartAddr);
	std::cout << "start trace: value of SP(!): " << std::hex << sp
			<< " function start addr: " << funcStartAddr << " offset in image: "
			<< imgOffset << endl;

	// Here it seems, that sp is already decremented. 
	// See other sp recording in callInstruction for offset!
	callStackReturnAddr->push_fun_start(funcStartAddr, sp);
	callStackReturnAddr->print();

	std::cout << "  startTrace: " << endl;
	start_trace = 1;
}

VOID startTraceAfter() {

	callStackReturnAddr->pop_fun_start();
	callStackReturnAddr->print();

	std::cout << "start traceAfter: " << endl;
	start_trace = 0;
}

VOID Image(IMG img, VOID *v) {
	callStackReturnAddr->push(img);

	// Instrument the malloc() and free() functions.  Print the input argument
	// of each malloc() or free(), and the return value of malloc().
	RTN mainRtn = RTN_FindByName(img, "main");
	if (RTN_Valid(mainRtn)) {
		RTN_Open(mainRtn);
		ADDRINT function_address = RTN_Address(mainRtn);
		std::cout << "intercepting main: start address: " << std::hex
				<< function_address << endl;
		// Instrument main() to print the input argument value and the return value.
		RTN_InsertCall(mainRtn, IPOINT_BEFORE, (AFUNPTR) startTrace,
				IARG_REG_VALUE, REG_STACK_PTR, IARG_ADDRINT, function_address,
				IARG_END);

		RTN_InsertCall(mainRtn, IPOINT_AFTER, (AFUNPTR) startTraceAfter,
				IARG_END);
		RTN_Close(mainRtn);

	}

	//  Find the malloc() function.
	RTN mallocRtn = RTN_FindByName(img, MALLOC);
	if (RTN_Valid(mallocRtn)) {
		RTN_Open(mallocRtn);

		ADDRINT function_address = RTN_Address(mallocRtn);
		// Instrument malloc() to print the input argument value and the return value.
		RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR) Arg1Before,
				IARG_ADDRINT, MALLOC, IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
				IARG_REG_VALUE, REG_STACK_PTR, IARG_ADDRINT, function_address,
				IARG_INST_PTR, IARG_RETURN_IP, IARG_END);
		RTN_InsertCall(mallocRtn, IPOINT_AFTER, (AFUNPTR) MallocAfter,
				IARG_FUNCRET_EXITPOINT_VALUE, IARG_END);

		RTN_Close(mallocRtn);
	}

	// Find the free() function.
	RTN freeRtn = RTN_FindByName(img, FREE);
	if (RTN_Valid(freeRtn)) {
		RTN_Open(freeRtn);
		// Instrument free() to print the input argument value.
		RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR) FreeArg1Before,
				IARG_ADDRINT, FREE, IARG_FUNCARG_ENTRYPOINT_VALUE, 0, IARG_END);
		RTN_Close(freeRtn);
	}
}

/* ===================================================================== */
VOID *last_IP = NULL;

// Print a memory read record
VOID RecordMemRead(VOID * ip, VOID * addr, UINT32 memOp) {

	int readVal = *((int*) addr);
	if (last_IP == ip) {
		TraceFile << std::hex << ip << ": ip not changed R " << std::hex << addr
				<< " value: " << std::dec << readVal << endl;
	} else {
		TraceFile << std::hex << ip << ": ip changed     R " << std::hex << addr
				<< " value: " << std::dec << readVal << endl;
	}

	last_IP = ip;
}
// Print a memory write record
VOID RecordMemWrite(VOID * ip, VOID * addr, UINT32 memOp)
		{
	if (last_IP == ip) {
		TraceFile << std::hex << ip << ": ip not changed W " << std::hex << addr
				<< endl;
	} else {
		TraceFile << std::hex << ip << ": ip changed     W " << std::hex << addr
				<< endl;
	}
	last_IP = ip;
}
VOID RecordMemWriteAfter(VOID * ip, VOID * addr, UINT32 memOp) {
	int readVal = *((int*) addr);
	TraceFile << std::hex << ip << ": after W " << std::hex << addr
			<< " value: " << std::hex << *((int*) addr) << " dec: " << std::dec
			<< readVal << endl;
}

HowardType* getTypeForAddressTarget(unsigned long offset,
		std::vector<HowardType*> *foundTypes) {
	// At the moment always return the innermost (most specific) struct
	// => start from the back and return first compound element
	std::vector<HowardType*>::reverse_iterator foundTypesIt =
			foundTypes->rbegin();
	for (; foundTypes->rend() != foundTypesIt; foundTypesIt++) {
		HowardType *sub = (*foundTypesIt);
		if (sub->getIsCompound()) {
			return sub;
		}
	}
	TraceFile << "getTypeForAddressTarget: nothing found." << endl;
	return NULL;
}

HowardType* getTypeForAddressSource(unsigned long offset,
		std::vector<HowardType*> *foundTypes) {
	std::cout << "getTypeForAddressSource: entered." << endl;
	return foundTypes->size() > 0 ? foundTypes->back() : NULL;
}

HowardType* getTypeForAddress(unsigned long addr,
		HowardType*(*typeProducer)(unsigned long, std::vector<HowardType*>*)) {
	std::vector<HeapChunk*>::iterator memChunkIt2 =
			heap->findHeapChunkByAddress(addr);
	if (heap->getHeapChunksEndIt() != memChunkIt2) {
		unsigned long offset = (unsigned long) addr
				- (*memChunkIt2)->getStartAddr();
		cout << "Addr " << std::hex << addr
				<< " is located on heap chunk with type: "
				<< (*memChunkIt2)->getType()->getType() << endl;
		TraceXMLFile << "<!-- " << "Addr " << std::hex << addr
				<< " is located on heap chunk with type: "
				<< (*memChunkIt2)->getType()->getType() << "-->\n" << std::endl;
		cout << "\toffset: " << offset << std::endl;
		HowardType *type = (*memChunkIt2)->getType();
		std::vector<HowardType*> *foundTypes = type->findTypeForOffset(offset);

		std::vector<HowardType*>::iterator foundTypesIt = foundTypes->begin();
		for (; foundTypes->end() != foundTypesIt; foundTypesIt++) {
			HowardType *sub = (*foundTypesIt);
			cout << "\ttype:" << sub->getType() << " offset: "
					<< sub->getOffset() << endl;
			TraceXMLFile << "<!-- \ttype:" << sub->getType() << " offset: "
					<< sub->getOffset() << "-->" << std::endl;
		}
		// Let the typeProducer decide which type to return here
		cout << "\tcalling typeProducer:" << endl;
		TraceXMLFile << "<!-- calling typeProducer: found on heap addr: "
				<< std::hex << addr << "-->\n" << std::endl;
		HowardType * retType = typeProducer(offset, foundTypes);
		if (retType == NULL) {
			TraceXMLFile << "<!-- calling typeProducer: returned NULL -->\n"
					<< std::endl;
		} else {
			TraceXMLFile << "<!-- calling typeProducer: returned "
					<< retType->getType() << " -->\n" << std::endl;
		}
		return retType;
	} else {
		TraceXMLFile << "<!-- getTypeForAddress: not found on heap addr: "
				<< std::hex << addr << "-->\n" << std::endl;
		cout << "Addr not found on heap: " << std::hex << addr << endl;
		return NULL;
	}
}

std::vector<HowardType*> * inspectStackAddress(unsigned long addr) {
	StackChunk* memChunk = callStackReturnAddr->findStackChunkByAddress(addr);
	std::cout << "inspectStackAddress: addr: " << std::hex << addr << std::endl;

	// Addr was not found, see if this is a red zone access
	if (memChunk == NULL) {
		// This part should only be executed if we actually have a red zone access
		// and there was no previous red zone access which already subsumes the current one
		bool isInRedZone = callStackReturnAddr->addrInRedZone(addr);

		if (!isInRedZone) {
			std::cout << "addr not found in red zone: " << endl;
			return NULL;
		}

		std::cout << "addr " << std::hex << addr << " is in red zone: "
				<< std::endl;
		std::string funcStartAddr = callStackReturnAddr->getLastFuncStartAddr();

		std::map<std::string, std::vector<HowardType*>*>::iterator csTypeIt =
				stackTypeDB->find(funcStartAddr);

		if (stackTypeDB->end() != csTypeIt) {
			std::cout << "Found types for funcStartAddr: " << funcStartAddr
					<< std::endl;
			callStackReturnAddr->addStackChunk(addr, 0, (*csTypeIt).second,
					event_id, TraceXMLFile);
		} else {
			std::cout << "No entry found for funcStartAddr: " << funcStartAddr
					<< endl;
			callStackReturnAddr->addStackChunk(addr, 0, NULL, event_id,
					TraceXMLFile);
		}
	}

	// Try again with red zone fix
	StackChunk* stackChunk = callStackReturnAddr->findStackChunkByAddress(addr);
	if (stackChunk != NULL) {
		std::cout << "addr: " << std::hex << addr
				<< " found on live stack! stack belongs to function: "
				<< stackChunk->getFuncStartAddr() << endl;
		// Stack grows downward: start address >= addr
		std::cout << "addr: " << std::hex << addr
				<< " stackChunk->getStartAddr(): " << stackChunk->getStartAddr()
				<< endl;
		unsigned long offset = stackChunk->getStartAddr()
				- (unsigned long) addr;
		std::cout << "\toffset: " << offset << " (dec: " << std::dec << offset
				<< ")" << std::endl;
		std::vector<HowardType*> *foundTypes = stackChunk->findTypeForOffset(
				offset);
		std::cout << "findTypeForOffset done" << std::endl;
		if (foundTypes != NULL) {
			std::vector<HowardType*>::iterator foundTypesIt =
					foundTypes->begin();

			for (; foundTypes->end() != foundTypesIt; foundTypesIt++) {
				HowardType *sub = (*foundTypesIt);
				cout << "\ttype:" << sub->getType() << " offset: "
						<< sub->getOffset() << endl;
			}
			return foundTypes;
		} else {
			std::cout << "no type found for offset: " << std::hex << offset
					<< endl;
			return NULL;
		}
	} else {
		std::cout << "addr: " << std::hex << addr << " not found on live stack!"
				<< endl;
	}
	return NULL;
}

VOID RecordMemWriteRegToMem(VOID *disassembled, CONTEXT *ctxt, VOID * ip,
		VOID * addr, REG reg, ADDRINT regval) {

	char *disassembledChar = (static_cast<char*>(disassembled));
	if (!start_trace) {
		std::cout << "RecordMemWriteRegToMem not enabled yet" << std::endl;
		return;
	}
	TraceFile << std::hex << ip << ": W to addr: " << std::hex << addr << endl;
	TraceFile << "  Reg : " << REG_StringShort(reg) << " Value: " << regval
			<< endl;
	std::cout << std::hex << ip << ": W to addr: " << std::hex << addr << endl;
	std::cout << "  Reg : " << REG_StringShort(reg) << " Value: " << regval
			<< endl;

	cout << "Inspecting regval, which is the address of the target" << endl;
	TraceXMLFile << "<!-- inspecting instruction: " << hex << ip << ": "
			<< disassembledChar << "-->\n" << std::endl;
	cout << "<!-- inspecting instruction: " << hex << ip << ": "
			<< disassembledChar << "-->\n" << std::endl;
	TraceXMLFile << "<!-- inspecting target -->\n" << std::endl;
	HowardType *targetType = getTypeForAddress((unsigned long) regval,
			&getTypeForAddressTarget);

	cout << "Inspecting addr, which is the address of the source" << endl;
	TraceXMLFile << "<!-- inspecting source -->\n" << std::endl;
	HowardType *sourceType = getTypeForAddress((unsigned long) addr,
			&getTypeForAddressSource);

	// First try to inspect the source, if it is not found on the heap
	if (sourceType == NULL) {
		TraceXMLFile << "<!-- not found on heap addr: " << std::hex << addr
				<< "-->\n" << std::endl;
		std::cout << "inspecting sourceType: " << std::hex << addr << endl;
		TraceXMLFile << "<!-- inspectStackAddress addr: " << std::hex << addr
				<< "-->\n" << std::endl;
		std::vector<HowardType*> *types = inspectStackAddress(
				(unsigned long) addr);
		TraceXMLFile << "<!-- inspectStackAddress done addr: " << std::hex
				<< addr << "-->\n" << std::endl;
		if (types == NULL) {
			TraceXMLFile << "<!-- not found on stack addr: " << std::hex << addr
					<< "-->\n" << std::endl;
			std::cout << "\tno type found for: " << std::hex << addr
					<< std::endl;
			std::cout << "\tstopping." << std::endl;
			return;
		} else {
			std::cout << "\ttypes found for addr " << std::hex << addr << ": "
					<< types->size() << std::endl;
			TraceXMLFile << "<!-- found on stack addr: " << std::hex << addr
					<< "-->\n" << std::endl;
			if (types->size() <= 0) {
				std::cout << "\ttypes size not greater 0. skipping"
						<< std::endl;
				return;
			}
			// The innermost type should be the primitive type:
			HowardType *primitiveType = types->back();
			if (primitiveType->getIsCompound()) {
				std::cout
						<< "\terror! found a compound type instead of a primitive type: "
						<< primitiveType->getType() << std::endl;
				std::cout << "\tstopping." << std::endl;
				TraceXMLFile << "<!-- compound found for addr: " << std::hex
						<< addr << "-->\n" << std::endl;
				return;
			} else {
				std::cout << "\tfound type for addr " << std::hex << addr
						<< ": " << types->size() << std::endl;
				TraceXMLFile << "<!-- found primitiveType "
						<< primitiveType->getType() << " for addr: " << std::hex
						<< addr << "-->\n" << std::endl;
				sourceType = primitiveType;
			}
			TraceXMLFile << "<!-- done with stack addr: " << std::hex << addr
					<< "-->\n" << std::endl;
		}
	}

	// Now check, if the source type is a pointer. If not, skip!
	if (sourceType->getType().find("VOID*") != std::string::npos ||
	// At the moment treat INT64 like a pointer, as this needs to be fixed by Chen
			sourceType->getType().find("INT64") != std::string::npos) {
		std::cout << "\tfound pointer type for source: "
				<< sourceType->getType() << std::endl;
	} else {
		std::cout << "\tsource is not of pointer type: "
				<< sourceType->getType() << std::endl;
		std::cout << "\tstopping." << std::endl;
		TraceXMLFile << "<!-- skipping write as type is not a pointer: "
				<< sourceType->getType() << " ip: " << std::hex << ip
				<< ": W to addr: " << std::hex << addr << "-->\n" << std::endl;
		return;
	}

	// We found a suitable source type (pointer). Now check if the target was not found on the heap.
	if (targetType == NULL) {
		std::cout << "inspecting targetType: " << std::hex << regval << endl;
		std::vector<HowardType*> *types = inspectStackAddress(
				(unsigned long) regval);
		if (types == NULL) {
			std::cout << "\tno type found for: " << std::hex << regval
					<< std::endl;
			std::cout << "\tstopping." << std::endl;
			return;
		} else {
			std::cout << "\ttypes found for addr " << std::hex << regval << ": "
					<< types->size() << std::endl;
			// Offset does not matter: 0
			targetType = getTypeForAddressTarget(0, types);
			// Still not found
			if (targetType == NULL) {
				std::cout << "\ttype producer failed! stopping. " << std::endl;
				return;
			}
		}
	}

	event_id++;

	TraceXMLFile << "<event id=\"" << std::dec << event_id << "\">\n\
		<!--"
			<< std::hex << ip << ": W to addr: " << std::hex << addr
			<< "-->\n\
		<!--" << "  Reg : " << REG_StringShort(reg)
			<< " Value: " << regval
			<< "-->\n\
		<memory-write>\n\
		<sourceLocation>\n\
		<file>dummy</file>\n\
		<line>0</line>\n\
		<column>0</column>\n\
		</sourceLocation>\n\
		<lval>\n\
		<address>"
			<< std::hex << addr << "</address>\n";

	std::stringstream typeTag;
	if (targetType != NULL && sourceType != NULL) {

		cout << "Source and target found" << endl;
		cout << "\tsource type: " << sourceType->getType() << endl;
		cout << "\ttarget type: " << targetType->getType() << endl;

		// Type the source pointer with the target
		if (targetType->getIsCompound()) {
			typeTag << "struct ";
		}
		typeTag << targetType->getType() << " *";
		if (sourceType->getType().find("VOID*") != std::string::npos) {
			TraceXMLFile << "<type>" << typeTag.str()
					<< "</type><!-- sourceType: " << sourceType->getType()
					<< " -->\n";
		} else {
			TraceXMLFile << "<type>" << typeTag.str()
					<< "</type><!-- sourceType: " << sourceType->getType()
					<< " -->\n";
		}

	} else if (targetType != NULL && sourceType == NULL) {
		typeTag << "" << "VOID *";
		TraceXMLFile << "<type>" << typeTag.str() << "</type>\n";
	}

	TraceXMLFile
			<< "<codeFragment>*start</codeFragment>\n\
		</lval>\n\
		<content>\n\
		<content>"
			<< std::hex << regval << "</content>\n";

	std::string cutType = typeTag.str().substr(0, typeTag.str().size() - 1);
	if (cutType.at(cutType.size() - 1) == ' ') {
		cutType = cutType.substr(0, cutType.size() - 1);
	}
	TraceXMLFile << "<lvalDerefType>" << cutType << "</lvalDerefType>\n";
	/* <lvalDerefType>struct parent</lvalDerefType>\n\ */
	TraceXMLFile
			<< "<rhsCodeFragment>(struct parent *)tmp</rhsCodeFragment>\n\
		</content>\n\
		</memory-write>\n\
		</event>"
			<< endl;

}

/* TR: Taken from structtrace.cpp: Check if address belongs to main executable */
bool isMain(ADDRINT ret) {
	PIN_LockClient();
	IMG im = IMG_FindByAddress(ret);
	PIN_UnlockClient();
	int inMain = IMG_Valid(im) ? IMG_IsMainExecutable(im) : 0;
	return inMain;
}

VOID callInstruction(ADDRINT addr, ADDRINT retaddr, ADDRINT sp,
		ADDRINT targetAddr, ADDRINT bp, ADDRINT ins_size) {
	if (start_trace) {

		std::cout << "Call instruction " << std::hex << addr
				<< " with return address: " << std::hex << retaddr
				<< " push call stack. Address of stack pointer: " << std::hex
				<< sp << endl;

		std::cout << "Call instruction: value of SP(!): " << std::hex << sp
				<< " value of BP: " << std::hex << bp << endl;
		// Stack pointer needs adjustment, as there seems to be a 
		// difference between RTN_InsertCall and instrumenting 
		// an actual call instruction in terms of the SP value. 
		// See other last sp saving in startTrace().
		callStackReturnAddr->push(addr, (addr + ((USIZE) ins_size)), targetAddr,
				sp - 0x8);
		callStackReturnAddr->print();

	} else {
		std::cout << "Call instruction not enabled yet" << endl;
	}
}

VOID returnInstruction(ADDRINT ip, ADDRINT sp) {
	std::cout << "\tstack after pop: " << std::endl;
	if (start_trace) {

		ADDRINT retAddr = (*((ADDRINT*) sp));
		bool popped = callStackReturnAddr->pop(retAddr, event_id, TraceXMLFile,
				stackTypeDB);
		if (popped) {
			std::cout << "Return address " << std::hex << retAddr
					<< " was popped." << std::endl;
		} else {
			std::cout << "Return address " << std::hex << retAddr
					<< " was not popped." << std::endl;
		}
		callStackReturnAddr->print();

	} else {
		TraceFile << "Return instruction not enabled yet" << endl;
	}
}

VOID pushRbp(ADDRINT sp) {
	if (start_trace) {
		std::cout << "Inspecting push of rbp: value of SP(!): " << std::hex
				<< sp << endl;
	} else {
		std::cout << "Inspecting push of rbp: value of SP(!): not enabled yet "
				<< endl;
	}
}

VOID pushRbpAfter(ADDRINT sp) {
	if (start_trace) {
		std::cout << "Inspecting push of rbp after: value of SP(!): " << sp
				<< endl;
	} else {
		std::cout
				<< "Inspecting push of rbp after: value of SP(!): not enabled yet "
				<< endl;
	}
}

void stackPointerManipulation(ADDRINT sp, UINT32 size, ADDRINT bp) {
	if (start_trace) {
		std::cout << "Called stackPointerManipulation: SP: " << std::hex << sp
				<< " BP: " << bp << " size: " << std::dec << size << "(0x"
				<< std::hex << size << ")" << endl;

		callStackReturnAddr->print();

		std::string funcStartAddr =
				callStackReturnAddr->getLastFuncStartAddrImgOffset();

		std::cout << "funcStartAddr : " << funcStartAddr << std::endl;
		std::map<std::string, std::vector<HowardType*>*>::iterator csTypeIt =
				stackTypeDB->find(funcStartAddr);

		if (stackTypeDB->end() != csTypeIt) {
			callStackReturnAddr->addStackChunk(sp, size, (*csTypeIt).second,
					event_id, TraceXMLFile);
		} else {
			std::cout << "No entry found for funcStartAddr: " << funcStartAddr
					<< endl;
			callStackReturnAddr->addStackChunk(sp, size, NULL, event_id,
					TraceXMLFile);
		}
		callStackReturnAddr->print();

	} else {

		std::cout << "Called stackPointerManipulation: SP: not enabled yet "
				<< endl;
	}
}

unsigned long registerToAddressMapping(const char *reg_name) {
	unsigned long baseAddr = 0x10;
	int numRegisters = 64;
	const char *registers[] = { "al", "ax", "bl", "bp", "bpl", "bx", "cl", "cx",
			"di", "dil", "dl", "dx", "eax", "ebp", "ebx", "ecx", "edi", "edx",
			"esi", "esp", "r10", "r10b", "r10d", "r10w", "r11", "r11b", "r11d",
			"r11w", "r12", "r12b", "r12d", "r12w", "r13", "r13b", "r13d",
			"r13w", "r14", "r14b", "r14d", "r14w", "r15", "r15b", "r15d",
			"r15w", "r8", "r8b", "r8d", "r8w", "r9", "r9b", "r9d", "r9w", "rax",
			"rbp", "rbx", "rcx", "rdi", "rdx", "rsi", "rsp", "si", "sil", "sp",
			"spl" };

	for (int i = 0; i < numRegisters; i++) {
		if (strcmp(reg_name, registers[i]) == 0) {
			std::cout << "register found: " << reg_name << std::endl;
			return baseAddr;
		}
		// Eight bytes per register, which is not correct but a plain hack
		baseAddr += 0x8;
	}

	std::cout << "Unsupported register found: " << reg_name << std::endl;
	std::cout << "Stopping. " << std::endl;
	exit(1);
	return 0xdeadbeef;
}

/*
 * Keep track of the current content of a register to filter out relevant events
 */
typedef unsigned long registerID;
enum registerState {
	NOPOINTER, POINTER
};
std::map<registerID, registerState> regShadow;

registerState getCurrentStateForRegister(unsigned long regID) {
	std::map<registerID, registerState>::iterator regIt = regShadow.find(regID);
	if (regShadow.end() == regIt) {
		std::cout << "getCurrentStateForRegister: not found regID: " << regID
				<< " adding " << std::endl;
		regShadow[regID] = NOPOINTER;
	}
	std::cout << "getCurrentStateForRegister: regID: " << regID << " -> "
			<< regShadow[regID] << std::endl;
	return regShadow[regID];
}

VOID RecordMemReadMemToReg(VOID *disassembled, VOID *reg_name, ADDRINT ip,
		VOID *read_ea) {
	UINT64 readValue = (*static_cast<UINT64*>(read_ea));
	char *disassembledChar = (static_cast<char*>(disassembled));
	char *regNameChar = (static_cast<char*>(reg_name));

	std::cout << "RecordMemReadMemToReg: reg: " << regNameChar << " read_ea: "
			<< std::hex << read_ea << " *read_ea: " << std::hex << readValue
			<< std::endl;
	std::cout << "\t(" << std::hex << ip << "): " << disassembledChar
			<< std::endl;

	if (start_trace) {

		unsigned long regId = registerToAddressMapping(regNameChar);
		registerState curRegState = getCurrentStateForRegister(regId);

		std::vector<HeapChunk*>::iterator heapIt = heap->findHeapChunkByAddress(
				readValue);
		bool valueIsAddress = heap->getHeapChunksEndIt() != heapIt;

		// We will stop immediately, if the register currently does not store a pointer
		// and the value we are writing is not an address. => Nothing changes on the register state
		if (curRegState == NOPOINTER && !valueIsAddress) {
			TraceXMLFile
					<< "<!-- skipping register event: NOPOINTER -> NOPOINTER: "
					<< disassembledChar << " -->" << endl;
			return;
		}
		// We are saving a pointer to the a register, which held a non pointer value
		if (curRegState == NOPOINTER && valueIsAddress) {
			TraceXMLFile << "<!-- register event: NOPOINTER -> POINTER: "
					<< disassembledChar << " -->" << endl;
			regShadow[regId] = POINTER;
		}
		// We are saving a non pointer value to a register, which held a pointer
		if (curRegState == POINTER && !valueIsAddress) {
			// Always make the readValue a null pointer write to indicate the loss of a reference to DSI
			readValue = 0;
			regShadow[regId] = NOPOINTER;
			TraceXMLFile << "<!-- register event: POINTER -> NOPOINTER: "
					<< disassembledChar << " -->" << endl;
		}
		// We are saving a pointer value to register holding a pointer => Nothing changes on the register state
		if (curRegState == POINTER && valueIsAddress) {
			TraceXMLFile << "<!-- register event: POINTER -> POINTER: "
					<< disassembledChar << " -->" << endl;
		}

		// Now create the register write event
		event_id++;
		TraceXMLFile << "<event id=\"" << std::dec << event_id << "\">" << endl;
		TraceXMLFile
				<< "<memory-write>\n\
			<sourceLocation>\n\
			<file>dontcare</file>\n\
			<line>0</line>\n\
			<column>0</column>\n\
			</sourceLocation>\n\
			<lval>\n\
			<address>"
				<< std::hex << registerToAddressMapping(regNameChar)
				<< "</address>\n\
			<type>VOID *</type>\n\
			<codeFragment>writing to reg: "
				<< regNameChar << ": (" << std::hex << ip << "): "
				<< disassembledChar
				<< "</codeFragment>\n\
			</lval>\n\
			<content>\n\
			<content>"
				<< std::hex << (readValue == 0 ? "0x" : "") << readValue
				<< "</content>\n\
			<lvalDerefType>VOID</lvalDerefType>\n\
			<rhsCodeFragment>writing to reg: "
				<< regNameChar << ": (" << std::hex << ip << "): "
				<< disassembledChar
				<< "</rhsCodeFragment>\n\
			</content>\n\
			</memory-write>\n\
			</event>"
				<< endl;
	} else {
		std::cout << "\tnot enabled yet" << std::endl;
	}
}
VOID RecordMemReadMemToRegWrap(VOID *disassembled, VOID *reg_name, ADDRINT ip,
		ADDRINT read_reg_value) {
	std::cout << "RecordMemReadMemToRegWrap: reg: "
			<< (static_cast<char*>(reg_name)) << " read_reg_value: " << std::hex
			<< read_reg_value << std::endl;
	RecordMemReadMemToReg(disassembled, reg_name, ip, &read_reg_value);
}

VOID recordPush(ADDRINT ip, ADDRINT sp, ADDRINT reg_value) {
	std::cerr << "recordPush: sp: " << std::hex << sp << " reg_value: "
			<< std::hex << reg_value << std::endl;
	std::vector<HeapChunk*>::iterator heapIt = heap->findHeapChunkByAddress(
			reg_value);
	bool valueIsAddress = heap->getHeapChunksEndIt() != heapIt;
	if (valueIsAddress) {
		std::cerr << "\treg_value is an address" << std::endl;
		// Now create the register write event
		event_id++;
		TraceXMLFile << "<event id=\"" << std::dec << event_id << "\">" << endl;
		TraceXMLFile
				<< "<memory-write>\n\
			<sourceLocation>\n\
			<file>dontcare</file>\n\
			<line>0</line>\n\
			<column>0</column>\n\
			</sourceLocation>\n\
			<lval>\n\
			<address>"
				<< std::hex << sp
				<< "</address>\n\
			<type>VOID *</type>\n\
			<codeFragment>pushing to stack</codeFragment>\n\
			</lval>\n\
			<content>\n\
			<content>"
				<< std::hex << (reg_value == 0 ? "0x" : "") << reg_value
				<< "</content>\n\
			<lvalDerefType>VOID</lvalDerefType>\n\
			<rhsCodeFragment>pushing to stack</rhsCodeFragment>\n\
			</content>\n\
			</memory-write>\n\
			</event>"
				<< endl;
	} else {
		std::cerr << "\treg_value is not an address" << std::endl;
	}
}

// Is called for every instruction and instruments reads and writes
VOID Instruction(INS ins, VOID *v) {

	if (INS_IsCall(ins)) {
		std::cout << "Call number of operands: " << INS_OperandCount(ins)
				<< endl;
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) callInstruction,
				IARG_INST_PTR, IARG_RETURN_IP, IARG_REG_VALUE, REG_STACK_PTR,
				IARG_BRANCH_TARGET_ADDR, IARG_REG_VALUE, REG_GBP, IARG_ADDRINT,
				INS_Size(ins), IARG_END);
	}

	if (INS_IsRet(ins)) {
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) returnInstruction,
				IARG_INST_PTR, IARG_REG_VALUE, REG_STACK_PTR, IARG_END);

	}

	if (INS_Opcode(ins) == XED_ICLASS_PUSH) {
		std::cerr << "Found push instruction: " << INS_Disassemble(ins) << endl;
		if (INS_IsMemoryWrite(ins)) {
			std::cerr << "\tpush instruction is treated as a memory write "
					<< INS_Disassemble(ins) << endl;
		}

		if (INS_OperandIsReg(ins, 0)) {
			INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) recordPush,
					IARG_INST_PTR, IARG_REG_VALUE, REG_STACK_PTR,
					IARG_REG_VALUE, REG(INS_OperandReg(ins, 0)), IARG_END);
		}

		INT32 count = INS_OperandCount(ins);
		for (INT32 i = 0; i < count; i++) {
			std::cerr << "\tchecking operand: " << i << std::endl;
			if (INS_OperandIsReg(ins, i)) {
				std::cerr << "\tRegister operand: "
						<< REG_StringShort(INS_OperandReg(ins, i)) << std::endl;
			}

			if (INS_OperandIsImmediate(ins, i)) {
				std::cerr << "\tImmediate operand: "
						<< INS_OperandImmediate(ins, i) << std::endl;
			}

			if (INS_OperandIsImplicit(ins, i)) {
				std::cerr << "\tImplicit operand: " << std::endl;

				if (INS_OperandIsReg(ins, i)) {
					std::cerr << "\t\tRegister operand: "
							<< REG_StringShort(INS_OperandReg(ins, i))
							<< std::endl;
				}

				if (INS_OperandIsImmediate(ins, i)) {
					std::cerr << "\t\tImmediate operand: "
							<< INS_OperandImmediate(ins, i) << std::endl;
				}
			}
		}
		/*INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) pushInstruction,
		 IARG_REG_VALUE, REG_STACK_PTR,
		 (UINT32) INS_OperandImmediate(ins, 1),
		 IARG_REG_VALUE, REG_GBP, IARG_END);*/
		return;
	}

	// Check for a stack pointer manipulation
	if (INS_Opcode(ins) == XED_ICLASS_SUB
			&& INS_OperandReg(ins, 0) == REG_STACK_PTR
			&& INS_OperandIsImmediate(ins, 1)) {
		/*  &&
		 isMain(insaddr)*/
		std::cout << "Found stack pointer manipulation: "
				<< REG_StringShort(INS_OperandReg(ins, 0)) << endl;
		// Use the
		INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR) stackPointerManipulation,
				IARG_REG_VALUE, REG_STACK_PTR, IARG_ADDRINT,
				(UINT32) INS_OperandImmediate(ins, 1), IARG_REG_VALUE, REG_GBP,
				IARG_END);
		return;
	}

	// Only instrument move instructions
	if (INS_Opcode(ins) != XED_ICLASS_MOV)
		return;

	// Instruments memory accesses using a predicated call, i.e.
	// the instrumentation is called iff the instruction will actually be executed.
	if (INS_IsMemoryRead(ins)) {
		TraceFile << "Instrumenting mem read: " << INS_Disassemble(ins) << endl;
		std::cout << "Instrumenting mem read: " << INS_Disassemble(ins) << endl;
		std::string regName = REG_StringShort(INS_OperandReg(ins, 0));
		std::cout << "Reading into register: " << regName << endl;
		char * regNameChar = new char[regName.size() + 1];
		std::copy(regName.begin(), regName.end(), regNameChar);
		regNameChar[regName.size()] = '\0';
		std::string disassemble = INS_Disassemble(ins);
		char * disassembledChar = new char[disassemble.size() + 1];
		std::copy(disassemble.begin(), disassemble.end(), disassembledChar);
		disassembledChar[disassemble.size()] = '\0';

		INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(RecordMemReadMemToReg),
				IARG_PTR, disassembledChar, IARG_PTR, regNameChar,
				IARG_INST_PTR, IARG_MEMORYREAD_EA, IARG_END);
	} else if (INS_IsMemoryWrite(ins)) {
		TraceFile << "Instrumenting mem write: " << INS_Disassemble(ins)
				<< endl;
		// Detect, what we write
		if (INS_OperandIsMemory(ins, 0) && INS_OperandIsReg(ins, 1)) {
			TraceFile << "Reading from register: "
					<< REG_StringShort(INS_OperandReg(ins, 1)) << endl;
			std::string disassemble = INS_Disassemble(ins);
			char * disassembledChar = new char[disassemble.size() + 1];
			std::copy(disassemble.begin(), disassemble.end(), disassembledChar);
			disassembledChar[disassemble.size()] = '\0';
			INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(RecordMemWriteRegToMem),
					IARG_PTR, disassembledChar, IARG_CONTEXT, IARG_INST_PTR,
					IARG_MEMORYWRITE_EA, IARG_UINT32,
					REG(INS_OperandReg(ins, 1)), IARG_REG_VALUE,
					REG(INS_OperandReg(ins, 1)), IARG_END);
		} else {
			TraceFile
					<< "A write where the second argument is not the register: probably immediate"
					<< endl;
		}
	} else {
		TraceFile << "Instrumenting registers/immediate only : "
				<< INS_Disassemble(ins) << endl;
		std::cout << "Instrumenting mem read: " << INS_Disassemble(ins)
				<< " registers/immediate " << endl;
		if (INS_OperandIsReg(ins, 0) && INS_OperandIsReg(ins, 1)) {
			std::cout << "copying from register "
					<< REG_StringShort(INS_OperandReg(ins, 1))
					<< " to register "
					<< REG_StringShort(INS_OperandReg(ins, 0)) << endl;
			std::string regName = REG_StringShort(INS_OperandReg(ins, 0));
			std::cout << "Reading into register: " << regName << endl;
			char * regNameChar = new char[regName.size() + 1];
			std::copy(regName.begin(), regName.end(), regNameChar);
			regNameChar[regName.size()] = '\0';
			std::string disassemble = INS_Disassemble(ins);
			char * disassembledChar = new char[disassemble.size() + 1];
			std::copy(disassemble.begin(), disassemble.end(), disassembledChar);
			disassembledChar[disassemble.size()] = '\0';
			INS_InsertCall(ins, IPOINT_BEFORE,
					AFUNPTR(RecordMemReadMemToRegWrap), IARG_PTR,
					disassembledChar, IARG_PTR, regNameChar, IARG_INST_PTR,
					IARG_REG_VALUE, REG(INS_OperandReg(ins, 1)), IARG_END);
		} else {
			std::cout << "immediates involved: " << endl;
		}
	}
}
/* ===================================================================== */
void syscall_entry(THREADID thread_id, CONTEXT *ctx, SYSCALL_STANDARD std,
		void *v) {
	TraceFile << "sys call: " << PIN_GetSyscallNumber(ctx, std) << endl;
	TraceFile << " arguments: " << endl;
	for (int i = 0; i < 4; i++) {
		ADDRINT value = PIN_GetSyscallArgument(ctx, std, i);
		TraceFile << " value: " << value << " (" << std::hex << value << ")"
				<< endl;
	}
}

void syscall_exit(THREADID thread_id, CONTEXT *ctx, SYSCALL_STANDARD std,
		void *v) {
	ADDRINT return_value = PIN_GetSyscallReturn(ctx, std);
	TraceFile << " return_value: " << return_value << " (" << std::hex
			<< return_value << ")" << endl;
}
/* ===================================================================== */

std::stringstream ssTypes;
VOID Fini(INT32 code, VOID *v) {
	TraceFile.close();
	TraceXMLFile << "</events>" << endl;
	TraceXMLFile.close();
	writeTypeDBToXML(ssTypes.str().c_str(), typeDB, stackTypeDB);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage() {
	cerr << "This tool produces a trace of calls to malloc." << endl;
	cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
	return -1;
}
/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char *argv[]) {
	// Initialize pin & symbol manager
	PIN_InitSymbols();
	if (PIN_Init(argc, argv)) {
		return Usage();
	}

	// Optional: Specify the name for the DSI generated taint/stack/mergeinfo files on the command line
	std::string inputFilePath = KnobInputFile.Value();
	std::string callStackFile = KnobCallStackPath.Value();
	std::string outputFilePath = KnobWritePath.Value();

	std::stringstream ssCallStack;
	ssCallStack << callStackFile;
	std::stringstream ssTaint;
	ssTaint << inputFilePath << ".taint";
	std::stringstream ssStack;
	ssStack << inputFilePath << ".stack";
	std::stringstream ssMergeInfo;
	ssMergeInfo << inputFilePath << ".mergeinfo";
	ssTypes << outputFilePath << "types.xml";
	std::stringstream ssTrace;
	ssTrace << outputFilePath << "trace.xml";

	printf("call stack file: %s\n", ssCallStack.str().c_str());
	printf("taint file: %s\n", ssTaint.str().c_str());
	printf("stack file: %s\n", ssStack.str().c_str());
	printf("merge file: %s\n", ssMergeInfo.str().c_str());
	printf("types file: %s\n", ssTypes.str().c_str());
	printf("trace file: %s\n", ssTrace.str().c_str());

	std::vector<std::vector<std::string>*> *mergeInfo = parseMergeInfo(
			ssMergeInfo.str().c_str());
	mergeInfo->size();
	for (std::vector<std::vector<std::string>*>::iterator it =
			mergeInfo->begin(); it != mergeInfo->end(); ++it) {
		std::cout << "mergeInfo: found items: " << std::endl;
		for (std::vector<std::string>::iterator splittedIt = (*it)->begin();
				splittedIt != (*it)->end(); ++splittedIt) {
			std::cout << "\tsplittedIt: found item:" << *splittedIt
					<< std::endl;
		}
	}
	typeDB = createTypeDB(ssCallStack.str().c_str(), ssTaint.str().c_str(),
			mergeInfo);
	// Note, that the merge information is not passed to the stack. This is already an
	// indication, that currently only heap types are merged by Howard!
	stackTypeDB = createStackTypeDB(ssStack.str().c_str());

	for (std::map<std::string, std::vector<HowardType*>*>::iterator it =
			typeDB->begin(); it != typeDB->end(); ++it) {
		std::cout << "typeDB: key: " << it->first << std::endl;
	}

	// Write to a file since cout and cerr maybe closed by the application
	TraceFile.open(KnobOutputFile.Value().c_str());
	TraceFile << hex;
	TraceFile.setf(ios::showbase);

	TraceXMLFile.open(ssTrace.str().c_str());
	TraceXMLFile << hex;
	TraceXMLFile.setf(ios::showbase);

	TraceXMLFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?><events>"
			<< endl;

	// Register Image to be called to instrument functions.
	IMG_AddInstrumentFunction(Image, 0);
	// Register Instruction function to be called for each instruction
	INS_AddInstrumentFunction(Instruction, 0);

	PIN_AddFiniFunction(Fini, 0);

	// Never returns
	PIN_StartProgram();

	return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
