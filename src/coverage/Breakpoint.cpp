
//
// Created by gnilk on 06.02.2026.
//
#include <filesystem>
#include <stdint.h>

#include "logger.h"
#include "Breakpoint.h"
#include <unordered_set>

using namespace tcov;

// Create coverage breakpoints for a fully-resolved symbol. Static discovery is done -
// SymbolResolver::ResolveForTarget is the single source of truth (name, file, line and
// load-address range). The manager only owns DYNAMIC state: it installs breakpoints and,
// later, reads their hit counts. No symbol lookup or classification happens here.
int BreakpointManager::CreateCoverageForSymbol(lldb::SBTarget &target, const SymbolResolver::SymbolInfo &info) {

    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    int numCreated = CreateCoverageForFunction(target, info);

    // Dump
    logger->Debug("Dumping details");
    for (auto &[cname, cp] : compileUnits) {
        logger->Debug("%s @ %s nFunctions=%zu\n", cname, cp->pathName, cp->functions.size());
        for (auto &[fname, func] : cp->functions) {
            logger->Debug("  %s, line=%u, start=%llX, end=%llX, bp=%zu", func->info.full.c_str(), func->startLine, func->info.startLoadAddress, func->info.endLoadAddress, func->breakpoints.size());
        }
    }
    return numCreated;
}


// Create coverage breakpoints for a single resolved function. No FindFunctions / no
// classification - the function is built entirely from the resolved SymbolInfo. The
// SBTarget is used only to derive the compile unit by address (instrumentation plumbing)
// and to install breakpoints across the symbol's [start,end) load-address range.
int BreakpointManager::CreateCoverageForFunction(lldb::SBTarget &target, const SymbolResolver::SymbolInfo &info) {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");

    // D2: resolve the owning compile unit from the symbol's start load address.
    lldb::SBAddress startAddr = target.ResolveLoadAddress(info.startLoadAddress);
    if (!startAddr.IsValid()) {
        logger->Error("Unable to resolve load address %llX for '%s'", info.startLoadAddress, info.full.c_str());
        return 0;
    }
    auto sc = target.ResolveSymbolContextForAddress(startAddr, lldb::eSymbolContextCompUnit);
    auto compileUnit = sc.GetCompileUnit();
    auto fileSpec = compileUnit.GetFileSpec();
    if (!fileSpec.IsValid()) {
        logger->Error("Invalid Filespec for compile unit - skipping '%s'", info.full.c_str());
        return 0;
    }

    std::filesystem::path filename = std::filesystem::path(fileSpec.GetFilename());
    std::filesystem::path path = std::filesystem::path(fileSpec.GetDirectory());
    std::filesystem::path fullPathName = path / filename;
    logger->Debug("Resolving function '%s' in %s", info.full.c_str(), fullPathName.c_str());

    // Fetch, or create, the compile unit to which this function belongs
    CompileUnit::Ref ptrCompileUnit = GetOrAddCompileUnit(fullPathName);
    // Fetch, or create, the function within the compile unit - keyed by the with-args
    // display name (D5) so overloads stay distinct and report output is unchanged.
    auto ptrFunction = ptrCompileUnit->GetOrAddFunction(std::string(info.full));

    // D3/D4: Function composes the resolved SymbolInfo (static data). startLine is the one
    // dynamic scalar - seed it from info.line; it may be lowered while placing breakpoints.
    ptrFunction->info = info;
    ptrFunction->startLine = info.line;

    // Now create the actual breakpoints - across the resolved [start,end) load range
    int numCreated = CreateBreakpointsFunctionRange(target, compileUnit, ptrFunction);
    logger->Debug("  Num Breakpoints = %zu", ptrFunction->breakpoints.size());

    return numCreated;
}

// Create breakpoints for a function between start/end addr..
int BreakpointManager::CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction) {
    //auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    int numCreated = 0;

    auto numLines = compileUnit.GetNumLineEntries();
    for (uint32_t line = 0; line < numLines; line++) {
        auto lineEntry = compileUnit.GetLineEntryAtIndex(line);
        if (!lineEntry.IsValid()) {
            continue;
        }


        // Added for prolouge/epiolouge code and other intermediate stuff - we can't compute coverage for it - so let's just skip it...
        if (lineEntry.GetLine() == 0) {
            //logger->Debug("lineEntry.GetLine() == 0, invalid\n");
            continue;
        }
        // Filter out lines starting at column 0 - normally does not count...
        // FIXME: Put this on a flag - aggressive filtering (might be good for large projects)
        //        For 'normal' code the 'Proluge' check will detect this...
        // if (lineEntry.GetColumn() == 0) {
        //     printf("lineEntry.GetColumn() == 0, invalid (for line=%u)\n", lineEntry.GetLine());
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
        // if (addr.GetLoadAddress(target) == ptrFunction->info.startLoadAddress) {
        //     printf("Prolouge - skipping\n");
        //     continue;
        // }


        // seems DWARF data can contain multiple addresses pointing to same line etc...
        if (addr.GetLoadAddress(target) >= ptrFunction->info.startLoadAddress &&
            addr.GetLoadAddress(target) < ptrFunction->info.endLoadAddress) {
            // Was not sure which one to use - but load-address is the final truth, so let's use it
            auto bp = target.BreakpointCreateByAddress(addr.GetLoadAddress(target));
            //auto bp = target.BreakpointCreateBySBAddress(addr);
            bp.SetAutoContinue(false);

            auto my_breakpoint = std::make_shared<Breakpoint>();
            my_breakpoint->breakpoint = bp;
            my_breakpoint->line = lineEntry.GetLine();
            // startLine can be lowered here: the [start,end) range picks up line entries
            // that map to source lines BELOW the declared start (inlined/other code leaking
            // into the range - see §6 accuracy). So startLine is dynamic, not a mirror of
            // info.line - it stays a Function field.
            if (lineEntry.GetLine() < ptrFunction->startLine) {
                ptrFunction->startLine = lineEntry.GetLine();
            }
            my_breakpoint->loadAddress = addr.GetLoadAddress(target);
            ptrFunction->breakpoints.push_back(my_breakpoint);
            numCreated++;
        }
    }

    // Sort created breakpoint based on load address
    // this is safe - because a newly created breakpoint has not been hit yet!
    std::sort(ptrFunction->breakpoints.begin(), ptrFunction->breakpoints.end(),[](auto &a, auto &b) {
        return a->loadAddress < b->loadAddress;
    });
    return numCreated;
}


std::vector<FunctionCoverage> BreakpointManager::ComputeCoverage() const {
    auto logger = gnilk::Logger::GetLogger("BreakpointManager");
    logger->Debug("Computing coverage");
    std::vector<FunctionCoverage> coverageList;

    for (auto &[unitName, ptrCompileUnit] : compileUnits) {
        auto pathName = std::filesystem::path(ptrCompileUnit->pathName);
        if (std::filesystem::exists(pathName)) {
            // FIXME: Loadfile here?
        }

        for (auto &[_, ptrFunction] : ptrCompileUnit->functions) {
            size_t nHits = 0;
            std::vector<uint32_t> covered_lines;
            std::vector<uint32_t> uncovered_lines;
            // Some lines appear multiple times, in order to compute the 'total' lines of a function
            // we need to eliminate duplicates - this is the 'lazy' version
            std::unordered_set<uint32_t> covered_line_set;
            std::unordered_set<uint32_t> unique_line_set;
            for (auto &bp : ptrFunction->breakpoints) {
                if (bp->breakpoint.GetHitCount() > 0) {
                    nHits++;
                    covered_lines.push_back(bp->line);
                    covered_line_set.insert(bp->line);
                } else {
                    uncovered_lines.push_back(bp->line);
                }
                unique_line_set.insert(bp->line);

                // if (bp->breakpoint.GetHitCount() > 1) {
                //     printf("****  %d - %s - %lX - %d\n",bp->line, ptrFunction->info.full.c_str(), bp->loadAddress, bp->breakpoint.GetHitCount());
                // }

            }

            // Store this - certain report engines want hit
            ptrFunction->nHits = nHits;

            // Sort and remove duplicates in the 'covered_lines' vector
            std::ranges::sort(covered_lines);
            const auto ures = std::ranges::unique(covered_lines);
            covered_lines.erase(std::ranges::begin(ures), std::ranges::end(ures));

            // Same for uncovered_lines
            std::ranges::sort(uncovered_lines);
            const auto uncres = std::ranges::unique(uncovered_lines);
            uncovered_lines.erase(std::ranges::begin(uncres), std::ranges::end(uncres));

            // We don't support multi-statements per line - thus some of these lines appear
            // as both in covered and uncovered - just remove them from uncovered for now...
            if (!uncovered_lines.empty()) {
                bool bFoundDuplicate = true;
                while (bFoundDuplicate) {
                    bFoundDuplicate = false;
                    for (auto it = uncovered_lines.begin(); it != uncovered_lines.end(); ++it) {
                        if (covered_line_set.contains(*it)) {
                            logger->Debug("!! %s - Duplicate line found, removing: %d",ptrFunction->info.full.c_str(), *it);
                            uncovered_lines.erase(it);
                            bFoundDuplicate = true;
                            break;
                        }
                    }
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
                (uint32_t)unique_line_set.size(),
                std::move(covered_lines),
                std::move(uncovered_lines),
            };
            coverageList.push_back(funcCoverage);
        }
    }
    return coverageList;
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

