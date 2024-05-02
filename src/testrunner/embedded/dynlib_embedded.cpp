/*-------------------------------------------------------------------------
 File    : dynlib_embedded.cpp
 Author  : FKling
 Version : -
 Orginal : 2022-11-21
 Descr   : Dynamic library functionality wrapper for embedded platforms

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2022.11.21, FKling, Implementation
 ---------------------------------------------------------------------------*/

#include "dynlib_embedded.h"
#include <memory>
using namespace trun;

DynLibEmbedded::Ref DynLibEmbedded::Create() {
    return std::make_shared<DynLibEmbedded>();
}

bool DynLibEmbedded::AddTestFunc(std::string name, PTESTFUNC func) {
    testfuncs[name] = func;
    exports.push_back(name);
    return true;
}

void *DynLibEmbedded::Handle() {
    return nullptr;
}

PTESTFUNC DynLibEmbedded::FindExportedSymbol(const std::string &symbolName) {
    return testfuncs[symbolName];
}

bool DynLibEmbedded::Scan(const std::string &pathName) {
    return true;
}
