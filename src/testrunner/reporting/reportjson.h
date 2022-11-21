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
        void PrintFailures(const std::vector<const TestResult *> &results);
        void PrintPasses(const std::vector<const TestResult *> &results);
    protected:
        void PrintTestResult(const TestResult *result);
    protected:
        bool bHadFailures = false;
        bool bHadSuccess = false;
        bool bHadSummary = false;
    };
}



#endif //TESTRUNNER_REPORTJSON_H
