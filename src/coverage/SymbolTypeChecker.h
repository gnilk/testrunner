//
// Created by gnilk on 06.02.2026.
//

#ifndef TESTRUNNER_SYMBOLTYPECHECKER_H
#define TESTRUNNER_SYMBOLTYPECHECKER_H

#include <stdint.h>
#include <string>

#include <lldb/SBTarget.h>

namespace tcov {
    class SymbolTypeChecker {
    public:
        enum class SymbolType {
            kSymNotFound,
            kSymClass,
            kSymFunc,
        };
    public:
        SymbolTypeChecker() = default;
        virtual ~SymbolTypeChecker() = default;

        static SymbolType ClassifySymbol(lldb::SBTarget &target, const std::string &symbol);
    private:
        static bool IsClassType(lldb::SBTarget &target, const std::string &symbol);
        static bool IsFuncType(lldb::SBTarget &target, const std::string &symbol);
    };
}

#endif //TESTRUNNER_SYMBOLTYPECHECKER_H