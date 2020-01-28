#pragma once

#include "logger.h"
#include "testinterface.h"
#include "module.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
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
class ModuleMac : public IModule {
public:
    ModuleMac();
    ~ModuleMac();
    
public: // IModule
    virtual void *Handle();    
    virtual std::vector<std::string> &Exports();
    virtual void *FindExportedSymbol(std::string funcName);
    virtual bool Scan(std::string pathName);
    virtual std::string &Name() { return pathName; };

private:
    bool Open();
    bool Close();
    int FindImage();
    bool ParseCommands();
    bool IsValidTestFunc(std::string funcName);
    void ProcessSymtab(uint8_t *ptrData);
    void ParseSymTabNames(uint8_t *ptrData);
    void ExtractTestFunctionFromSymbols();
    uint8_t *FromOffset32(uint32_t offset);
    uint8_t *ModuleStart();
    uint8_t *AlignPtr(uint8_t *ptr);
    //void ExecuteSingleTest(std::string funcName, std::string moduleName, std::string caseName);

    std::string pathName;
    void *handle;
    int idxLib;
    // Set this global variable - for now, so we can resolve relative offsets
    uint8_t *ptrModuleStart;
    struct mach_header *header;
    std::vector<std::string> exports;
    std::map<std::string, int> symbols;

    gnilk::ILogger *pLogger;
};


