//
// Created by gnilk on 11/21/2022.
//
#include "testrunner/testinterface.h"
#include "testrunner/trunembedded.h"
#include "testrunner/config.h"

//#include "testrunner/dynlib.h"
//#include "testrunner/dynlib_embedded.h"
//#include "testrunner/testrunner.h"
//static DynLibEmbedded dynlib;

// Declare some test functions
extern "C" {
DLL_EXPORT int test_main(ITesting *t);
DLL_EXPORT int test_emb(ITesting *t);
DLL_EXPORT int test_emb_exit(ITesting *t);
DLL_EXPORT int test_emb_func1(ITesting *t);
DLL_EXPORT int test_emb_func2(ITesting *t);
}

DLL_EXPORT int test_main(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_emb(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_emb_exit(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_emb_func1(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_emb_func2(ITesting *t) {
    return kTR_Pass;
}

int main(int argc, char **argv) {
    // Add some test cases
    trun::AddTestCase("test_main", (PTESTFUNC)test_main);
    trun::AddTestCase("test_emb", (PTESTFUNC)test_emb);
    trun::AddTestCase("test_emb_exit", (PTESTFUNC)test_emb_exit);
    trun::AddTestCase("test_emb_func1", (PTESTFUNC) test_emb_func1);
    trun::AddTestCase("test_emb_func2", (PTESTFUNC)test_emb_func2);

    // Run some tests...
    trun::RunTests();

    return 0;
}