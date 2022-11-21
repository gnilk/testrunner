//
// Created by gnilk on 11/21/2022.
//

#include "dynlib_embedded.h"
using namespace trun;

bool DynLibEmbedded::AddTestFunc(std::string name, PTESTFUNC func) {
    testfuncs[name] = func;
    exports.push_back(name);
    return true;
}

void *DynLibEmbedded::Handle() {
    return nullptr;
}

PTESTFUNC DynLibEmbedded::FindExportedSymbol(std::string funcName) {
    return testfuncs[funcName];
}

bool DynLibEmbedded::Scan(std::string pathName) {
    return true;
}
