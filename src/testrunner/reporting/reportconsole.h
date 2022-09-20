//
// Created by gnilk on 20.09.22.
//

#ifndef TESTRUNNER_REPORTCONSOLE_H
#define TESTRUNNER_REPORTCONSOLE_H

#include "reportingbase.h"

class ResultsReportConsole : public ResultsReportPinterBase {
public:
    void Begin() override;
    void PrintSummary() override;
    void PrintFailures(std::vector<TestResult *> &results) override;
    void PrintPasses(std::vector<TestResult *> &results) override;
};

#endif //TESTRUNNER_REPORTCONSOLE_H
