//
// Created by gnilk on 20.09.22.
//

#ifndef TESTRUNNER_REPORTINGBASE_H
#define TESTRUNNER_REPORTINGBASE_H

#include <vector>
#include "../testresult.h"


class ResultsReportPinterBase {
public:
    ResultsReportPinterBase() = default;
    virtual ~ResultsReportPinterBase() = default;
    virtual void Begin() {}
    virtual void End() {}
    virtual void PrintSummary() {}
    virtual void PrintFailures([[maybe_unused]] std::vector<TestResult *> &results) {}
    virtual void PrintPasses([[maybe_unused]] std::vector<TestResult *> &results) {}

};



#endif //TESTRUNNER_REPORTINGBASE_H
