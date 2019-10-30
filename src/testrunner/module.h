#pragma once

#include "logger.h"
#include "testinterface.h"

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <stdint.h>
#include <vector>
#include <string>
#include <map>


extern "C"
{
	#ifdef WIN32
	//#define CALLCONV __stdcall
	#define CALLCONV
	#undef GetObject
	#else
	#define CALLCONV
	#endif


    typedef int (CALLCONV *PTESTFUNC)(void *param);
} 

// TODO: Move to generic include (platform independent)
class IModule {
public:
    virtual void *Handle() = 0;
    virtual void *FindExportedSymbol(std::string funcName) = 0;
    virtual std::vector<std::string> &Exports() = 0;
    virtual bool Scan(std::string pathName) = 0;
    virtual std::string &Name() = 0;
};

//
// Keep the following MacOS specfic
//
class BaseCommand;

class Module : public IModule {
public:
    Module();
    ~Module();
    
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
#ifdef WIN32
	HMODULE handle;
#else
    void *handle;
#endif
    int idxLib;
    // Set this global variable - for now, so we can resolve relative offsets
    uint8_t *ptrModuleStart;
    struct mach_header *header;
    std::vector<std::string> exports;
    std::map<std::string, int> symbols;

    gnilk::ILogger *pLogger;
};


