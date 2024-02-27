#pragma once

#include "../logger.h"
#include "../testinterface.h"
#include "../dynlib.h"

#ifdef WIN32
#include <Windows.h>
#endif

#include <stdint.h>
#include <vector>
#include <string>
#include <map>



//
// Keep the following MacOS specfic
//
namespace trun {
    class BaseCommand;

    class DynLibWin : public IDynLibrary {
    public:
        static IDynLibrary::Ref Create();
        DynLibWin();
        virtual ~DynLibWin();

    public: // IDynLibrary
        void *Handle() override;
        PTESTFUNC FindExportedSymbol(const std::string &funcName) override;
        bool Scan(const std::string &libPathName) override;
        const std::vector<std::string> &Exports() const override;
        const std::string &Name() const override { return pathName; };

    private:
        bool Open();
        bool Close();
        bool IsValidTestFunc(std::string funcName);

        std::string pathName;
        HMODULE handle = {};
        ILogger *pLogger = nullptr;

        // Set this global variable - for now, so we can resolve relative offsets
        std::vector<std::string> exports;
        std::map<std::string, int> symbols;

    };


}