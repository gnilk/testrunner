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

#include "module.h"
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


Module::Module() {
    this->handle = NULL;
    this->idxLib = -1;
    this->pLogger = gnilk::Logger::GetLogger("Module");
}
Module::~Module() {
    pLogger->Debug("DTOR, closing library");
    Close();
}

//
// Handle, returns a handle to the library for this module
// NULL if not opened or failed to open
//
void *Module::Handle() {
    return handle;
}

//
// FindExportedSymbol, returns a handle (function pointer) to the exported symbol
//
void *Module::FindExportedSymbol(std::string funcName) {
  
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
std::vector<std::string> &Module::Exports() {
    return exports;
}

//
// Scan, scans a dynamic library for exported test functions
//
bool Module::Scan(std::string pathName) {
    this->pathName = pathName;

    pLogger->Debug("Module::Scan, entering, pathName: %s\n", pathName.c_str());
    if (!Open()) {
        return false;
    }
    pLogger->Debug("Module::Open ok");


    // pLogger->Debug("File type: 0x%.8x", header->filetype);
    // pLogger->Debug("Flags: 0x%.8x", header->flags);
    // pLogger->Debug("Cmds: %d", header->ncmds);
    // pLogger->Debug("Size of header: %lu\n", sizeof(struct mach_header_64));
 
    //ParseCommands();

    pLogger->Debug("Module::Scan, leaving");

    return true;
}

//
// ========= Protected/Private below this point
//


//
// Open, opens the dynamic library and scan's for exported symbols
//
bool Module::Open() {

    // See: https://stackoverflow.com/questions/1128150/win32-api-to-enumerate-dll-export-functions
    
    HMODULE lib = LoadLibraryEx(pathName.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (lib == NULL) {
        return false;
    }
    
	pLogger->Debug("LoadLibrary, ok\n");

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

	
	if (header->OptionalHeader.NumberOfRvaAndSizes == 0) {
		pLogger->Error("Number of RVA and Sizes is zero!\n");
		return false;
	}
	assert(header->OptionalHeader.NumberOfRvaAndSizes > 0);

    PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

    assert(exports->AddressOfNames != 0);
    BYTE** names = (BYTE**)(pLibStart + exports->AddressOfNames);

	pLogger->Debug("Num Exports: %d\n", exports->NumberOfNames);

    for (int i = 0; i < exports->NumberOfNames; i++) {
		char* ptrName = (char*)((BYTE *)lib + (uint64_t)names[i]);
		pLogger->Debug("%d:%p\n", i, ptrName);
        std::string name(ptrName);
        if (IsValidTestFunc(name)) {
            this->exports.push_back(name);
        }
    }
    // Let's free the library and open it properly
    FreeLibrary(lib);

	handle = LoadLibrary(pathName.c_str());
    if (handle == NULL) {
        return false;
    }

    return true;    
}

bool Module::Close() {
    if (handle != NULL) {
		FreeLibrary(handle);
        return true;
    }
    return false;
}

//
// Validates a function name as a test function
//
bool Module::IsValidTestFunc(std::string funcName) {
    // The function table is what really matters
    if (funcName.find("_test_",0) == 0) {
        return true;
    }
    return false;
}

//
// Below is not used on Win32 - just for class/interface compliancy
//


//
bool Module::ParseCommands() {
    // Not used on Win32
    return true;
}


int Module::FindImage() {
    return 0;
}



void Module::ProcessSymtab(uint8_t *ptrData) {
}
void Module::ParseSymTabNames(uint8_t *ptrData) {
}
void Module::ExtractTestFunctionFromSymbols() {
}


uint8_t *Module::FromOffset32(uint32_t offset) {
    return NULL;
}

uint8_t *Module::ModuleStart() {
    return NULL;
}

uint8_t *Module::AlignPtr(uint8_t *ptr) {
    return NULL;
}

