#pragma once

#ifdef WIN32
#include <Windows.h>
#endif

#include <memory>


#include "testinterface_internal.h"

#include <stdint.h>
#include <vector>
#include <string>
#include <map>

#include "logger.h"

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
        using Ref = std::shared_ptr<IDynLibrary>;
    public:
        virtual ~IDynLibrary() = default;
        virtual bool Scan(const std::string &pathName) = 0;
        virtual void *Handle() = 0;
        virtual PTESTFUNC FindExportedSymbol(const std::string &funcName) = 0;
        virtual const std::vector<std::string> &Exports() const = 0;
        virtual const std::string &Name() const = 0;

        uint32_t GetVersion() { return version; }
    protected:
        uint32_t version = TRUN_MAGICAL_IF_VERSION1;
    };
}
