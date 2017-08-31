#include "typerefiner.h"

bool structsEquivalent(HowardType *typeA, HowardType *typeB) {

	std::cout << "structsEquivalent:" << std::endl;
	int sizeTypeA = typeA->getSize();
	int sizeTypeB = typeB->getSize();
	if (sizeTypeA != sizeTypeB)
		return false;

	std::vector<HowardType*>::iterator typeAMembers =
			typeA->getMembers()->begin();
	std::vector<HowardType*>::iterator typeBMembers =
			typeB->getMembers()->begin();
	for (; typeA->getMembers()->end() != typeAMembers; typeAMembers++) {
		HowardType *memberA = *typeAMembers;
		HowardType *memberB = *typeBMembers;
		std::cout << memberA->getType() << " compare " << memberB->getType()
				<< std::endl;
		if (memberA->getType().compare(memberB->getType()) != 0) {
			std::cout << "members not equal" << std::endl;
			return false;
		}
		typeBMembers++;
	}
	std::cout << "members equal" << std::endl;
	return true;
}

std::vector<std::vector<HowardType*>*>* groupStructTypes(
		std::vector<HowardType*> *types) {
	std::vector<HowardType*>::iterator typesIt = types->begin();
	std::vector<std::vector<HowardType*>*> *groups = new std::vector<
			std::vector<HowardType*>*>();

	std::cout << "groupStructTypes:" << std::endl;
	for (; types->end() != typesIt; typesIt++) {
		HowardType *curType = *typesIt;

		if (groups->size() == 0) {
			std::vector<HowardType*> *group = new std::vector<HowardType*>();
			group->push_back(curType);
			groups->push_back(group);
		} else {
			std::vector<std::vector<HowardType*>*>::iterator groupsIt =
					groups->begin();
			bool found = false;
			for (; groups->end() != groupsIt; groupsIt++) {
				std::vector<HowardType*>* group = *groupsIt;
				std::vector<HowardType*>::iterator groupIt = group->begin();

				// We only need to test the first element of each group. The types should
				// all be (structural) equivalent per group
				if (group->end() != groupIt) {
					HowardType *curGroupType = *groupIt;
					if (structsEquivalent(curType, curGroupType)) {
						std::cout << "types are equal, adding to group: "
								<< curGroupType->getType() << std::endl;
						group->push_back(curType);
						found = true;
						break;
					}
				}
			}

			// No suitable group found, create new one
			if (!found) {
				std::cout << "type not found, creating new group: "
						<< curType->getType() << std::endl;
				std::vector<HowardType*> *group =
						new std::vector<HowardType*>();
				group->push_back(curType);
				groups->push_back(group);

			}
		}
	}
	return groups;
}

std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>* calcTypeCombinationsPerGroup(
		std::vector<HowardType*> *typesPerGroup, int groupId) {
	std::cout << "calcTypeCombinationsPerGroup: " << std::endl;

	int groupSize = typesPerGroup->size();
	std::vector<std::vector<std::pair<HowardType*, HowardType*> >*> *retTypes =
			new std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>();
	if (groupSize == 1) {
		std::vector<std::pair<HowardType*, HowardType*> > *typeMapping =
				new std::vector<std::pair<HowardType*, HowardType*> >();
		std::pair<HowardType*, HowardType*> typePair = std::make_pair(
				(*typesPerGroup)[0], (*typesPerGroup)[0]);
		typeMapping->push_back(typePair);
		retTypes->push_back(typeMapping);
		return retTypes;
	}

	int upperLimit = (int) (pow(2, groupSize));

	std::stringstream groupTypeString;
	groupTypeString << "group_type_" << groupId;
	HowardType *groupType = new HowardType(groupTypeString.str(), 0, true);

	std::cout << "number of combinations: " << std::dec << upperLimit
			<< std::endl;
	for (int i = 0; i < upperLimit; i++) {
		std::string s = std::bitset<64>(i).to_string();
		std::string typeCombinations = s.substr(s.size() - groupSize,
				groupSize);
		std::cout << "processing i: " << i << " s: " << s
				<< " typeCombinations: " << typeCombinations << std::endl;
		const char *typeCombinationsChars = typeCombinations.c_str();
		int numberOfGroups = 0;
		for (unsigned int u = 0; u < typeCombinations.size(); u++) {
			char inGroup = typeCombinationsChars[u];
			if (inGroup == '1') {
				numberOfGroups++;
			}
		}
		if (numberOfGroups == 0 || numberOfGroups > 1) {
			std::cout << "\tprocessing typeCombinations: " << typeCombinations
					<< std::endl;
			std::vector<std::pair<HowardType*, HowardType*> > *typeMapping =
					new std::vector<std::pair<HowardType*, HowardType*> >();
			for (unsigned int u = 0; u < typeCombinations.size(); u++) {
				char inGroup = typeCombinationsChars[u];
				if (inGroup == '0') {
					std::cout << "\tputting " << (*typesPerGroup)[u]->getType()
							<< " -> " << (*typesPerGroup)[u]->getType()
							<< std::endl;
					std::pair<HowardType*, HowardType*> typePair =
							std::make_pair((*typesPerGroup)[u],
									(*typesPerGroup)[u]);
					typeMapping->push_back(typePair);
				} else {
					std::cout << "\tputting " << groupType->getType() << " -> "
							<< (*typesPerGroup)[u]->getType() << std::endl;
					std::pair<HowardType*, HowardType*> typePair =
							std::make_pair(groupType, (*typesPerGroup)[u]);
					typeMapping->push_back(typePair);
				}
			}
			std::cout << std::endl;
			retTypes->push_back(typeMapping);
		} else {
			std::cout << "\tskipping typeCombinations: " << typeCombinations
					<< std::endl;
		}
	}
	return retTypes;
}

std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>* typeCombinations(
		std::vector<std::vector<HowardType*>*>* groupedTypes) {
	std::vector<std::vector<HowardType*>*>::iterator groupedTypesIt =
			groupedTypes->begin();
	std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>* retCombinations =
			new std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>();
	std::cout << "typeCombinations: " << std::endl;

	int groupId = 0;
	for (; groupedTypes->end() != groupedTypesIt; groupedTypesIt++) {
		std::vector<HowardType*>* typeGroup = *groupedTypesIt;
		std::cout << "\tprocessing typeGroup: " << std::endl;
		std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>* combinations =
				calcTypeCombinationsPerGroup(typeGroup, groupId++);
		std::cout << "Created the following type combinations: " << std::endl;
		std::vector<std::vector<std::pair<HowardType*, HowardType*> >*>::iterator combiIt =
				combinations->begin();
		for (; combinations->end() != combiIt; combiIt++) {
			std::vector<std::pair<HowardType*, HowardType*> > *typeMapping =
					*combiIt;
			std::vector<std::pair<HowardType*, HowardType*> >::iterator typeMappingIt =
					typeMapping->begin();
			std::cout << "\t";
			for (; typeMapping->end() != typeMappingIt; typeMappingIt++) {
				std::cout << (*typeMappingIt).first->getType() << " -> "
						<< (*typeMappingIt).second->getType() << ", ";
			}
			std::cout << std::endl;
		}
	}

	return retCombinations;
}

void collectSubtypes(HowardType *structType,
		std::vector<HowardType*> *allCompoundTypes) {
	std::cout << "collectSubtypes: " << std::endl;
	std::vector<HowardType*>* structTypeMembers = structType->getMembers();
	std::vector<HowardType*>::iterator membersIt = structTypeMembers->begin();
	allCompoundTypes->push_back(structType);
	for (; structTypeMembers->end() != membersIt; membersIt++) {
		HowardType *memberType = (*membersIt);
		if (memberType->getIsCompound()) {
			std::cout << "\trecurse: " << memberType->getType() << std::endl;
			collectSubtypes(memberType, allCompoundTypes);
		}
	}
}

void typeRefiner(std::map<std::string, std::vector<HowardType*>*>* typeDB,
		std::map<std::string, std::vector<HowardType*>*>* stackTypeDB) {
	std::cout << "typeRefiner: " << std::endl;
	std::map<std::string, std::vector<HowardType*>*> *types = new std::map<
			std::string, std::vector<HowardType*>*>();

	types->insert(typeDB->begin(), typeDB->end());
	types->insert(stackTypeDB->begin(), stackTypeDB->end());

	std::map<std::string, std::vector<HowardType*>*>::iterator it =
			types->begin();
	std::vector<HowardType*> *allCompoundTypes = new std::vector<HowardType*>();
	for (; types->end() != it; it++) {

		std::string key = it->first;
		std::vector<HowardType*>* valTypes = it->second;
		std::cout << "key: " << key << std::endl;
		std::vector<HowardType*>::iterator itTypes = valTypes->begin();

		for (; valTypes->end() != itTypes; itTypes++) {
			HowardType *hwType = (*itTypes);
			std::cout << "\ttype: " << hwType->getType() << std::endl;

			if (hwType->getIsCompound()) {
				collectSubtypes(hwType, allCompoundTypes);
			}
		}
	}

	std::cout << "print all found compounds: " << std::endl;

	std::vector<HowardType*>::iterator cmpIt = allCompoundTypes->begin();
	for (; allCompoundTypes->end() != cmpIt; cmpIt++) {
		HowardType *hwType = (*cmpIt);
		std::cout << "\ttype: " << hwType->getType() << std::endl;
	}

	std::cout << "print all grouped compounds: " << std::endl;
	std::vector<std::vector<HowardType*>*> *groups = groupStructTypes(
			allCompoundTypes);
	std::vector<std::vector<HowardType*>*>::iterator groupsIt = groups->begin();

	int groupId = 0;
	for (; groups->end() != groupsIt; groupsIt++) {
		std::cout << "Group: " << groupId << std::endl;
		std::vector<HowardType*>* group = *groupsIt;
		std::vector<HowardType*>::iterator groupIt = group->begin();
		for (; group->end() != groupIt; groupIt++) {
			HowardType *type = *groupIt;
			std::cout << "\ttype: " << type->getType() << std::endl;
		}
		groupId++;
	}

	typeCombinations(groups);

}
