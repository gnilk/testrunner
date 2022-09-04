#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#include "logger.h"
#include "testinterface.h"
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

class ModuleContainer {
public:
    ModuleContainer(std::string pName) : name(pName) {

    }
    virtual ~ModuleContainer() = default;
    void AddSymbol(std::string sym) {
        symbols.push_back(sym);
    }
    std::vector<std::string> &Exports() {
        return symbols;
    }
    void *FindExportedSymbol(std::string funcName) {
        return nullptr;
    }
    std::string &Name() {
        return name;
    }
private:
    std::string name = "";
    std::vector<std::string> symbols = {};
};
// TODO: Move to generic include (platform independent)
class IModule {
public:
    virtual void *Handle() = 0;
    virtual void *FindExportedSymbol(std::string funcName) = 0;
    virtual std::vector<std::string> &Exports() = 0;
    virtual std::pair<ModuleContainer *, bool> Scan(std::string pathName) = 0;
    virtual std::string &Name() = 0;
};

