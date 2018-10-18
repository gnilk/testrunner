#include "testresult.h"
#include <string>

TestResult::TestResult(std::string symbolName) {
    testResult = kTestResult_NotExecuted;
    this->symbolName = symbolName;
}

