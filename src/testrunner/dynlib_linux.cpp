/*-------------------------------------------------------------------------
 File    : module_linux.cpp
 Author  : FKling
 Version : -
 Orginal : 2020-01-08
 Descr   : Linux version of reading exported functions from a shared library


    NOTE: This will spawn the 'nm' utility (from binutils) and parse the output!!!
    

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>
    + Refactor this to module_linux.cpp
 </pre>
 
 
 \History
 - 2020.01.08, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "dynlib.h"
#include "dynlib_linux.h"
#include "strutil.h"
#include "logger.h"
#include "process.h"
#include "config.h"

#include <dlfcn.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <utility>
#include <sstream>


using namespace gnilk;

DynLibLinux::DynLibLinux() {
    this->handle = NULL;
    this->idxLib = -1;
    this->pLogger = gnilk::Logger::GetLogger("Module");
}
DynLibLinux::~DynLibLinux() {
    pLogger->Debug("DTOR, closing library");
    Close();
}

//
// Handle, returns a handle to the library for this module
// NULL if not opened or failed to open
//
void *DynLibLinux::Handle() {
    return handle;
}

//
// FindExportedSymbol, returns a handle (function pointer) to the exported symbol
//
void *DynLibLinux::FindExportedSymbol(std::string funcName) {
  
   // TODO: Strip leading '_' from funcName...

    std::string exportName = funcName;
    if (exportName[0] == '_') {
        exportName = &exportName[1];
    }

    void *ptrInvoke = dlsym(handle, exportName.c_str());
    if (ptrInvoke == NULL) {
        pLogger->Debug("FindExportedSymbol, unable to find symbol '%s'", exportName.c_str());
        return NULL;
    }
    return ptrInvoke;
}

//
// Exports, returns all valid test functions
//
std::vector<std::string> &DynLibLinux::Exports() {
    return exports;
}

//
// Scan, scans a dynamic library for exported test functions
//
bool DynLibLinux::Scan(std::string pathName) {
    this->pathName = pathName;

    pLogger->Debug("Scan, entering");
    if (!Open()) {
        pLogger->Debug("Open failed");
        return false;
    }

    pLogger->Debug("Scan, leaving");
    return true;
}

//
// ========= Protected/Private below this point
//

class MyProcCallbacks : public ProcessCallbackBase {
public:
	virtual void OnStdOutData(std::string _data) {
		this->data.append(_data);
	}
	const std::string &Data() {
		return data;
	}
protected:
	std::string data;
};

//
// Open, opens the dynamic library and scan's for exported symbols
//
bool DynLibLinux::Open() {

    int openFlags = RTLD_LAZY;

    // NOTE: The RTLD_DEEPBIND will override the default symbol resolve mechanism causing module externals to be
    //       in favor of hosting exe..  That is - if a symbol is defined twice (exe and lib) RTLD_DEEPBIND will
    //       prioritize the symbol belonging to the lib...
    //
    if (Config::Instance()->linuxUseDeepBinding) {
#ifdef __linux
        openFlags |= RTLD_DEEPBIND;
#endif
    }

    // Handle is used later when resolving imports
    handle = dlopen(pathName.c_str(), openFlags);
    if (handle == NULL) {
        pLogger->Error("Failed to open: %s, error: %s", pathName.c_str(), dlerror());
        return false;
    }


	MyProcCallbacks cb;

    Process proc("nm");
	proc.SetCallback(&cb);
	proc.AddArgument(pathName.c_str());
	if (!proc.ExecuteAndWait()) {
        pLogger->Error("Failed to scan: %s\n", pathName.c_str());
        return false;
    }

	std::stringstream ss(cb.Data());
	std::string to;

	int cnt = 0;
	while(std::getline(ss,to,'\n')) {
        pLogger->Debug("got: %s", to.c_str());
        std::vector<std::string> parts;
		strutil::split(parts,to.c_str(), ' ');
		if (parts.size() == 3) {
			if (parts[1] == std::string("T")) {
                if (IsValidTestFunc(parts[2])) {
                    exports.push_back(parts[2]);
                }
			}
		}
		cnt++;
	}

    return true;

}

bool DynLibLinux::Close() {
    if (handle != NULL) {
        int res = dlclose(handle);
        idxLib = -1;
        return true;
    }
    return false;
}

//
// Validates a function name as a test function
//
bool DynLibLinux::IsValidTestFunc(std::string funcName) {
    // The function table is what really matters
    if (funcName.find("test_",0) == 0) {
        return true;
    }
    if (funcName.find("_test_",0) == 0) {
        return true;
    }
    return false;
}


/////////////////

