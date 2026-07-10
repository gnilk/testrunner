//
// Created by gnilk on 06.02.2026.
//

#ifndef TESTRUNNER_BREAKPOINT_H
#define TESTRUNNER_BREAKPOINT_H

// #include <lldb/SBBreakpoint.h>
// #include <lldb/SBTarget.h>
// #include <lldb/SBCompileUnit.h>
// #include <lldb/SBSymbol.h>
#include <lldb/API/SBBreakpoint.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBCompileUnit.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <stdint.h>
#include "SymbolResolver.h"

namespace tcov {
    struct Breakpoint {
        using Ref = std::shared_ptr<Breakpoint>;
        lldb::addr_t loadAddress;
        uint32_t line;
        lldb::SBBreakpoint breakpoint;
    };
    struct Function {
        using Ref = std::shared_ptr<Function>;
        // STATIC data - the single source of truth is SymbolResolver::ResolveForTarget.
        // Function composes the resolved SymbolInfo (D3/D4): name, file, line and the
        // [startLoadAddress,endLoadAddress) range all live in `info`, no mirrors.
        SymbolResolver::SymbolInfo info = {};
        // DYNAMIC state, owned by the breakpoint layer. The function's start line is NOT here -
        // it is info.line, the resolver's authoritative value. (Pre-§6 a `startLine` field was
        // LOWERED while placing breakpoints, but that "lowering" only ever fired on leaked
        // line entries - cross-file inlined code, now filtered in CreateBreakpointsFunctionRange,
        // and neighbouring-function lines that mapped a `}` into the range - so it produced wrong
        // FN: starts, never a legitimately-lower one. Removed: reports read info.line.)
        size_t nHits = 0;
        std::vector<Breakpoint::Ref> breakpoints = {};

        // Normalized (no-args) display name - reports (LCOV/console) use this.
        std::string GetDisplayName() const {
            return info.name;
        }
    };
    struct CompileUnit {
        using Ref = std::shared_ptr<CompileUnit>;
        std::string pathName;
        std::unordered_map<std::string, Function::Ref> functions;

        Function::Ref GetOrAddFunction(const std::string &&dispName) {
            Function::Ref ptrFunction = nullptr;
            if (!functions.contains(dispName)) {
                ptrFunction = std::make_shared<Function>();
                ptrFunction->info.full = dispName;   // identity == the with-args map key (D5)
                functions[dispName] = ptrFunction;
            } else {
                ptrFunction = functions[dispName];
            }
            return ptrFunction;
        }
    };

    struct FunctionCoverage {
        using Ref = std::shared_ptr<FunctionCoverage>;
        float functionCoverage;
        uint32_t percentageCoverage;
        size_t nHits;
        size_t nBreakpoints;
        Function::Ref ptrFunction;
        CompileUnit::Ref ptrCompileUnit;
        uint32_t totalLines;
        std::vector<uint32_t> coveredLines;
        std::vector<uint32_t> uncoveredLines;
    };

    class BreakpointManager {
    public:
        BreakpointManager() = default;
        virtual ~BreakpointManager() = default;

        int CreateCoverageForSymbol(lldb::SBTarget &target, const SymbolResolver::SymbolInfo &info);
        std::vector<FunctionCoverage> ComputeCoverage() const;
    protected:
        int CreateCoverageForFunction(lldb::SBTarget &target, const SymbolResolver::SymbolInfo &info);
        int CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction);
        CompileUnit::Ref GetOrAddCompileUnit(const std::string &&pathName);


    private:
        std::unordered_map<std::string, CompileUnit::Ref> compileUnits;
        std::vector<Breakpoint::Ref> breakpoints;
    };


}

#endif //TESTRUNNER_BREAKPOINT_H