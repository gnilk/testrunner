#pragma once
#include "platform.h"

#include <map>
#include <vector>
#include <mutex>
#include <string>
#include <unordered_set>
#include "testfunc.h"
#include "testresult.h"

namespace trun {
    class ResultSummary {
    public:
        static ResultSummary &Instance();
        void PrintSummary();

        void AddResult(const TestFunc::Ref tfunc);
        void ListReportingModules();

        const std::vector<TestResult::Ref> &Results() const {
            return results;
        }
        const std::vector<TestFunc::Ref> &TestFunctions() const {
            return testFunctions;
        }
    public:
        int testsExecuted = 0;
        int testsFailed = 0;
        double durationSec = 0.0;
    protected:
        // Protected (not private) so a test can subclass for an isolated instance;
        // production still goes through Instance(). It was private only as the
        // singleton idiom - nothing else depends on it.
        ResultSummary() = default;

        void SendResultToParentProc();

    private:
#ifdef TRUN_HAVE_THREADS
        std::mutex lock;
#endif
        std::vector<TestResult::Ref> results;
        std::vector<TestFunc::Ref > testFunctions;
        std::unordered_set<std::string> seenSymbols;    // de-dup key: test symbol name
    };
}