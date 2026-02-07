//
// Created by gnilk on 05.02.2026.
//
#include <stdio.h>
#include "ext_testinterface/testinterface.h"

class CTestCoverage {
public:
    CTestCoverage() = default;
    virtual ~CTestCoverage() = default;

    void SomeFunc(int arg);
    void SomeInlineFunc(int arg) {
        printf("this is an inline func with stuff\n");
    }
protected:
    void ProtectedFunc(int arg) {
        printf("is inline\n");
    }
private:
    void PrivateFunc(int arg);
};

void CTestCoverage::SomeFunc(int arg) {
    if (arg < 10) {
        printf("Less then 10\n");
    } else {
        printf("More than 10\n");
    }
    return;
}
void CTestCoverage::PrivateFunc(int arg) {
    printf("not inline\n");
}

static void internal_tcov_func(int arg) {
    printf("bla bla\n");
}
void glb_tcov_func(int arg) {
    printf("bla\n");
}

namespace gurka {
    void ns_tcov_func(int arg) {
        printf("bla\n");
    }
}

extern "C" int test_coverage(ITesting *t) {
    ITestingCoverage *icoverage = {};
    t->QueryInterface(ITestingCoverage_IFace_ID, reinterpret_cast<void **>(&icoverage));
    if (icoverage != nullptr) {
        // This will stress test the various ways we resolve coverage breakpoints
        icoverage->BeginCoverage("CTestCoverage");    // resolves to class (symbollist)
        // icoverage->BeginCoverage("CTestCoverage::SomeFunc");  // resolves to a function (functionlist)
        //
        // icoverage->BeginCoverage("glb_tcov_func");     // global function, resolves to a function in tcov
        // icoverage->BeginCoverage("gurka::ns_tcov_func");    // function in a namespace, resolves to a function
        // icoverage->BeginCoverage("internal_tcov_func");   // internal function, not found at all...
    }
    printf("coverage main completed\n");
    return kTR_Pass;
}
extern "C" int test_coverage_somefunc(ITesting *t) {
    CTestCoverage cov;
    cov.SomeFunc(5);
    return kTR_Pass;
}