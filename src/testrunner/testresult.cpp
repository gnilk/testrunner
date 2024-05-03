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
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "testinterface_internal.h"
#include "testresult.h"
#include <string>
#include "config.h"

using namespace trun;

TestResult::Ref TestResult::Create(const std::string &symbolName) {
    return std::make_shared<TestResult>(symbolName);
}

TestResult::TestResult(const std::string &use_symbolName) {
    testResult = kTestResult_NotExecuted;
    symbolName = use_symbolName;
    numError = numAssert = 0;
}

void TestResult::SetAssertError(class AssertError &other) {
    assertError = other;
}
void TestResult::SetTestResultFromReturnCode(int testReturnCode) {
    // Discard return code???
    auto pLogger = gnilk::Logger::GetLogger("TestResult");

    if (Config::Instance().discardTestReturnCode) {
        pLogger->Debug("Discarding return code\n");
        return;
    }
    // Let's overwrite test result from the test
    switch(testReturnCode) {
        case kTR_Pass :
            if ((Errors() == 0) && (Asserts() == 0)) {
                SetResult(kTestResult_Pass);
            } else {
                // This could be depending on 'strict' checking flag (fail in strict mode)
                SetResult(kTestResult_TestFail);
            }
            break;
        case kTR_Fail :
            SetResult(kTestResult_TestFail);
            break;
        case kTR_FailModule :
            SetResult(kTestResult_ModuleFail);
            break;
        case kTR_FailAll :
            SetResult(kTestResult_AllFail);
            break;
        default:
            if ((Errors() > 0) || (Asserts() > 0)) {
                // in this case we have called 'thread_exit' and we don't have a return code..
                SetResult(kTestResult_TestFail);
            } else {
                // This could be depending on 'strict' checking flag (fail in strict mode)
                SetResult(kTestResult_InvalidReturnCode);
            }

    }
}

TestResult::kRunResultAction TestResult::CheckIfContinue() const {
    //auto pLogger = gnilk::Logger::GetLogger("TestResult");

    if (testResult == kTestResult_ModuleFail) {
        if (Config::Instance().skipOnModuleFail) {
            // pLogger->Info("Module test failure, skipping remaining test cases in library");
            return kRunResultAction::kAbortModule;
        } else {
            // pLogger->Info("Module test failure, continue anyway (configuration)");
        }
    } else if (testResult == kTestResult_AllFail) {
        if (Config::Instance().stopOnAllFail) {
            // pLogger->Critical("Total test failure, aborting");
            return kRunResultAction::kAbortAll;
        } else {
            // pLogger->Info("Total test failure, continue anyway (configuration)");
        }
    }
    return kRunResultAction::kContinue;

}

