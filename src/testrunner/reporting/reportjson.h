//
// Created by gnilk on 20.09.22.
//

#ifndef TESTRUNNER_REPORTJSON_H
#define TESTRUNNER_REPORTJSON_H

#include "reportingbase.h"

class ResultsReportJSON : public ResultsReportPinterBase {
public:
    void Begin() override;
    void End() override;
    void PrintSummary() override;
    void PrintFailures(std::vector<TestResult *> &results) override;
    void PrintPasses(std::vector<TestResult *> &results) override;
private:
    bool bHadFailures = false;
    bool bHadSuccess = false;
    FILE *fout = nullptr;
};



#endif //TESTRUNNER_REPORTJSON_H
