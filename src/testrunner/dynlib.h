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

// TODO: Move to generic include (platform independent)
class IDynLibrary {
public:
    virtual bool Scan(std::string pathName) = 0;
    virtual void *Handle() = 0;
    virtual void *FindExportedSymbol(std::string funcName) = 0;
    virtual std::vector<std::string> &Exports() = 0;
    virtual std::string &Name() = 0;
};
