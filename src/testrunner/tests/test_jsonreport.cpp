//
// Created by gnilk on 09.02.24.
//
#include "../config.h"
#include "../testinterface.h"
#include "../testresult.h"
#include "../resultsummary.h"
#include "../reporting/reportjson.h"

extern "C" {
DLL_EXPORT int test_jsonreport(ITesting *t);
DLL_EXPORT int test_jsonreport_escape(ITesting *t);
}

DLL_EXPORT int test_jsonreport(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_jsonreport_escape(ITesting *t) {
    trun::TestFunc tfunc;
    trun::TestResult result("dummy");
    result.SetResult(trun::kTestResult_TestFail);
    trun::AssertError assertError;
    auto assertErrMsg = std::string("fopen(\"filename.txt\",\"r\");");
    assertError.Set(trun::AssertError::kAssert_Error, 0, "dummy.cpp", assertErrMsg);
    result.SetAssertError(assertError);

    tfunc.UTEST_SetMockResultPtr(&result);

    // Add this to the global result summary instance
    trun::ResultSummary::Instance().AddResult(&tfunc);

    trun::ResultsReportJSON jsonReport;
    jsonReport.Begin();
    jsonReport.PrintReport();
    jsonReport.End();

    // I don't really have an interface to deal with testing the result without loading and doing shit..
    // this is verified through visual inspection of the result!

    return kTR_Pass;
}
