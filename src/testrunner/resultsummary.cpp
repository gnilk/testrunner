//
// Created by Fredrik Kling on 17.08.22.
//

#include "resultsummary.h"
#include <stdio.h>
#include <map>

class ResultsReportPinterBase {
public:
    ResultsReportPinterBase() = default;
    virtual ~ResultsReportPinterBase() = default;
    virtual void Begin() {}
    virtual void End() {}
    virtual void PrintSummary() {}
    virtual void PrintFailures(std::vector<TestResult *> &results) {}
    virtual void PrintPasses(std::vector<TestResult *> &results) {}

};

class ResultsReportConsole : public ResultsReportPinterBase {
public:
    void PrintSummary() override;
    void PrintFailures(std::vector<TestResult *> &results) override;
    void PrintPasses(std::vector<TestResult *> &results) override;
};

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

void ResultsReportConsole::PrintSummary() {

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

//
// JSON Reporting
//

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
    fprintf(fout, "  },");
}
void ResultsReportJSON::PrintFailures(std::vector<TestResult *> &results) {
    if (bHadSuccess) {
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
    if (bHadFailures) {
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








// Only one instance...
static ResultSummary *glb_Instance = nullptr;
static ResultsReportPinterBase *reportInstance = new ResultsReportJSON();

void ResultSummary::PrintSummary(bool bPrintSuccess) {
    reportInstance->Begin();
    reportInstance->PrintFailures(results);
    if (bPrintSuccess) {
        reportInstance->PrintPasses(results);
    }
    reportInstance->End();

//    PrintFailureDetails();
//    if (bPrintSuccess) {
//        PrintPassDetails();
//    }
}
void ResultSummary::PrintFailureDetails() {
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
void ResultSummary::PrintPassDetails() {
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

ResultSummary &ResultSummary::Instance() {
    if (glb_Instance == nullptr) {
        glb_Instance = new ResultSummary;
    }
    return *glb_Instance;
}