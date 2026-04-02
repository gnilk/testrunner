//
// Created by gnilk on 11/21/2022.
//
// This example illustrates how you can embed unit testing within the actual application (standalone).
// The main purpose for this is embedded applications for smaller systems..
//
// Note: The testrunner was never intended to run this way and might therefore be a bit thick - that's work in progress
//

#include "testrunner/trunembedded.h"
// FIXME: Do I need this???
#include "testrunner/config.h"

// Declare some test functions
extern "C" {
int test_main(ITesting *t);
int test_emb(ITesting *t);
int test_emb_exit(ITesting *t);
int test_emb_func1(ITesting *t);
int test_emb_func2(ITesting *t);
}

int test_main(ITesting *t) {
    return kTR_Pass;
}

int test_emb(ITesting *t) {
    return kTR_Pass;
}

int test_emb_exit(ITesting *t) {
    return kTR_Pass;
}

int test_emb_func1(ITesting *t) {
    TR_ASSERT(t, 1==2);
    printf("Should not be shown\n!");
    return kTR_Pass;
}

int test_emb_func2(ITesting *t) {
    return kTR_Pass;
}

int main(int argc, char **argv) {
    // Initialize the library - this is done for you when adding test-cases or setting the verbose level
    // but it is also possible to do it explicitly (better practice)
    trun::Initialize();

    // Increase log-level - this is an explicit thing in embedded, by default it is all switched off...
    trun::SetVerbose(2);

    // Add some test cases
    trun::AddTestCase("test_main", test_main);
    trun::AddTestCase("test_emb", test_emb);
    trun::AddTestCase("test_emb_exit", test_emb_exit);
    trun::AddTestCase("test_emb_func1", test_emb_func1);
    trun::AddTestCase("test_emb_func2", test_emb_func2);

    // Run some tests...
    trun::RunTests("-", "-");

    return 0;
}
