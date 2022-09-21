#pragma once

#include "logger.h"
#include "testinterface.h"
#include "dynlib.h"

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
class BaseCommand;

class DynLibWin : public IDynLibrary {
public:
    DynLibWin();
    virtual ~DynLibWin();
    
public: // IDynLibrary
    void *Handle() override;
    void *FindExportedSymbol(std::string funcName) override;
    bool Scan(std::string pathName) override;
    const std::vector<std::string> &Exports() const override;
    const std::string &Name() const override { return pathName; };

private:
    bool Open();
    bool Close();
    bool IsValidTestFunc(std::string funcName);

    std::string pathName;
	HMODULE handle;

    int idxLib;
    // Set this global variable - for now, so we can resolve relative offsets
    std::vector<std::string> exports;
    std::map<std::string, int> symbols;

    gnilk::ILogger *pLogger;
};


