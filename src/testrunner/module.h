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
