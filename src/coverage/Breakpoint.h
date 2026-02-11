//
// Created by gnilk on 06.02.2026.
//

#ifndef TESTRUNNER_BREAKPOINT_H
#define TESTRUNNER_BREAKPOINT_H

#include <lldb/SBBreakpoint.h>
#include <lldb/SBTarget.h>
#include <lldb/SBCompileUnit.h>
#include <lldb/SBSymbol.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdint.h>
#include "SymbolTypeChecker.h"

namespace tcov {
    struct Breakpoint {
        using Ref = std::shared_ptr<Breakpoint>;
        lldb::addr_t loadAddress;
        uint32_t line;
        lldb::SBBreakpoint breakpoint;
    };
    struct Function {
        using Ref = std::shared_ptr<Function>;
        lldb::addr_t startLoadAddress;
        lldb::addr_t endLoadAddress;
        lldb::SBSymbol symbol;
        uint32_t startLine;
        std::string name;       // will I have this?

        std::vector<Breakpoint::Ref> breakpoints;

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
    };

    class BreakpointManager {
    public:
        BreakpointManager() = default;
        virtual ~BreakpointManager() = default;

        void CreateCoverageBreakpoints(lldb::SBTarget &target, const std::string &symbol);
        std::vector<FunctionCoverage> ComputeCoverage() const;
    protected:
        SymbolTypeChecker::SymbolType CheckSymbolType(lldb::SBTarget &target, const std::string &symbol);
        void CreateCoverageForFunction(lldb::SBTarget &target, const std::string &symbol);
        void CreateCoverageForClass(lldb::SBTarget &target, const std::string &symbol);
        void CreateBreakpointsFunctionRange(lldb::SBTarget &target, lldb::SBCompileUnit &compileUnit, Function::Ref ptrFunction);
        std::vector<std::string> EnumerateMembers(lldb::SBTarget &target, const std::string &className);
        CompileUnit::Ref GetOrAddCompileUnit(const std::string &&pathName);


    private:
        std::unordered_map<std::string, CompileUnit::Ref> compileUnits;
        std::vector<Breakpoint::Ref> breakpoints;
    };


}

#endif //TESTRUNNER_BREAKPOINT_H