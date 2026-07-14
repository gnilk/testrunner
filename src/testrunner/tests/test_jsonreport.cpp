//
// Created by gnilk on 09.02.24.
//
#include <memory>

#include "../config.h"
#include "testinterface.h"
#include "../testresult.h"
#include "../resultsummary.h"
#include "../reporting/reportjson.h"

extern "C" {
DLL_EXPORT int test_jsonreport(ITesting *t);
DLL_EXPORT int test_jsonreport_escape(ITesting *t);
DLL_EXPORT int test_jsonreport_escapestring(ITesting *t);
}

DLL_EXPORT int test_jsonreport(ITesting *t) {
    return kTR_Pass;
}

// Promotes the protected EscapeString to public so it can be unit-tested directly.
namespace {
    struct EscapeProbe : public trun::ResultsReportJSON {
        using trun::ResultsReportJSON::EscapeString;
    };
}

DLL_EXPORT int test_jsonreport_escapestring(ITesting *t) {
    EscapeProbe probe;

    // The two chars that are illegal raw in a JSON string must be backslash-escaped.
    TR_ASSERT(t, probe.EscapeString("a\"b") == "a\\\"b");

    // A Windows path (Symbol/File fields) used to be emitted raw, breaking the JSON.
    // Every backslash must be doubled.
    TR_ASSERT(t, probe.EscapeString("C:\\src\\foo.cpp") == "C:\\\\src\\\\foo.cpp");

    // Control chars must be escaped, not dropped: named short escapes...
    TR_ASSERT(t, probe.EscapeString("\n") == "\\n");
    TR_ASSERT(t, probe.EscapeString("\t") == "\\t");
    // ...and other control chars as \u00XX.
    TR_ASSERT(t, probe.EscapeString("\x01") == "\\u0001");

    // UTF-8 bytes (>= 0x80) must survive - the old signed-char code dropped them.
    // "é" == 0xC3 0xA9; both bytes must pass through untouched.
    std::string utf8 = "\xC3\xA9";
    TR_ASSERT(t, probe.EscapeString(utf8) == utf8);

    // A plain string is returned verbatim.
    TR_ASSERT(t, probe.EscapeString("hello") == "hello");

    return kTR_Pass;
}
DLL_EXPORT int test_jsonreport_escape(ITesting *t) {
    auto tfunc = std::make_shared<trun::TestFunc>();
    auto result = trun::TestResult::Create("dummy");
    result->SetResult(trun::kTestResult_TestFail);
    trun::AssertError assertError;
    auto assertErrMsg = std::string("fopen(\"filename.txt\",\"r\");");
    assertError.Add(trun::AssertError::kAssert_Error, 0, "dummy.cpp", assertErrMsg);
    result->SetAssertError(assertError);

    tfunc->SetResultFromSubProcess(result);

    // Add this to the global result summary instance
    trun::ResultSummary::Instance().AddResult(tfunc);

    trun::ResultsReportJSON jsonReport;
    jsonReport.Begin();
    jsonReport.PrintReport();
    jsonReport.End();

    // I don't really have an interface to deal with testing the result without loading and doing shit..
    // this is verified through visual inspection of the result!

    return kTR_Pass;
}
