/*-------------------------------------------------------------------------
 File    : testresult.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Container for testresults, see .h file for more info
 
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

#include "testresult.h"
#include <string>

using namespace trun;

TestResult::Ref TestResult::Create(const std::string &symbolName) {
    return std::make_shared<TestResult>(symbolName);
}

TestResult::TestResult(const std::string &symbolName) {
    testResult = kTestResult_NotExecuted;
    this->symbolName = symbolName;
    this->numError = this->numAssert = 0;
}

void TestResult::SetAssertError(class AssertError &other) {
    assertError = other;
}
