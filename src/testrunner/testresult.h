#pragma once
/*-------------------------------------------------------------------------
 File    : testresult.h
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Container for test results


 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include <string>
#include "asserterror.h"

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
    int Asserts() { return numAssert; }
    double ElapsedTimeSec() { return tElapsedSec; }
    std::string &SymbolName() { return symbolName; }

    const class AssertError &AssertError() { return assertError; };

    // Setters
    void SetResult(kTestResult result) { this->testResult = result; }
    void SetTimeElapsedSec(double t) { tElapsedSec = t; }
    void SetNumberOfErrors(int count) {numError = count;}
    void SetNumberOfAsserts(int count) {numAssert = count;}
    void SetAssertError(class AssertError &other);
private:
    class AssertError assertError;
    kTestResult testResult;
    double tElapsedSec;
    int numError;
    int numAssert;
    std::string symbolName;
};