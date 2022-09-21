//
// Created by gnilk on 20.09.22.
//

#ifndef TESTRUNNER_REPORTINGBASE_H
#define TESTRUNNER_REPORTINGBASE_H

#include <vector>
#include "../testresult.h"


class ResultsReportPinterBase {
public:
    ResultsReportPinterBase();
    virtual ~ResultsReportPinterBase() = default;
    virtual void Begin() {}
    virtual void End() {}
    virtual void PrintReport() {};


//    virtual void PrintSummary() {}
//    virtual void PrintFailures([[maybe_unused]] const std::vector<const TestResult *> &results) {}
//    virtual void PrintPasses([[maybe_unused]] const std::vector<const TestResult *> &results) {}


    // 0 - means use from config
    void PushIndent();
    void PopIndent();
    void WriteLine(const char *format,...);
    void Write(const char *format,...);
    void WriteNoIndent(const char *format,...);

protected:
    std::string indent;
    FILE *fout = nullptr;

};



#endif //TESTRUNNER_REPORTINGBASE_H
