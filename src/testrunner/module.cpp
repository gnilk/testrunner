/*-------------------------------------------------------------------------
 File    : module.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Reading of a shared library and parsing of all exported functions

    In order for this project to be X-Platform this must be replaced

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "module.h"
#include "strutil.h"
#include "logger.h"

#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>

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

    //pLogger->Debug("Export Name: %s", exportName.c_str());

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
std::vector<std::string> &Module::Exports() {
    return exports;
}

//
// Scan, scans a dynamic library for exported test functions
//
bool Module::Scan(std::string pathName) {
    this->pathName = pathName;

    pLogger->Debug("Module::Scan, entering");
    if (!Open()) {
        return false;
    }
    pLogger->Debug("Module::Open ok");


    // pLogger->Debug("File type: 0x%.8x", header->filetype);
    // pLogger->Debug("Flags: 0x%.8x", header->flags);
    // pLogger->Debug("Cmds: %d", header->ncmds);
    // pLogger->Debug("Size of header: %lu\n", sizeof(struct mach_header_64));
 
    ParseCommands();

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

//    pLogger->Debug("Module::Open, calling dlopen with: %s", pathName.c_str());
    handle = dlopen(pathName.c_str(), RTLD_LAZY);
//    pLogger->Debug("Module::Open, dlopen returned %p", handle);
    if (handle == NULL) {
        pLogger->Error("Failed to open: %s, error: %s", pathName.c_str(), dlerror());
        return false;
    }

    if (FindImage() == -1) {
        Close();
        return false;
    }

    // Check magic
    header = (struct mach_header *)_dyld_get_image_header(idxLib);    
    pLogger->Debug("Magic: 0x%.8x", header->magic);
    if (header->magic != MH_MAGIC_64) {
        pLogger->Error("Not Mach-O 64bit!");
        Close();
        return false;
    }
    ptrModuleStart = (uint8_t *)header;

    return true;    
}


//
// Parse Mach-O commands, we only search for SYMTAB
//
bool Module::ParseCommands() {

    uint8_t *ptrData = (uint8_t *)header;
    ptrData += sizeof(struct mach_header_64);
    ptrData = AlignPtr(ptrData);
    bool haveSymbols = false;
    pLogger->Debug("Module::ParseCommands, ncmds: %d\n", header->ncmds);
    for(int i=0;i<header->ncmds;i++) {
        struct load_command *pcmd = (struct load_command *)ptrData;

        if (pcmd->cmd == LC_SYMTAB) {
            pLogger->Debug("Module::ParseCommands, LC_SYMTAB found\n");
            ProcessSymtab(ptrData);
            haveSymbols = true;
        }
        ptrData += pcmd->cmdsize;
        ptrData = AlignPtr(ptrData);
    }
    if (!haveSymbols) {
        pLogger->Error("Module::ParseCommands, no symbols found!!!\n");
    }

    return true;
}


bool Module::Close() {
    if (handle != NULL) {
        int res = dlclose(handle);
        idxLib = -1;
        return true;
    }
    return false;
}

int Module::FindImage() {
    int nImages = _dyld_image_count();
    for(int i=0;i<nImages;i++) {
        std::string imageName(_dyld_get_image_name(i));
        if (imageName.find(pathName.c_str()) != std::string::npos) {
            idxLib = i;
            break;
        }
    }
    if (idxLib == -1) {
        pLogger->Debug("Image not found!");
    }
    return idxLib;
}



//
// Process LC_SYMTAB command
//
void Module::ProcessSymtab(uint8_t *ptrData) {
    // Parse data
    ParseSymTabNames(ptrData);
    ExtractTestFunctionFromSymbols();

}

//
// Parse LC_SYMTAB
//
void Module::ParseSymTabNames(uint8_t *ptrData) {
    // Locate string table for all symbols (stroff - is relative 0 from file start)
    struct symtab_command *symtab = (struct symtab_command *)ptrData;

    pLogger->Debug("Module::ParseSymTabNames, symtab strsize: %d\n", symtab->strsize);

    uint8_t *ptrStringTable = FromOffset32(symtab->stroff);
    uint32_t bytes = 0;
    // Loop symbol table and look for names starting with "_test_"
    while(bytes < symtab->strsize) {
        std::string symbolName((char *)&ptrStringTable[bytes]);
        if (symbolName.length() > 1) {
            // Just dump all symbols to a large map            
            if (symbols.find(symbolName) == symbols.end()) {
                pLogger->Debug("Module::ParseSymTabNames, '%s'\n", symbolName.c_str());
                symbols.insert(std::pair<std::string, int>(symbolName, bytes));
            }
        }       
        bytes += symbolName.length();
        bytes++;
    }
}

//
// Extracts any exported symbol matching a test function
//
void Module::ExtractTestFunctionFromSymbols() {
    // Extract valid test functions
    for(auto x:symbols) {
        if (IsValidTestFunc(x.first)) {
            exports.push_back(x.first);
        }
    }
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

uint8_t *Module::FromOffset32(uint32_t offset) {
    return &ptrModuleStart[offset];
}

uint8_t *Module::ModuleStart() {
    return ptrModuleStart;
}

uint8_t *Module::AlignPtr(uint8_t *ptr) {
    while( ( (uint64_t)ptr) & (uint64_t)0x07) ptr++;
    return ptr;
}




/////////////////

