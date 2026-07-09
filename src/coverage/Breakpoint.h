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
        // Static data - the single source of truth is SymbolResolver::ResolveForTarget.
        // Function composes the resolved SymbolInfo (D3/D4); the scalars below mirror it
        // (filled once in CreateCoverageForFunction) so the debug dump and reports stay
        // unchanged. startLine may be lowered while placing breakpoints.
        SymbolResolver::SymbolInfo info = {};
        lldb::addr_t startLoadAddress = {};
        lldb::addr_t endLoadAddress = {};
        uint32_t startLine = 0;
        std::string name = {};       // == info.full (with-args) - map key / report identity
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
                ptrFunction->name = dispName;
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