//
// Created by gnilk on 20.09.22.
//

#include "reportjson.h"
#include "../resultsummary.h"
#include <map>
#include <string>
#include <cstdint>
#include <cstdio>

void ResultsReportJSON::Begin() {
    fout = stdout;
    fprintf(fout, "{\n");
}
void ResultsReportJSON::End() {
    printf("\n");
    fprintf(fout, "}\n");
}
void ResultsReportJSON::PrintSummary() {
    fprintf(fout, "  \"Summary\":{\n");
    fprintf(fout, "    \"DurationSec\":%f,\n", ResultSummary::Instance().durationSec);
    fprintf(fout, "    \"TestsExecuted\":%d,\n", ResultSummary::Instance().testsExecuted);
    fprintf(fout, "    \"TestsFailed\":%d,\n", ResultSummary::Instance().testsFailed);
    fprintf(fout, "  }");
    bHadSummary = true;

}
void ResultsReportJSON::PrintFailures(std::vector<TestResult *> &results) {
    if (bHadSuccess || bHadSummary) {
        fprintf(fout,",\n");
    }

    fprintf(fout, "  \"Failures\":[\n");
    static std::map<kTestResult, std::string> resultToName={
            {kTestResult_Pass, "Pass"},
            {kTestResult_AllFail, "AllFail"},
            {kTestResult_ModuleFail,"ModuleFail"},
            {kTestResult_TestFail, "TestFail"},
    };
    bool bNeedComma = false;

    for (auto r : results) {
        if (r->Result() != kTestResult_Pass) {
            if (bNeedComma) {
                fprintf(fout, ",\n");
            }
            // Only print this the first time if we have any...
            fprintf(fout, "      {\n");
            fprintf(fout, "         \"Status\":\"%s\",\n", resultToName[r->Result()].c_str());
            fprintf(fout, "         \"Symbol\":\"%s\",\n", r->SymbolName().c_str());
            if (r->AssertError().isValid) {
                fprintf(fout, "         \"AssertValid\":true,\n");
                fprintf(fout, "         \"Assert\": {\n");
                fprintf(fout, "            \"File\": \"%s\",\n", r->AssertError().file.c_str());
                fprintf(fout, "            \"Line\": %d,\n", r->AssertError().line);
                fprintf(fout, "            \"Message\": \"%s\"\n", r->AssertError().message.c_str());
                fprintf(fout, "         }\n");
            } else {
                fprintf(fout, "         \"AssertValid\":false\n");
            }
            fprintf(fout, "      }");
            bNeedComma = true;
        }
    }
    fprintf(fout, "\n");
    fprintf(fout, "   ]");
    bHadFailures = true;
}
void ResultsReportJSON::PrintPasses(std::vector<TestResult *> &results) {
    if (bHadFailures || bHadSummary) {
        fprintf(fout,",\n");
    }
    fprintf(fout, "  \"Passes\":[\n");
    static std::map<kTestResult, std::string> resultToName={
            {kTestResult_Pass, "Pass"},
            {kTestResult_AllFail, "AllFail"},
            {kTestResult_ModuleFail,"ModuleFail"},
            {kTestResult_TestFail, "TestFail"},
    };
    bool bNeedComma = false;

    for (auto r : results) {
        if (r->Result() != kTestResult_Pass) {
            continue;
        }
        if (bNeedComma) {
            fprintf(fout, ",\n");
        }

        // Only print this the first time if we have any...
        fprintf(fout, "      {\n");
        fprintf(fout, "         \"Status\":\"%s\",\n", resultToName[r->Result()].c_str());
        fprintf(fout, "         \"Symbol\":\"%s\",\n", r->SymbolName().c_str());
        fprintf(fout, "      }");
        bNeedComma = true;
    }
    fprintf(fout, "\n");
    fprintf(fout, "   ]");

    bHadSuccess = true;
}
