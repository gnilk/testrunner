//
// Created by gnilk on 11/21/2022.
//

#ifndef TESTRUNNER_DYNLIB_EMBEDDED_H
#define TESTRUNNER_DYNLIB_EMBEDDED_H


#include "logger.h"
#include "testinterface.h"
#include "dynlib.h"

#include <string>
#include <map>
#include <vector>

namespace trun {
    class DynLibEmbedded : public IDynLibrary {
    public:
        DynLibEmbedded() = default;
        virtual ~DynLibEmbedded() = default;
    public:
        bool AddTestFunc(std::string name, PTESTFUNC func);
    public: // IDynLibrary
        void *Handle() override;
        PTESTFUNC FindExportedSymbol(std::string funcName) override;
        bool Scan(std::string pathName) override;
        const std::vector<std::string> &Exports() const override { return exports; }
        const std::string &Name() const override { return name; };
    private:
        std::vector<std::string> exports;
        std::map<std::string, PTESTFUNC> testfuncs;
        std::string name = "embedded";

    };
}

#endif //TESTRUNNER_DYNLIB_EMBEDDED_H
