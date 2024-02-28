//
// Created by gnilk on 28.02.24.
// Used to test circular dependency resolution
//
#include "../config.h"
#include "../testinterface.h"

extern "C" {
DLL_EXPORT int test_cdepends(ITesting *t);
DLL_EXPORT int test_cdepends_circ1(ITesting *t);
DLL_EXPORT int test_cdepends_circ2(ITesting *t);
}
DLL_EXPORT int test_cdepends(ITesting *t) {
    t->CaseDepends("circ1", "circ2");
    t->CaseDepends("circ2", "circ1");
    return kTR_Pass;
}

DLL_EXPORT int test_cdepends_circ1(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_cdepends_circ2(ITesting *t) {
    return kTR_Pass;
}
