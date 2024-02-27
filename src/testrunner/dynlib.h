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

namespace trun {

    class IDynLibrary {
    public:
        virtual bool Scan(const std::string &pathName) = 0;
        virtual void *Handle() = 0;
        virtual PTESTFUNC FindExportedSymbol(const std::string &funcName) = 0;
        virtual const std::vector<std::string> &Exports() const = 0;
        virtual const std::string &Name() const = 0;
    };
}
