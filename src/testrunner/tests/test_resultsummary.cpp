//
// Tests for ResultSummary de-duplication.
//
// Under forked execution a module pulled in as a dependency is run (and
// reported) by several child processes, so the same test symbol arrives at the
// parent more than once. ResultSummary must keep each test symbol only once.
//
#include "testresult.h"
#include "testfunc.h"
#include "resultsummary.h"
#include "ext_testinterface/testinterface.h"

#include <string>

extern "C" {
DLL_EXPORT int test_resultsummary(ITesting *t);
DLL_EXPORT int test_resultsummary_dedup(ITesting *t);
}

// Test seam: ResultSummary's ctor is protected (singleton). Subclassing gives us
// an isolated instance to exercise AddResult without touching the live Instance().
class MockResultSummary : public trun::ResultSummary {
public:
    MockResultSummary() = default;
};

// Build a TestFunc carrying a mock result for the given symbol.
static trun::TestFunc::Ref MakeResult(const std::string &symbol, trun::kTestResult code) {
    auto tr = trun::TestResult::Create(symbol);
    tr->SetResult(code);
    auto func = trun::TestFunc::Create(symbol, "dupmod", "case");
    func->SetResultFromSubProcess(tr);
    return func;
}

DLL_EXPORT int test_resultsummary(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_resultsummary_dedup(ITesting *t) {
    // Isolated instance so we don't disturb the live run's ResultSummary::Instance().
    MockResultSummary summary;

    summary.AddResult(MakeResult("test_dupmod_one", trun::kTestResult::kTestResult_Pass));
    summary.AddResult(MakeResult("test_dupmod_two", trun::kTestResult::kTestResult_TestFail));

    // Two distinct symbols: both counted.
    TR_ASSERT(t, summary.Results().size() == 2);
    TR_ASSERT(t, summary.testsExecuted == 2);
    TR_ASSERT(t, summary.testsFailed == 1);

    // Re-adding the same symbols (as a forked dependency run would) must be ignored.
    summary.AddResult(MakeResult("test_dupmod_one", trun::kTestResult::kTestResult_Pass));
    summary.AddResult(MakeResult("test_dupmod_two", trun::kTestResult::kTestResult_TestFail));

    TR_ASSERT(t, summary.Results().size() == 2);
    TR_ASSERT(t, summary.testsExecuted == 2);
    TR_ASSERT(t, summary.testsFailed == 1);

    return kTR_Pass;
}
