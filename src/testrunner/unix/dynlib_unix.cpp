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
#include "strutil.h"
#include "logger.h"
#include "config.h"

#include "process.h"
#include "dynlib_unix.h"

#include <dlfcn.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <utility>
#include <sstream>


using namespace trun;

IDynLibrary::Ref DynLibLinux::Create() {
    return std::make_shared<DynLibLinux>();
}

DynLibLinux::DynLibLinux() {
    this->pLogger = gnilk::Logger::GetLogger("Loader");
}
DynLibLinux::~DynLibLinux() {
    pLogger->Debug("DTOR, closing library");
    Close();
}

//
// Handle, returns a handle to the library for this library
// NULL if not opened or failed to open
//
void *DynLibLinux::Handle() {
    return handle;
}

//
// FindExportedSymbol, returns a handle (function pointer) to the exported symbol
//
PTESTFUNC DynLibLinux::FindExportedSymbol(const std::string &symbolName) {
  
    std::string exportName = symbolName;
    if (exportName[0] == '_') {
        exportName = &exportName[1];
    }

    void *ptrInvoke = dlsym(handle, exportName.c_str());
    if (ptrInvoke == nullptr) {
        pLogger->Debug("FindExportedSymbol, unable to find symbol '%s'", exportName.c_str());
        return nullptr;
    }
    return (PTESTFUNC)ptrInvoke;
}

//
// Exports, returns all valid test functions
//
const std::vector<std::string> &DynLibLinux::Exports() const {
    return exports;
}

//
// Scan, scans a dynamic library for exported test functions
//
bool DynLibLinux::Scan(const std::string &libPathName) {
    pathName = libPathName;

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

    // NOTE: The RTLD_DEEPBIND will override the default symbol resolve mechanism causing library externals to be
    //       in favor of hosting exe..  That is - if a symbol is defined twice (exe and lib) RTLD_DEEPBIND will
    //       prioritize the symbol belonging to the lib...
    //
    if (Config::Instance().linuxUseDeepBinding) {
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
        std::vector<std::string> parts;
		trun::split(parts, to.c_str(), ' ');
		if (parts.size() == 3) {
			if (parts[1] == std::string("T")) {
                if (IsValidTestFunc(parts[2])) {
                    pLogger->Debug("found: %s", to.c_str());
                    exports.push_back(parts[2]);
                }
			}
		}
		cnt++;
	}
    pLogger->Debug("found %d valid test cases out of %d symbols", (int)exports.size(), cnt);

    return true;

}

bool DynLibLinux::Close() {
    if (handle != nullptr) {
        dlclose(handle);
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

