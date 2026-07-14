//
// Created by gnilk on 25.03.26.
//

#ifndef TESTRUNNER_SYMBOLRESOLVER_H
#define TESTRUNNER_SYMBOLRESOLVER_H

#include <string>
#include <vector>

#include <lldb/API/SBTarget.h>


namespace tcov {
    class SymbolResolver {
    public:
        struct SymbolInfo {
            std::string name;     // normalized (no args)
            std::string full;     // original (optional)
            std::string file;
            uint32_t line = 0;
            // Load-address range of the owning function. ResolveForTarget is the single
            // source of truth for this range (D1): it resolves the symbol's owning
            // SBFunction and fills start/end from its start/end load addresses. Named to
            // match struct Function's startLoadAddress/endLoadAddress, which now mirror
            // these.
            lldb::addr_t startLoadAddress = 0;
            lldb::addr_t endLoadAddress = 0;
        };
    public:
        SymbolResolver() = default;
        virtual ~SymbolResolver() = default;

        static std::vector<SymbolResolver::SymbolInfo> ResolveForTarget(lldb::SBTarget &target);
    private:
    };

}


#endif //TESTRUNNER_SYMBOLRESOLVER_H