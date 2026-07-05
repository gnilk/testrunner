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
void TestResult::SetErrorString(const std::string &error) {
    errorString = error;
}
void TestResult::SetTestResultFromReturnCode(int testReturnCode) {
    // Thin wrapper over the pure decision below - kept for callers that only have a
    // return code (a normal, non-terminated case).
    SetResult(DeriveResult(CaseTermination::Returned, testResult, testReturnCode,
                           Errors(), (int)Asserts(), Config::Instance().discardTestReturnCode));
}

// Reconciles the return code (Channel A) and the ITesting-callback severity (Channel B,
// carried in proxyResult) into one result. The intended rule is a monotonic max on the
// severity ladder (Pass<TestFail<ModuleFail<AllFail), where 'termination' decides which
// channels get a vote; --discard silences A; an unrecognised return code is a diagnostic
// (InvalidReturnCode), not a pass. Full contract: todo/result_model.md "Authority contract".
kTestResult TestResult::DeriveResult(CaseTermination termination, kTestResult proxyResult,
                                     int returnCode, int errors, int asserts,
                                     bool discardReturnCode) {
    switch (termination) {
        case CaseTermination::ForciblyTerminated :
            // Fatal/Abort/V1-assert unwound the case mid-body. The proxy already recorded
            // the intended severity (ModuleFail/AllFail/TestFail) - keep it, so "stop the
            // module" / "stop the run" survive. (Defensive: a flagged failure with no
            // severity set still fails.)
            if ((proxyResult == kTestResult_Pass) && ((errors > 0) || (asserts > 0))) {
                return kTestResult_TestFail;
            }
            return proxyResult;
        case CaseTermination::UserException :
            // A C++ exception escaped the body - a failure, unless a stronger module/all
            // severity was already recorded before it was thrown.
            if ((proxyResult == kTestResult_ModuleFail) || (proxyResult == kTestResult_AllFail)) {
                return proxyResult;
            }
            return kTestResult_TestFail;
        case CaseTermination::Returned :
            break;
    }

    // Normal return: honour the test's return code (unless configured to discard it).
    if (discardReturnCode) {
        return proxyResult;
    }
    switch (returnCode) {
        case kTR_Pass :
            // Pass return code, but a flagged error/assert still fails (strict).
            return ((errors == 0) && (asserts == 0)) ? kTestResult_Pass : kTestResult_TestFail;
        case kTR_Fail :
            return kTestResult_TestFail;
        case kTR_FailModule :
            return kTestResult_ModuleFail;
        case kTR_FailAll :
            return kTestResult_AllFail;
        default:
            // No usable return code (e.g. a forced thread-exit with no exceptions). If
            // something was flagged, keep an already-set severity, else it's a plain
            // fail; otherwise the return code itself is invalid.
            if ((errors > 0) || (asserts > 0)) {
                return (proxyResult == kTestResult_Pass) ? kTestResult_TestFail : proxyResult;
            }
            return kTestResult_InvalidReturnCode;
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

