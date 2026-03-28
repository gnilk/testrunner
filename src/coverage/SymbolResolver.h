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
            lldb::addr_t addr = 0;
        };
    public:
        SymbolResolver() = default;
        virtual ~SymbolResolver() = default;

        static std::vector<SymbolResolver::SymbolInfo> ResolveForTarget(lldb::SBTarget &target);
    private:
    };

}


#endif //TESTRUNNER_SYMBOLRESOLVER_H