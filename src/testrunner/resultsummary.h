#pragma once

#include <map>
#include <vector>
#include "testfunc.h"
#include "testresult.h"

class ResultSummary {
public:
    static ResultSummary &Instance();
    void PrintSummary(bool bPrintSuccess);

    void AddResult(TestFunc *tfunc);
    void ListReportingModules();

    const std::vector<const TestResult *> &Results() const {
        return results;
    }
    const std::vector<const TestFunc *> &TestFunctions() const {
        return testFunctions;
    }
public:
    int testsExecuted = 0;
    int testsFailed = 0;
    double durationSec = 0.0;
private:
    std::vector<const TestResult *> results;
    std::vector<const TestFunc *> testFunctions;
    ResultSummary() = default;
};




