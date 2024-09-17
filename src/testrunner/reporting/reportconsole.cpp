//
// Created by gnilk on 20.09.22.
//

#include "../resultsummary.h"
#include "reportconsole.h"
#include <cstdio>
using namespace trun;


void ResultsReportConsole::Begin() {
    ResultsReportPinterBase::Begin();
    if (fout == stdout) {
        fprintf(fout, "\n-------------------\n");
    }
}

void ResultsReportConsole::End() {
    ResultsReportPinterBase::End();
}


void ResultsReportConsole::PrintReport() {
    PrintSummary();
    PrintFailures(ResultSummary::Instance().Results());
    if (Config::Instance().printPassSummary) {
        PrintPasses(ResultSummary::Instance().Results());
    }
}


void ResultsReportConsole::PrintSummary() {
    fprintf(fout, "Duration......: %.3f sec\n", ResultSummary::Instance().durationSec);
    fprintf(fout, "Tests Executed: %d\n", ResultSummary::Instance().testsExecuted);
    fprintf(fout, "Tests Failed..: %d\n", ResultSummary::Instance().testsFailed);
}

void ResultsReportConsole::PrintFailures(const std::vector<TestResult::Ref> &results) {
    bool haveHeader = false;
    static char lineHeader[64];
    for(auto r : results) {
        if (r->Result() != kTestResult_Pass) {
            // Only print this the first time if we have any...
            if (!haveHeader) {
                fprintf(fout, "Failed:\n");
                haveHeader = true;
            }
            auto nChars = snprintf(lineHeader, sizeof(lineHeader),"  [%c%c%c]: %s",
                     r->Result() == kTestResult_TestFail?'T':'t',
                     r->Result() == kTestResult_ModuleFail?'M':'m',
                     r->Result() == kTestResult_AllFail?'A':'a',
                     r->SymbolName().c_str());

            if (nChars < 0) {
                continue;
            }
            // Append if there is room left...
            if ((nChars < sizeof(lineHeader)) && (r->FailState() != TestResult::kFailState::Main)) {
                //fprintf(fout, " (%s)", r->FailStateName().c_str());
                snprintf(&lineHeader[nChars], sizeof(lineHeader) - nChars, " (%s)", r->FailStateName().c_str());
            }

            if (r->AssertError().NumErrors() > 0) {
                for(auto aerr : r->AssertError().Errors()) {
                    fprintf(fout, "%s, %s:%d, %s\n", lineHeader,
                            aerr.file.c_str(),
                            aerr.line,
                            aerr.message.c_str());
                }
            } else {
                fprintf(fout, "%s\n", lineHeader);
            }
        }
    }
}
void ResultsReportConsole::PrintPasses(const std::vector<TestResult::Ref> &results) {
    bool haveHeader = false;
    for(auto r : results) {
        if (r->Result() == kTestResult_Pass) {
            // Only print this the first time - if we have any...
            if (!haveHeader) {
                fprintf(fout, "Success:\n");
                haveHeader = true;
            }
            fprintf(fout, "  [P--]: %s\n",r->SymbolName().c_str());
        }
    }
}
