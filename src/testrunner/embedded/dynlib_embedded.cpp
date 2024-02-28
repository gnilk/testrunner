//
// Created by gnilk on 11/21/2022.
//

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
