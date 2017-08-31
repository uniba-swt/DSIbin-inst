#include "shadowstack.h"

ShadowStack::ShadowStack(ADDRINT fun_addr, ADDRINT sp_val) {
	this->stack_chunk = NULL;
	this->last_stack_pointer_val = sp_val;
	this->cur_stack_pointer_val = 0;
	this->function_addr = fun_addr;
}

ShadowStack::~ShadowStack() {
	delete this->stack_chunk;
}

ADDRINT ShadowStack::getCurStackPointerVal() {
	return this->cur_stack_pointer_val;
}

ADDRINT ShadowStack::getLastStackPointerVal() {
	return this->last_stack_pointer_val;
}

ADDRINT ShadowStack::getFunAddr() {
	return this->function_addr;
}
std::string ShadowStack::getFunAddrAsStr() {
	std::stringstream ss;
	ss << std::hex << this->getFunAddr();
	return ss.str();
}

StackChunk* ShadowStack::getStackChunk() {
	return this->stack_chunk;
}

void writeVariableEnterScope(unsigned long spAddr,
		std::vector<HowardType *>*types, int &event_id,
		std::ofstream &TraceXMLFile) {
	if (types == NULL)
		return;
	std::vector<HowardType *>::iterator typesIt = types->begin();
	for (; types->end() != typesIt; typesIt++) {
		event_id++;
		TraceXMLFile << "<event id=\"" << std::dec << event_id << "\">"
				<< std::endl;
		TraceXMLFile << "<variable-enter-scope>" << std::endl;
		TraceXMLFile << "<kind>" << "dontcare</kind>" << std::endl;
		TraceXMLFile << "<name>" << "dontcare</name>" << std::endl;
		TraceXMLFile << "<type>";

		std::string typeName = (*typesIt)->getType();
		if (typeName.find("INT64") != std::string::npos) {
			typeName = "VOID*";
		}
		std::size_t pos = typeName.find("*");
		if (pos != std::string::npos)
			typeName.insert(pos, " ");

		if ((*typesIt)->getIsCompound()) {
			TraceXMLFile << "struct ";
		}
		TraceXMLFile << typeName << "</type>" << std::endl;
		TraceXMLFile << "<address>" << std::hex
				<< (spAddr - (*typesIt)->getOffset()) << "</address>"
				<< std::endl;
		TraceXMLFile << "</variable-enter-scope>" << std::endl;
		TraceXMLFile << "</event>" << std::endl;
	}
}
void ShadowStack::setStackChunk(ADDRINT cur_sp_val, ADDRINT size,
		std::vector<HowardType *>*types, int &event_id,
		std::ofstream &TraceXMLFile) {
	this->cur_stack_pointer_val = cur_sp_val;
	unsigned long stackSizeDiff = this->last_stack_pointer_val
			- this->cur_stack_pointer_val;
	std::cout << "StackChunk::setStackChunk: (lastSPAddr - sp): "
			<< stackSizeDiff << std::endl;

	if (stackSizeDiff > 0x1000) {
		std::cout
				<< "StackChunk::setStackChunk: skipping stack frame resize as the value seems unplausible: "
				<< std::hex << stackSizeDiff << std::endl;
		return;
	}

	if (this->stack_chunk != NULL) {
		unsigned long currentChunkSize = this->stack_chunk->getSize();
		if (stackSizeDiff > currentChunkSize) {
			std::cout
					<< "StackChunk::setStackChunk: resize needed: stackSizeDiff : "
					<< std::hex << stackSizeDiff << " currentChunkSize: "
					<< std::hex << currentChunkSize << std::endl;
			this->stack_chunk->setSize(stackSizeDiff);
		} else {
			std::cout
					<< "StackChunk::setStackChunk: no resize needed: stackSizeDiff : "
					<< std::hex << stackSizeDiff << " currentChunkSize: "
					<< std::hex << currentChunkSize << std::endl;
		}
	} else {
		std::cout << "StackChunk::setStackChunk: adding new StackChunk()"
				<< std::endl;
		this->stack_chunk = new StackChunk(
				(unsigned long) this->last_stack_pointer_val,
				(unsigned long) stackSizeDiff + size, this->getFunAddrAsStr(),
				types);
		writeVariableEnterScope(this->last_stack_pointer_val, types, event_id,
				TraceXMLFile);

	}

}

bool ShadowStack::hasStackFrame() {
	return this->stack_chunk != NULL;
}
