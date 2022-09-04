#pragma once

#include "logger.h"
#include "testinterface.h"
#include "module.h"


#include <stdint.h>
#include <vector>
#include <string>
#include <map>


//
// TODO: Split this to a spearate .h file for each module_xxx.cpp
//
class ModuleLinux : public IModule {
public:
    ModuleLinux();
    ~ModuleLinux();
    
public: // IModule
    virtual void *Handle();    
    virtual std::vector<std::string> &Exports();
    virtual void *FindExportedSymbol(std::string funcName);
    virtual std::pair<ModuleContainer *, bool> Scan(std::string pathName);
    virtual std::string &Name() { return pathName; };

private:
    bool Open();
    bool Close();
    bool IsValidTestFunc(std::string funcName);

    std::string pathName;
    void *handle;
    int idxLib;
    // Set this global variable - for now, so we can resolve relative offsets
    uint8_t *ptrModuleStart;
    std::vector<std::string> exports;
    std::map<std::string, int> symbols;

    gnilk::ILogger *pLogger;
};


