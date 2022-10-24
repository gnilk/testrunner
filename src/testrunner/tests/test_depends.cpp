//
// Created by gnilk on 24.10.22.
//
#include "../config.h"
#include "../testinterface.h"

extern "C" {
DLL_EXPORT int test_depends(ITesting *t);
DLL_EXPORT int test_depends_func1(ITesting *t);
DLL_EXPORT int test_depends_func2(ITesting *t);
DLL_EXPORT int test_depends_func3(ITesting *t);
}
DLL_EXPORT int test_depends(ITesting *t) {
    t->CaseDepends("func3", "func3");
    t->CaseDepends("func2", "func1");
    t->CaseDepends("func1", "func3");
    return kTR_Pass;
}
DLL_EXPORT int test_depends_func1(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_depends_func2(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_depends_func3(ITesting *t) {
    return kTR_Pass;
}
