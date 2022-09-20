#pragma once

#include <vector>
#include "testresult.h"

class ResultSummary {
public:
    static ResultSummary &Instance();
    void PrintSummary(bool bPrintSuccess);

    void ListReportingModules();

public:
    int testsExecuted = 0;
    int testsFailed = 0;
    double durationSec = 0.0;
    std::vector<TestResult *> results;
private:
    ResultSummary() = default;
};




