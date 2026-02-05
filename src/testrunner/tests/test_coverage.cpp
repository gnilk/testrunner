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
};

void CTestCoverage::SomeFunc(int arg) {
    if (arg < 10) {
        printf("Less then 10\n");
    } else {
        printf("More than 10\n");
    }
    return;
}

extern "C" int test_coverage(ITesting *t) {
    ITestingCoverage *icoverage = {};
    t->QueryInterface(ITestingCoverage_IFace_ID, reinterpret_cast<void **>(&icoverage));
    if (icoverage != nullptr) {
        icoverage->BeginCoverage("CTestCoverage");
    }
    printf("coverage main completed\n");
    return kTR_Pass;
}
extern "C" int test_coverage_somefunc(ITesting *t) {
    CTestCoverage cov;
    cov.SomeFunc(5);
    return kTR_Pass;
}