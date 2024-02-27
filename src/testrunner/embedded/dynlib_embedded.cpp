//
// Created by gnilk on 11/21/2022.
//

#include "dynlib_embedded.h"
using namespace trun;

bool DynLibEmbedded::AddTestFunc(const std::string &funcName, PTESTFUNC func) {
    testfuncs[funcName] = func;
    exports.push_back(funcName);
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
