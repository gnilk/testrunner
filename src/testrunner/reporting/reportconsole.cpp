//
// Created by gnilk on 20.09.22.
//

#include "../resultsummary.h"
#include "reportconsole.h"
#include <cstdio>


void ResultsReportConsole::Begin() {
    printf("\n-------------------\n");
}


void ResultsReportConsole::PrintSummary() {
    printf("Duration......: %.3f sec\n", ResultSummary::Instance().durationSec);
    printf("Tests Executed: %d\n", ResultSummary::Instance().testsExecuted);
    printf("Tests Failed..: %d\n", ResultSummary::Instance().testsFailed);
}

void ResultsReportConsole::PrintFailures(std::vector<TestResult *> &results) {
    bool haveHeader = false;
    for(auto r : results) {
        if (r->Result() != kTestResult_Pass) {
            // Only print this the first time if we have any...
            if (!haveHeader) {
                printf("Failed:\n");
                haveHeader = true;
            }
            printf("  [%c%c%c]: %s",
                   r->Result() == kTestResult_TestFail?'T':'t',
                   r->Result() == kTestResult_ModuleFail?'M':'m',
                   r->Result() == kTestResult_AllFail?'A':'a',
                   r->SymbolName().c_str());

            if (r->AssertError().isValid) {
                printf(", %s:%d, %s", r->AssertError().file.c_str(), r->AssertError().line, r->AssertError().message.c_str());
            }
            printf("\n");
        }
    }
}
void ResultsReportConsole::PrintPasses(std::vector<TestResult *> &results) {
    bool haveHeader = false;
    for(auto r : results) {
        if (r->Result() == kTestResult_Pass) {
            // Only print this the first time - if we have any...
            if (!haveHeader) {
                printf("Success:\n");
                haveHeader = true;
            }
            printf("  [P--]: %s\n",r->SymbolName().c_str());
        }
    }
}
