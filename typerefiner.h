#ifndef TYPEREFINER_H
#define TYPEREFINER_H

#include <vector>
#include <map>
#include <iostream>
#include <string>
#include <sstream>
#include <bitset>
#include <cmath>

#include "typeparser.h"

void typeRefiner(std::map<std::string, std::vector<HowardType*>* >* typeDB, std::map<std::string, std::vector<HowardType*>* >* stackTypeDB);

#endif
