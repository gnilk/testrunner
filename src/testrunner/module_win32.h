#pragma once

#include "logger.h"
#include "testinterface.h"
#include "module.h"

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

//
// TODO: Split this to a spearate .h file for each module_xxx.cpp
//
class ModuleWin : public IModule {
public:
    ModuleWin();
    virtual ~ModuleWin();
    
public: // IModule
    virtual void *Handle();    
    virtual std::vector<std::string> &Exports();
    virtual void *FindExportedSymbol(std::string funcName);
    virtual bool Scan(std::string pathName);
    virtual std::string &Name() { return pathName; };

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


