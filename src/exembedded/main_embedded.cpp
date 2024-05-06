//
// Created by gnilk on 11/21/2022.
//
// This example illustrates how you can embed unit testing within the actual application (standalone).
// The main purpose for this is embedded applications for smaller systems..
//
// Note: The testrunner was never intended to run this way and might therefore be a bit thick - that's work in progress
//



#include "testrunner/trunembedded.h"
#include "testrunner/config.h"

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
    TR_ASSERT(t, 1==2);
    printf("Should not be shown\n!");
    return kTR_Pass;
}
DLL_EXPORT int test_emb_func2(ITesting *t) {
    return kTR_Pass;
}

int main(int argc, char **argv) {

    // Increase log-level - this is an explicit thing in embedded, by default it is all switched off...
    // Testrunner will read the verbose level from config (normally comes from cmd-line parsing) and update the log-level in the logger..
    trun::Config::Instance().verbose = 2;

    // Add some test cases
    trun::AddTestCase("test_main", test_main);
    trun::AddTestCase("test_emb", test_emb);
    trun::AddTestCase("test_emb_exit", test_emb_exit);
    trun::AddTestCase("test_emb_func1",  test_emb_func1);
    trun::AddTestCase("test_emb_func2", test_emb_func2);

    // Run some tests...
    trun::RunTests("-", "-");

    return 0;
}