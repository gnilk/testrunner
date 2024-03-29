//
// Created by gnilk on 20.09.22.
//

#ifndef TESTRUNNER_REPORTJSON_H
#define TESTRUNNER_REPORTJSON_H

#include "reportingbase.h"
#include "../testfunc.h"
#include "../testresult.h"


namespace trun {
    class ResultsReportJSON : public ResultsReportPinterBase {
    public:
        void Begin() override;
        void End() override;
        void PrintReport() override;
    protected:
        void PrintSummary();
        void PrintFailures(const std::vector<TestResult::Ref> &results);
        void PrintPasses(const std::vector<TestResult::Ref> &results);
    protected:
        void PrintTestResult(const TestResult::Ref result);
        std::string EscapeString(const std::string &str);
    protected:
        bool bHadFailures = false;
        bool bHadSuccess = false;
        bool bHadSummary = false;
    };
}



#endif //TESTRUNNER_REPORTJSON_H
