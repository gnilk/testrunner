#pragma once

#include <vector>
#include "testresult.h"

class ResultSummary {
public:
    static ResultSummary &Instance();
    void PrintSummary(bool bPrintSuccess);

    void PrintFailureDetails();
    void PrintPassDetails();
public:
    int testsExecuted = 0;
    int testsFailed = 0;
    std::vector<TestResult *> results;
private:
    ResultSummary() = default;
};


