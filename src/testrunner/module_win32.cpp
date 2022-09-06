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
#ifdef WIN32
#include <Windows.h>
#endif

#include "dynlib.h"
#include "module_win32.h"
#include "strutil.h"
#include "logger.h"

#include <Windows.h>
#include <assert.h>
#include <winnt.h>

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <utility>

static void PrintWin32Error(gnilk::ILogger *pLogger, char *title) {
    char *msg;
    DWORD err = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL, err,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPSTR)&msg, 0 , NULL);
    pLogger->Error("%s: %d, %s\n", title, err, msg);
}

ModuleWin::ModuleWin() {
    this->handle = NULL;
    this->pLogger = gnilk::Logger::GetLogger("ModuleWin");
}
ModuleWin::~ModuleWin() {
    pLogger->Debug("DTOR, closing library");
    Close();
}

//
// Handle, returns a handle to the library for this module
// NULL if not opened or failed to open
//
void *ModuleWin::Handle() {
    return handle;
}

//
// FindExportedSymbol, returns a handle (function pointer) to the exported symbol
//
void *ModuleWin::FindExportedSymbol(std::string funcName) {
  
   // TODO: Strip leading '_' from funcName...

    std::string exportName = funcName;
    if (exportName[0] == '_') {
        exportName = &exportName[1];
    }

    void *ptrInvoke = (void *)GetProcAddress(handle, exportName.c_str());

    //pLogger->Debug("Export Name: %s", exportName.c_str());

    if (ptrInvoke == NULL) {
        pLogger->Debug("FindExportedSymbol, unable to find symbol '%s'", exportName.c_str());
        return NULL;
    }
    return ptrInvoke;
}

//
// Exports, returns all valid test functions
//
std::vector<std::string> &ModuleWin::Exports() {
    return exports;
}

//
// Scan, scans a dynamic library for exported test functions
//
bool ModuleWin::Scan(std::string pathName) {
    this->pathName = pathName;

    pLogger->Debug("ModuleWin::Scan, entering, pathName: %s\n", pathName.c_str());
    if (!Open()) {
        return false;
    }
    pLogger->Debug("ModuleWin::Open ok");


    // pLogger->Debug("File type: 0x%.8x", header->filetype);
    // pLogger->Debug("Flags: 0x%.8x", header->flags);
    // pLogger->Debug("Cmds: %d", header->ncmds);
    // pLogger->Debug("Size of header: %lu\n", sizeof(struct mach_header_64));
 
    pLogger->Debug("ModuleWin::Scan, leaving");

    return true;
}

//
// ========= Protected/Private below this point
//


//
// Open, opens the dynamic library and scan's for exported symbols
//
bool ModuleWin::Open() {

    // See: https://stackoverflow.com/questions/1128150/win32-api-to-enumerate-dll-export-functions
    
    HMODULE lib = LoadLibraryEx(pathName.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (lib == NULL) {
		pLogger->Error("LoadLibraryEx failed, with code: %d\n", GetLastError());
        return false;
    }
    
	uint64_t pLibStart = (uint64_t)lib;
	if (((PIMAGE_DOS_HEADER)lib)->e_magic != IMAGE_DOS_SIGNATURE) {
		pLogger->Error("PIMAGE DOS HEADER magic mismatch\n");
		return false;
	}
    assert(((PIMAGE_DOS_HEADER)lib)->e_magic == IMAGE_DOS_SIGNATURE);

    PIMAGE_NT_HEADERS header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);

	if (header->Signature != IMAGE_NT_SIGNATURE) {
		pLogger->Error("Header signature mistmatch!\n");
		return false;
	}
	assert(header->Signature == IMAGE_NT_SIGNATURE);


	switch (header->OptionalHeader.Magic) {
	case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
		pLogger->Debug("32bit DLL Header found\n");
#ifdef _WIN64
		pLogger->Error("32 bit DLL in 64bit context not allowed!\n");
		return false;
#endif
		break;
	case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
		pLogger->Debug("Ok, 64bit DLL Header found\n");
#ifdef _WIN64
#else
		pLogger->Error("64 bit DLL in 32bit context not allowed!\n");
		return false;
#endif
		break;
	default:
		printf(" - Unknown/Unsupported DLL header type found\n");
		return 1;
		break;
	}

	
	if (header->OptionalHeader.NumberOfRvaAndSizes == 0) {
		pLogger->Error("Number of RVA and Sizes is zero!\n");
		return false;
	}
	assert(header->OptionalHeader.NumberOfRvaAndSizes > 0);

    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    if (exports->AddressOfNames == NULL) {
        return false;
    }

    //assert(exports->AddressOfNames != 0);
    BYTE** names = (BYTE**)(pLibStart + exports->AddressOfNames);

	pLogger->Debug("Num Exports: %d\n", exports->NumberOfNames);

    for (int i = 0; i < exports->NumberOfNames; i++) {
		char* ptrName = (char *)((BYTE*)lib + ((DWORD*)names)[i]);
        std::string name(ptrName);
        if (IsValidTestFunc(name)) {
			pLogger->Debug("[OK] '%s' - valid test func\n", ptrName);
			this->exports.push_back(name);
		}
		else {
			pLogger->Debug("[NOK] '%s' invalid test func\n", ptrName);
		}
    }
    // Let's free the library and open it properly
    FreeLibrary(lib);


	handle = LoadLibrary(pathName.c_str());
    if (handle == NULL) {
        PrintWin32Error(pLogger, (char *)"Final LoadLibrary failed");
        return false;
    }

    return true;    
}

bool ModuleWin::Close() {
    if (handle != NULL) {
		FreeLibrary(handle);
        return true;
    }
    return false;
}

//
// Validates a function name as a test function
//
bool ModuleWin::IsValidTestFunc(std::string funcName) {
    // The function table is what really matters
    if (funcName.find("test_",0) == 0) {
        return true;
    }
    return false;
}
