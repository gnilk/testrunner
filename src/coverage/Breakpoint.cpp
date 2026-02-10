
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

    // Dump
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    logger->Debug("Dumping details");
    for (auto &[name, cp] : compileUnits) {
        logger->Debug("%s @ %s nFunctions=%zu\n", name, cp->pathName, cp->functions.size());
        for (auto &[name, func] : cp->functions) {
            logger->Debug("  %s, line=%u, start=%llX, end=%llX, bp=%zu", func->name.c_str(), func->startLine, func->startLoadAddress, func->endLoadAddress, func->breakpoints.size());
        }
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

    // FIXME: IF number of symbols is 0 we should treat this as an inlined function (or declared in the body)
    //        switch to line-table based breakpoint creation instead of address based..

    // Not sure when there is one more in the list
    logger->Debug("Resolving function '%s', num symbols=%u", symbol.c_str(), symbollist.GetSize());
    for (uint32_t i=0;i<symbollist.GetSize();i++) {
        auto ctx = symbollist.GetContextAtIndex(i);
        auto compileUnit = ctx.GetCompileUnit();
        auto func = ctx.GetFunction();
        auto displayName =  ctx.GetSymbol().GetDisplayName();

        auto fileSpec = compileUnit.GetFileSpec();
        std::filesystem::path filename = {};
        std::filesystem::path path = {};
        std::filesystem::path fullPathName = {};
        if (fileSpec.IsValid()) {
            filename = std::filesystem::path(compileUnit.GetFileSpec().GetFilename());
            path = std::filesystem::path(compileUnit.GetFileSpec().GetDirectory());
            fullPathName = path / filename;
        }

        // Fetch, or create, the compile unit to which this function belongs
        CompileUnit::Ref ptrCompileUnit = GetOrAddCompileUnit(fullPathName);
        // Fetch, or create, the function within the compile unit
        auto ptrFunction = ptrCompileUnit->GetOrAddFunction(displayName);

        // Resolve the address range
        auto startAddr = ctx.GetFunction().GetStartAddress();
        auto endAddr = ctx.GetFunction().GetEndAddress();

        // Convert and assign
        ptrFunction->startLine = startAddr.GetLineEntry().GetLine();
        ptrFunction->startLoadAddress = startAddr.GetLoadAddress(target);
        ptrFunction->endLoadAddress = endAddr.GetLoadAddress(target);
        ptrFunction->symbol = ctx.GetSymbol();

        // Now create the actual breakpoints - using start/end addr
        CreateBreakpointsFunctionRange(target, compileUnit, ptrFunction);
        logger->Debug("  Num Breakpoints = %zu", ptrFunction->breakpoints.size());
    }
}

// Create breakpoints for a function between start/end addr..
void BreakpointManager::CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction) {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");

    auto numLines = compileUnit.GetNumLineEntries();
    for (uint32_t line = 0; line < numLines; line++) {
        auto lineEntry = compileUnit.GetLineEntryAtIndex(line);
        if (!lineEntry.IsValid()) {
            continue;
        }

        // 'invalid'?
        // if (lineEntry.GetLine() == 0) {
        //     logger->Debug("lineEntry.GetLine() == 0, invalid\n");
        //     continue;
        // }
        // if (!lineEntry.GetFileSpec().IsValid()) {
        //     logger->Debug("Filespec for line entry invalid\n");
        //     continue;
        // }
        // // FIXME: Verify and put this behind a flag!
        // // Filter inlined stuff from other places
        // if (lineEntry.GetFileSpec() != compileUnit.GetFileSpec()) {
        //     // this is an inlined function from something else - skip it, not part of our coverage
        //     continue;
        // }
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
        // Might be a bit too simplistic
        // if (addr.GetLoadAddress(target) == ptrFunction->startLoadAddress) {
        //     printf("Prolouge - skipping\n");
        //     continue;
        // }


        // FIXME: Don't create breakpoints here - just gather the coverage addresses
        // seems DWARF data can contain multiple addresses pointing to same line etc...

        if (addr.GetLoadAddress(target) >= ptrFunction->startLoadAddress &&
            addr.GetLoadAddress(target) < ptrFunction->endLoadAddress) {
            // Was not sure which one to use - but load-address is the final truth, so let's use it
            auto bp = target.BreakpointCreateByAddress(addr.GetLoadAddress(target));
            //auto bp = target.BreakpointCreateBySBAddress(addr);
            bp.SetAutoContinue(false);

            auto my_breakpoint = std::make_shared<Breakpoint>();
            my_breakpoint->breakpoint = bp;
            my_breakpoint->line = lineEntry.GetLine();
            // Should not happen - but C/C++ works in mysterious ways...
            if (lineEntry.GetLine() < ptrFunction->startLine) {
                ptrFunction->startLine = lineEntry.GetLine();
            }
            my_breakpoint->loadAddress = addr.GetLoadAddress(target);
            ptrFunction->breakpoints.push_back(my_breakpoint);
        }
    }

    // Sort created breakpoint based on load address
        // this is safe - because a newly created breakpoint has not been hit yet!
    std::sort(ptrFunction->breakpoints.begin(), ptrFunction->breakpoints.end(),[](auto &a, auto &b) {
        return a->loadAddress < b->loadAddress;
    });
}


// Create coverage breakpoints for classes
void BreakpointManager::CreateCoverageForClass(lldb::SBTarget &target, const std::string &symbol) {
    auto members = EnumerateMembers(target,symbol);
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    logger->Debug("Creating coverage for members");

    // Loop over all members and create coverage for each of them..
    // This will create for everything, CTOR/DTOR/Public/Protected/Private members
    // optional we should perhaps allow filtering for public/private/protected...
    // See comment in enumerate...
    for (auto &member : members) {
        CreateCoverageForFunction(target, member);
    }
}

std::vector<std::string> BreakpointManager::EnumerateMembers(lldb::SBTarget &target, const std::string &className) {

    std::vector<std::string> classMembers;

    auto classtype = target.FindTypes(className.c_str());

    //auto classctx = target.FindSymbols("Dummy");
    if (classtype.IsValid()) {
        printf("class type ok - size=%u\n", classtype.GetSize());
        for (size_t i=0;i<classtype.GetSize();i++) {
            auto ct = classtype.GetTypeAtIndex(i);
            printf("%zu:%s\n",i,ct.GetDisplayTypeName());

            if (!ct.IsValid()) {
                printf("Invalid - skipping\n");
                continue;
            }

            // This is wrong
            printf("Class found - good!\n");
            for (size_t j=0; j<ct.GetNumberOfMemberFunctions();j++) {
                auto member = ct.GetMemberFunctionAtIndex(j);
                // FIXME: we can use 'getkind' to figure out if this is a CTOR/DTOR/etc..
                // member.GetKind();
                printf("  %zu:%s (%s, %s)\n",j, member.GetName(), member.GetMangledName(), member.GetDemangledName());
                classMembers.push_back(member.GetDemangledName());
            }
        }
    }
    return classMembers;
}
std::vector<FunctionCoverage> BreakpointManager::ComputeCoverage() {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    logger->Debug("Computing coverage");
    std::vector<FunctionCoverage> coverageList;

    for (auto &[unitName, ptrCompileUnit] : compileUnits) {
        auto pathName = std::filesystem::path(ptrCompileUnit->pathName);
        if (std::filesystem::exists(pathName)) {
            // FIXME: Loadfile here
        }

        for (auto &[_, ptrFunction] : ptrCompileUnit->functions) {
            size_t nHits = 0;
            for (auto &bp : ptrFunction->breakpoints) {
                if (bp->breakpoint.GetHitCount() > 0) {
                    nHits++;
                }
                if (bp->breakpoint.GetHitCount() > 1) {
                    printf("****  %d - %s - %llX - %d\n",bp->line, ptrFunction->name.c_str(), bp->loadAddress, bp->breakpoint.GetHitCount());
                }

            }
            float coverage = (float)nHits / (float)ptrFunction->breakpoints.size();
            uint32_t coveragePercentage = 100 * coverage;
            FunctionCoverage funcCoverage {
                coverage,
                coveragePercentage,
                nHits,
                ptrFunction->breakpoints.size(),
                ptrFunction,
                ptrCompileUnit,
            };
            coverageList.push_back(funcCoverage);
        }
    }
    return coverageList;
}


void BreakpointManager::Report() {

    // NOT USED!!!

    // FIXME: Sort results here!
    for (auto &[unitName, ptrCompileUnit] : compileUnits) {
        auto pathName = std::filesystem::path(ptrCompileUnit->pathName);
        if (std::filesystem::exists(pathName)) {
            // FIXME: Loadfile here
        }
        std::vector<Function::Ref> functions;
        for (auto &[_, ptrFunction] : ptrCompileUnit->functions) {
            functions.push_back(ptrFunction);
        }


        printf("%s\n", ptrCompileUnit->pathName.c_str());
        for (auto &ptrFunction: functions) {
            size_t nHits = 0;
            for (auto &bp : ptrFunction->breakpoints) {
                if (bp->breakpoint.GetHitCount() > 0) {
                    nHits++;
                }
                if (bp->breakpoint.GetHitCount() > 1) {
                    printf("****  %d - %s - %llX - %d\n",bp->line, ptrFunction->name.c_str(), bp->loadAddress, bp->breakpoint.GetHitCount());
                }

            }
            float funcCoverage = (float)nHits / (float)ptrFunction->breakpoints.size();
            int coveragePercentage = 100 * funcCoverage;
            printf("%s - Coverage: %d%% (%.3f) (hits: %zu, bp:%zu) \n", ptrFunction->name.c_str(), coveragePercentage, funcCoverage, nHits, ptrFunction->breakpoints.size());

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

