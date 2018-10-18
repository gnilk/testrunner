#pragma once

#include <string>

typedef enum {
    kTestResult_Pass = 0,
    kTestResult_TestFail = 1,
    kTestResult_ModuleFail = 2,
    kTestResult_AllFail = 3,        
    kTestResult_NotExecuted = 4,
} kTestResult;

class TestResult {
public:
    TestResult(std::string symbolName);
    // Property access, getters
    kTestResult Result() { return testResult; }
    int Errors() { return numError; }
    double ElapsedTimeSec() { return tElapsedSec; }
    std::string &SymbolName() { return symbolName; }

    // Setters
    void SetResult(kTestResult result) { this->testResult = result; }
    void SetTimeElapsedSec(double t) { tElapsedSec = t; }
    void SetNumberOfErrors(int count) {numError = count;}
private:
    kTestResult testResult;
    double tElapsedSec;
    int numError;
    std::string symbolName;
};