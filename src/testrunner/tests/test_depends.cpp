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
DLL_EXPORT int test_depends_readDb(ITesting *t);
DLL_EXPORT int test_depends_writeDb(ITesting *t);
DLL_EXPORT int test_depends_connectDb(ITesting *t);
}
DLL_EXPORT int test_depends(ITesting *t) {
    t->CaseDepends("func1", "func3,func2");
    t->CaseDepends("readDb", "connectDb,writeDb");
    t->CaseDepends("writeDb", "connectDb");
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
DLL_EXPORT int test_depends_readDb(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_depends_writeDb(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_depends_connectDb(ITesting *t) {
    return kTR_Pass;
}
