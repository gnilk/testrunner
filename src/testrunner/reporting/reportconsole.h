//
// Created by gnilk on 20.09.22.
//

#ifndef TESTRUNNER_REPORTCONSOLE_H
#define TESTRUNNER_REPORTCONSOLE_H

#include "reportingbase.h"

namespace trun {
    class ResultsReportConsole : public ResultsReportPinterBase {
    public:
        void Begin() override;
        void PrintReport() override;
    protected:
        void PrintSummary();
        void PrintFailures(const std::vector<const TestResult *> &results);
        void PrintPasses(const std::vector<const TestResult *> &results);
    };
}
#endif //TESTRUNNER_REPORTCONSOLE_H
