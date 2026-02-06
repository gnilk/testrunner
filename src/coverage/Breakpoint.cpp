
//
// Created by gnilk on 06.02.2026.
//
#include <filesystem>
#include <stdint.h>

#include "logger.h"
#include "Breakpoint.h"
#include "SymbolTypeChecker.h"

using namespace tcov;

// Ok, we need to classify this harder...
// Either we diffrentiate BeginCoverage, like:
// - BeginCoverage(kTypeClass, "CTestCoverage");
// or with different functions:
// - BeginCoverageForClass("CTestCoverage") / BeginCoverageForFunc("some_func");
//
SymbolTypeChecker::SymbolType BreakpointManager::CheckSymbolType(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    auto symbolType = SymbolTypeChecker::ClassifySymbol(target, symbol);

    if (symbolType == SymbolTypeChecker::SymbolType::kSymClass) {
        logger->Info("%s - is class", symbol.c_str());
    } else if (symbolType == SymbolTypeChecker::SymbolType::kSymFunc) {
        logger->Info("%s - is function", symbol.c_str());
    } else {
        logger->Error("%s - can't classify, not found", symbol.c_str());
    }
    return symbolType;
}

// Create coverage break points
void BreakpointManager::CreateCoverageBreakpoints(lldb::SBTarget &target, const std::string &symbol) {

    auto symbolType = CheckSymbolType(target, symbol);
    if (symbolType == SymbolTypeChecker::SymbolType::kSymClass) {
        CreateCoverageForClass(target, symbol);
    } else if (symbolType == SymbolTypeChecker::SymbolType::kSymFunc) {
        CreateCoverageForFunction(target, symbol);
    }
}

// Create coverage breakpoint for function
void BreakpointManager::CreateCoverageForFunction(lldb::SBTarget &target, const std::string &symbol) {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    auto symbollist = target.FindFunctions(symbol.c_str());

    // This is not needed - we have checked this a number of times already before getting here
    if (!symbollist.IsValid()) {
        logger->Debug("Unable to resolve symbol list for '%s'", symbol.c_str());
        return;
    }

    // Not sure when there is one more in the list
    logger->Debug("Resolving function '%s', num=%u\n", symbol.c_str(), symbollist.GetSize());
    for (uint32_t i=0;i<symbollist.GetSize();i++) {
        auto ctx = symbollist.GetContextAtIndex(i);
        auto compileUnit = ctx.GetCompileUnit();
        auto func = ctx.GetFunction();
        auto displayName =  ctx.GetSymbol().GetDisplayName();

        auto filename = std::filesystem::path(compileUnit.GetFileSpec().GetFilename());
        auto path = std::filesystem::path(compileUnit.GetFileSpec().GetDirectory());
        auto fullPathName = path / filename;

        // Fetch, or create, the compile unit to which this function belongs
        CompileUnit::Ref ptrCompileUnit = GetOrAddCompileUnit(fullPathName);
        // Fetch, or create, the function within the compile unit
        auto ptrFunction = ptrCompileUnit->GetOrAddFunction(displayName);

        // Resolve the address range
        auto startAddr = ctx.GetFunction().GetStartAddress();
        auto endAddr = ctx.GetFunction().GetEndAddress();

        // Convert and assign
        ptrFunction->startLoadAddress = startAddr.GetLoadAddress(target);
        ptrFunction->endLoadAddress = endAddr.GetLoadAddress(target);

        // Now create the actual breakpoints - using start/end addr
        CreateBreakpointsFunctionRange(target, compileUnit, ptrFunction);
    }
}

// Create breakpoints for a function between start/end addr..
void BreakpointManager::CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction) {
    auto numLines = compileUnit.GetNumLineEntries();
    for (uint32_t line = 0; line < numLines; line++) {
        auto lineEntry = compileUnit.GetLineEntryAtIndex(line);
        if (!lineEntry.IsValid()) {
            continue;
        }

        // 'invalid'?
        if (lineEntry.GetLine() == 0) {
            printf("lineEntry.GetLine() == 0, invalid\n");
            continue;
        }
        if (!lineEntry.GetFileSpec().IsValid()) {
            printf("Filespec for line entry invalid\n");
            continue;
        }
        // Filter out lines starting at column 0 - normally does not count...
        // FIXME: Put this on a flag - aggressive filtering (might be good for large projects)
        //        For 'normal' code the 'Proluge' check will detect this...
        // if (lineEntry.GetColumn() == 0) {
        //     printf("lineEntry.GetColumn() == 0, invalid (for line=%u)\n", lineEntry.GetLine());
        //     continue;
        // }

        auto addr = lineEntry.GetStartAddress();
        if (!addr.IsValid()) {
            continue;
        }
        if (addr.GetLoadAddress(target) == LLDB_INVALID_ADDRESS) {
            printf("INVALID address\n");
            continue;;
        }
        // FIXME: Put this on a flag - aggressive filtering
        if (addr.GetLoadAddress(target) == ptrFunction->startLoadAddress) {
            printf("Prolouge - skipping\n");
            continue;
        }

        // FIXME: Don't create breakpoints here - just gather the coverage addresses
        // seems DWARF data can contain multiple addresses pointing to same line etc...

        if (addr.GetLoadAddress(target) >= ptrFunction->startLoadAddress &&
            addr.GetLoadAddress(target) < ptrFunction->endLoadAddress) {
            // Not sure which one is best - or if one translates to the other..
            //auto bp = target.BreakpointCreateByAddress(addr.GetLoadAddress(target));
            auto bp = target.BreakpointCreateBySBAddress(addr);
            bp.SetAutoContinue(true);

            auto my_breakpoint = std::make_shared<Breakpoint>();
            my_breakpoint->breakpoint = bp;
            my_breakpoint->line = line;
            my_breakpoint->loadAddress = addr.GetLoadAddress(target);

            ptrFunction->breakpoints.push_back(my_breakpoint);
        }
    }
}


// Create coverage breakpoints for classes
// FIXME: Need to enumerate members and then call 'for function'
void BreakpointManager::CreateCoverageForClass(lldb::SBTarget &target, const std::string &symbol) {

}


void BreakpointManager::Report() {
    for (auto &[unitName, ptrCompileUnit] : compileUnits) {
        auto pathName = std::filesystem::path(ptrCompileUnit->pathName);
        if (std::filesystem::exists(pathName)) {
            // FIXME: Loadfile here
        }

        for (auto &[funcName, ptrFunction] : ptrCompileUnit->functions) {
            size_t nHits = 0;
            for (auto &bp : ptrFunction->breakpoints) {
                if (bp->breakpoint.GetHitCount() > 0) {
                    nHits++;
                }
                printf("%llX - %s:%d:%d\n",bp->loadAddress, funcName.c_str(), bp->loadAddress, bp->breakpoint.GetHitCount());

            }
            float funcCoverage = (float)nHits / (float)ptrFunction->breakpoints.size();
            int coveragePercentage = 100 * funcCoverage;
            printf("%s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n", funcName.c_str(), coveragePercentage, funcCoverage, nHits, ptrFunction->breakpoints.size());

        }
    }
}


CompileUnit::Ref BreakpointManager::GetOrAddCompileUnit(const std::string &&pathName) {
    CompileUnit::Ref ptrCompileUnit = nullptr;
    if (!compileUnits.contains(pathName)) {
        ptrCompileUnit = std::make_shared<CompileUnit>();
        ptrCompileUnit->pathName = pathName;
        compileUnits.insert(std::make_pair(pathName, ptrCompileUnit));
    } else {
        ptrCompileUnit = compileUnits[pathName];
    }
    return ptrCompileUnit;
}

