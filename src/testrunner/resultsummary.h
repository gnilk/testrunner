#pragma once
#include "platform.h"

#include <map>
#include <vector>
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
    private:
        std::vector<TestResult::Ref> results;
        std::vector<TestFunc::Ref > testFunctions;
        ResultSummary() = default;
    };
}