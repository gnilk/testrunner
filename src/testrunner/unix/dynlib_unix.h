#pragma once

#include "../logger.h"
#include "../testinterface.h"
#include "../dynlib.h"


#include <stdint.h>
#include <vector>
#include <string>
#include <map>

namespace trun {
    class DynLibLinux : public IDynLibrary {
    public:

        static IDynLibrary::Ref Create();

        DynLibLinux();
        virtual ~DynLibLinux();

    public: // IDynLibrary
        bool Scan(const std::string &libPathName) override;
        void *Handle() override;
        const std::vector<std::string> &Exports() const override;
        PTESTFUNC FindExportedSymbol(const std::string &symbolName) override;
        const std::string &Name() const override { return pathName; };

    private:
        bool Open();
        bool Close();
        bool IsValidTestFunc(std::string funcName);

        std::string pathName;
        void *handle = nullptr;
        ILogger *pLogger = nullptr;

        std::vector<std::string> exports;
        std::map<std::string, int> symbols;
    };
}

