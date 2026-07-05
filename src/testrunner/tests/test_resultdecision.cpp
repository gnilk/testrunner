//
// Unit tests for the pure result-decision logic (TestResult::DeriveResult) and the
// runner's continue/abort action (TestResult::CheckIfContinue).
//
// These pin the fix for the bug where a t->Fatal()/t->Abort() (implemented via a forced
// thread unwind) was scored as a plain TestFail instead of ModuleFail/AllFail, so "stop
// the module" / "stop the run" silently stopped working in the default (threaded +
// exceptions) build. Because DeriveResult is pure, we can assert the outcome WITHOUT
// running - and terminating - a real aborting test case.
//
#include "testinterface.h"
#include "../testresult.h"
#include "../config.h"
#include "../responseproxy.h"
#include <string>

using namespace trun;

extern "C" {
    DLL_EXPORT int test_resultdecision(ITesting *t);
    DLL_EXPORT int test_resultdecision_forced(ITesting *t);
    DLL_EXPORT int test_resultdecision_userexception(ITesting *t);
    DLL_EXPORT int test_resultdecision_returncode(ITesting *t);
    DLL_EXPORT int test_resultdecision_discard(ITesting *t);
    DLL_EXPORT int test_resultdecision_checkifcontinue(ITesting *t);
    DLL_EXPORT int test_resultdecision_errordetail(ITesting *t);
}

// module main - nothing to set up
DLL_EXPORT int test_resultdecision(ITesting *t) {
    return kTR_Pass;
}

//
// Forced termination (Fatal / Abort / V1-assert): the severity the proxy already recorded
// MUST survive - this is the actual regression.
//
DLL_EXPORT int test_resultdecision_forced(ITesting *t) {
    using CT = TestResult::CaseTermination;

    // Fatal -> proxy recorded ModuleFail; must stay ModuleFail (not downgraded to TestFail).
    TR_ASSERT(t, TestResult::DeriveResult(CT::ForciblyTerminated, kTestResult_ModuleFail, kTR_Pass, 1, 0, false) == kTestResult_ModuleFail);
    // Abort -> proxy recorded AllFail; must stay AllFail.
    TR_ASSERT(t, TestResult::DeriveResult(CT::ForciblyTerminated, kTestResult_AllFail, kTR_Pass, 1, 0, false) == kTestResult_AllFail);
    // V1 assert forced-terminate -> proxy recorded TestFail; stays TestFail.
    TR_ASSERT(t, TestResult::DeriveResult(CT::ForciblyTerminated, kTestResult_TestFail, kTR_Pass, 0, 1, false) == kTestResult_TestFail);
    // Defensive: flagged (error/assert) but no severity recorded -> at least TestFail.
    TR_ASSERT(t, TestResult::DeriveResult(CT::ForciblyTerminated, kTestResult_Pass, kTR_Pass, 1, 0, false) == kTestResult_TestFail);

    return kTR_Pass;
}

//
// A genuine C++ exception escaping the test body is a failure, but must not swallow an
// already-recorded stronger severity.
//
DLL_EXPORT int test_resultdecision_userexception(ITesting *t) {
    using CT = TestResult::CaseTermination;

    TR_ASSERT(t, TestResult::DeriveResult(CT::UserException, kTestResult_Pass, kTR_Pass, 0, 0, false) == kTestResult_TestFail);
    TR_ASSERT(t, TestResult::DeriveResult(CT::UserException, kTestResult_ModuleFail, kTR_Pass, 0, 0, false) == kTestResult_ModuleFail);
    TR_ASSERT(t, TestResult::DeriveResult(CT::UserException, kTestResult_AllFail, kTR_Pass, 0, 0, false) == kTestResult_AllFail);

    return kTR_Pass;
}

//
// Normal return: honour the return code (existing behaviour, kept intact by the refactor).
//
DLL_EXPORT int test_resultdecision_returncode(ITesting *t) {
    using CT = TestResult::CaseTermination;

    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, kTR_Pass, 0, 0, false) == kTestResult_Pass);
    // Pass return code but a flagged error/assert -> TestFail (strict).
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, kTR_Pass, 1, 0, false) == kTestResult_TestFail);
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, kTR_Fail, 0, 0, false) == kTestResult_TestFail);
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, kTR_FailModule, 0, 0, false) == kTestResult_ModuleFail);
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, kTR_FailAll, 0, 0, false) == kTestResult_AllFail);
    // No usable return code, nothing flagged -> invalid.
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, -1, 0, 0, false) == kTestResult_InvalidReturnCode);
    // No-exceptions forced thread-exit: no return code, but severity was recorded -> keep it.
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_ModuleFail, -1, 1, 0, false) == kTestResult_ModuleFail);

    return kTR_Pass;
}

//
// With --discard the return code is ignored and the proxy result stands.
//
DLL_EXPORT int test_resultdecision_discard(ITesting *t) {
    using CT = TestResult::CaseTermination;

    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_ModuleFail, kTR_Pass, 0, 0, true) == kTestResult_ModuleFail);
    TR_ASSERT(t, TestResult::DeriveResult(CT::Returned, kTestResult_Pass, kTR_FailAll, 0, 0, true) == kTestResult_Pass);

    return kTR_Pass;
}

//
// The result code must drive the runner's continue/abort action. This is the second half
// of the fix: ModuleFail stops the module, AllFail stops the run.
//
DLL_EXPORT int test_resultdecision_checkifcontinue(ITesting *t) {
    // CheckIfContinue reads these config flags - set them explicitly and restore after.
    bool origSkipOnModuleFail = Config::Instance().skipOnModuleFail;
    bool origStopOnAllFail = Config::Instance().stopOnAllFail;
    Config::Instance().skipOnModuleFail = true;
    Config::Instance().stopOnAllFail = true;

    TestResult modFail("dummy");
    modFail.SetResult(kTestResult_ModuleFail);
    TR_ASSERT(t, modFail.CheckIfContinue() == TestResult::kRunResultAction::kAbortModule);

    TestResult allFail("dummy");
    allFail.SetResult(kTestResult_AllFail);
    TR_ASSERT(t, allFail.CheckIfContinue() == TestResult::kRunResultAction::kAbortAll);

    TestResult testFail("dummy");
    testFail.SetResult(kTestResult_TestFail);
    TR_ASSERT(t, testFail.CheckIfContinue() == TestResult::kRunResultAction::kContinue);

    Config::Instance().skipOnModuleFail = origSkipOnModuleFail;
    Config::Instance().stopOnAllFail = origStopOnAllFail;
    return kTR_Pass;
}

//
// Error() must record a file/line/message into the assert list (like Fatal/Abort/AssertError
// already do), so an Error-only failure carries detail for the reporters and across the fork
// boundary (IPCTestResults marshals the list when TestResult::Asserts() == NumErrors() > 0).
// Regression for the "Error adds no assertError detail" gap.
//
// Note: under the normal V2 config Error() flags-and-continues (it only force-terminates in
// forced mode - V1 / --allow-thread-exit), so driving a local proxy here is safe.
//
DLL_EXPORT int test_resultdecision_errordetail(ITesting *t) {
    TestResponseProxy proxy;
    proxy.Begin("test_dummy_case", "dummy");
    TR_ASSERT(t, proxy.GetAssertError().NumErrors() == 0);

    proxy.Error(4711, "dummyfile.cpp", "boom");
    proxy.End();

    // The soft-fail severity and error count are as before...
    TR_ASSERT(t, proxy.Result() == kTestResult_TestFail);
    TR_ASSERT(t, proxy.Errors() == 1);
    // ...but now the detail is recorded exactly once, classed as a general error.
    TR_ASSERT(t, proxy.GetAssertError().NumErrors() == 1);

    const auto &item = proxy.GetAssertError().Errors().front();
    TR_ASSERT(t, item.assertClass == AssertError::kAssert_Error);
    TR_ASSERT(t, item.line == 4711);
    TR_ASSERT(t, item.file == std::string("dummyfile.cpp"));
    TR_ASSERT(t, item.message == std::string("boom"));

    return kTR_Pass;
}
