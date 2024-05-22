/*-------------------------------------------------------------------------
 File    : module_win32.cpp
 Author  : FKling
 Version : -
 Orginal : 2019-10-24
 Descr   : Reading of a shared library and parsing of all exported functions

    This is the win32 version of the module handler

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2019.10.24, FKling, Implementation
 
 ---------------------------------------------------------------------------*/
#include <Windows.h>

#include "../dynlib.h"
#include "../strutil.h"
#include "../logger.h"

#include "dynlib_win32.h"

#include <Windows.h>
#include <assert.h>
#include <winnt.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <utility>

using namespace trun;

static void PrintWin32Error(gnilk::ILogger *pLogger, char *title) {
    char *msg;
    DWORD err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&msg, 0 , NULL);
    pLogger->Error("%s: %d, %s", title, err, msg);
}

IDynLibrary::Ref DynLibWin::Create() {
    return std::make_shared<DynLibWin>();
}

DynLibWin::DynLibWin() {
    this->pLogger = gnilk::Logger::GetLogger("DynLibWin");
}

DynLibWin::~DynLibWin() {
    pLogger->Debug("DTOR, closing library");
    Close();
}

//
// Handle, returns a handle to the library for this module
// NULL if not opened or failed to open
//
void *DynLibWin::Handle() {
    return hLibrary;
}

//
// FindExportedSymbol, returns a handle (function pointer) to the exported symbol
//
PTESTFUNC DynLibWin::FindExportedSymbol(const std::string &funcName) {
  
    std::string exportName = funcName;
    if (exportName[0] == '_') {
        exportName = &exportName[1];
    }

    void *ptrInvoke = (void *)GetProcAddress(hLibrary, exportName.c_str());

    //pLogger->Debug("Export Name: %s", exportName.c_str());

    if (ptrInvoke == NULL) {
        pLogger->Debug("FindExportedSymbol, unable to find symbol '%s'", exportName.c_str());
        return NULL;
    }
    return (PTESTFUNC)ptrInvoke;
}

//
// Exports, returns all valid test functions
//
const std::vector<std::string> &DynLibWin::Exports() const {
    return exports;
}

//
// Scan, scans a dynamic library for exported test functions
//
bool DynLibWin::Scan(const std::string &libPathName) {
    pathName = libPathName;

    char cwd[MAX_PATH+1];
    GetCurrentDirectory(MAX_PATH, cwd);

    pLogger->Debug("DynLibWin::Scan, entering CWD=%s, pathName=%s", cwd, pathName.c_str());
    if (!Open()) {
        return false;
    }
    pLogger->Debug("DynLibWin::Open ok");


    // pLogger->Debug("File type: 0x%.8x", header->filetype);
    // pLogger->Debug("Flags: 0x%.8x", header->flags);
    // pLogger->Debug("Cmds: %d", header->ncmds);
    // pLogger->Debug("Size of header: %lu\n", sizeof(struct mach_header_64));
 
    pLogger->Debug("DynLibWin::Scan, leaving");

    return true;
}

//
// ========= Protected/Private below this point
//


//
// Open, opens the dynamic library and scan's for exported symbols
//
bool DynLibWin::Open() {

    // See: https://stackoverflow.com/questions/1128150/win32-api-to-enumerate-dll-export-functions
    
    HMODULE lib = LoadLibraryEx(pathName.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (lib == NULL) {
		pLogger->Error("LoadLibraryEx failed, with code: %d", GetLastError());
        return false;
    }
    
	uint64_t pLibStart = (uint64_t)lib;
	if (((PIMAGE_DOS_HEADER)lib)->e_magic != IMAGE_DOS_SIGNATURE) {
		pLogger->Error("PIMAGE DOS HEADER magic mismatch");
		return false;
	}
    assert(((PIMAGE_DOS_HEADER)lib)->e_magic == IMAGE_DOS_SIGNATURE);

    PIMAGE_NT_HEADERS header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);

	if (header->Signature != IMAGE_NT_SIGNATURE) {
		pLogger->Error("Header signature mistmatch!");
		return false;
	}
	assert(header->Signature == IMAGE_NT_SIGNATURE);


	switch (header->OptionalHeader.Magic) {
	case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
		pLogger->Debug("32bit DLL Header found");
#ifdef _WIN64
		pLogger->Error("32 bit DLL in 64bit context not allowed!");
		return false;
#endif
		break;
	case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
		pLogger->Debug("Ok, 64bit DLL Header found");
#ifdef _WIN64
#else
		pLogger->Error("64 bit DLL in 32bit context not allowed!\n");
		return false;
#endif
		break;
	default:
		printf(" - Unknown/Unsupported DLL header type found");
		return 1;
		break;
	}

	
	if (header->OptionalHeader.NumberOfRvaAndSizes == 0) {
		pLogger->Error("Number of RVA and Sizes is zero!");
		return false;
	}
	assert(header->OptionalHeader.NumberOfRvaAndSizes > 0);

    PIMAGE_EXPORT_DIRECTORY dllexports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    if (dllexports->AddressOfNames == NULL) {
        return false;
    }

    //assert(exports->AddressOfNames != 0);
    BYTE** names = (BYTE**)(pLibStart + dllexports->AddressOfNames);

	pLogger->Debug("Num Exports: %d", dllexports->NumberOfNames);

    for (DWORD i = 0; i < dllexports->NumberOfNames; i++) {
		char* ptrName = (char *)((BYTE*)lib + ((DWORD*)names)[i]);
        std::string name(ptrName);
        if (IsValidTestFunc(name)) {
			pLogger->Debug("[OK] '%s' - valid test func", ptrName);
			exports.push_back(name);
		}
		else {
			pLogger->Debug("[NOK] '%s' invalid test func", ptrName);
		}
    }
    // Let's free the library and open it properly
    FreeLibrary(lib);


    hLibrary = LoadLibrary(pathName.c_str());
    if (hLibrary == NULL) {
        char buffer[256];
        GetCurrentDirectory(256, buffer);
        PrintWin32Error(pLogger, (char *)"Final LoadLibrary failed");
        pLogger->Debug("Tried loading: '%s' from: '%s'\n", pathName.c_str(), buffer);
        pLogger->Debug("Hint: If you are mixing build envs. MSVC and GCC/CLang this might lead to dependencies not found");
        return false;
    }

    return true;    
}

bool DynLibWin::Close() {
    // Win32 API LoadLibrary documentation states that this is NULL in case of failures!
    if (hLibrary != NULL) {
		FreeLibrary(hLibrary);
        return true;
    }
    return false;
}

//
// Validates a function name as a test function
//
bool DynLibWin::IsValidTestFunc(std::string funcName) {
    // The function table is what really matters
    if (funcName.find("test_",0) == 0) {
        return true;
    }
    return false;
}
