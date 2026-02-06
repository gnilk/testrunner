//
// Created by gnilk on 06.02.2026.
//

#include "SymbolTypeChecker.h"
#include "logger.h"

using namespace tcov;

SymbolTypeChecker::SymbolType SymbolTypeChecker::ClassifySymbol(lldb::SBTarget &target, const std::string &symbol) {
    if (IsClassType(target, symbol)) {
        return SymbolType::kSymClass;
    }
    if (IsFuncType(target, symbol)) {
        return SymbolType::kSymFunc;
    }
    return SymbolType::kSymNotFound;
}
bool SymbolTypeChecker::IsFuncType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("SymbolTypeChecker");

    auto functionList = target.FindFunctions(symbol.c_str());
    // Is function?
    if (!functionList.IsValid()) {
        logger->Debug("Unable to resolve symbol list for '%s'", symbol.c_str());
        return false;
    }
    // Even if valid - it is quite often zero size if nothing did match
    if (functionList.GetSize() == 0) {
        return false;
    }

    // Having matches - normally this should only be one - but I am not sure how this works internally
    // FIXME: try to find a case where this list is > 1 and we have multiple options
    logger->Debug("Dumping functions, num=%u\n", functionList.GetSize());
    for (uint32_t i=0;i<functionList.GetSize();i++) {
        auto func = functionList.GetContextAtIndex(i);
        logger->Debug("  %u:%s - in functionlist", i, func.GetSymbol().GetDisplayName());
    }
    // FIXME: might have to do some partial matching at least...
    return true;
}
bool SymbolTypeChecker::IsClassType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("SymbolTypeChecker");
    auto symbolTypeList = target.FindTypes(symbol.c_str());

    if (!symbolTypeList.IsValid()) {
        return false;
    }
    if (symbolTypeList.GetSize() == 0) {
        return false;
    }

    auto nSymbols = symbolTypeList.GetSize();
    logger->Debug("Dumping symbol type list - size=%u", nSymbols);

    size_t nFound = 0;

    for (size_t i=0;i<symbolTypeList.GetSize();i++) {
        auto ct = symbolTypeList.GetTypeAtIndex(i);
        logger->Debug("  %zu:%s",i,ct.GetDisplayTypeName());

        if (!ct.IsValid()) {
            logger->Debug("Invalid - skipping");
            continue;
        }
        if (ct.IsFunctionType()) {
            logger->Debug("Function type - good");
            continue;;
        }

        if (ct.IsPolymorphicClass()) {
            logger->Debug("  Class found - dumping members!");
            for (size_t j=0; j<ct.GetNumberOfMemberFunctions();j++) {
                auto member = ct.GetMemberFunctionAtIndex(j);
                logger->Debug("    %zu:%s",j, member.GetName());
                nFound++;
            }
        }
    }

    return (nFound > 0);
}


