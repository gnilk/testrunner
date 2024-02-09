//
// Created by gnilk on 20.09.22.
//

#include "reportjson.h"
#include "../resultsummary.h"
#include <map>
#include <string>
#include <cstdint>
#include <cstdio>
using namespace trun;

void ResultsReportJSON::Begin() {
    ResultsReportPinterBase::Begin();
    WriteLine("{");
    PushIndent();
}
void ResultsReportJSON::End() {
    WriteLine("");
    PopIndent();
    WriteLine("}");


    ResultsReportPinterBase::End();
}

void ResultsReportJSON::PrintReport() {
    PrintSummary();
    PrintFailures(ResultSummary::Instance().Results());
    if (Config::Instance()->printPassSummary) {
        PrintPasses(ResultSummary::Instance().Results());
    }
}

void ResultsReportJSON::PrintSummary() {

    WriteLine("\"Summary\":{");
    PushIndent();
    WriteLine("\"DurationSec\":%f,", ResultSummary::Instance().durationSec);
    WriteLine("\"TestsExecuted\":%d,", ResultSummary::Instance().testsExecuted);
    WriteLine("\"TestsFailed\":%d", ResultSummary::Instance().testsFailed);
    PopIndent();
    Write("}");
    bHadSummary = true;
}
void ResultsReportJSON::PrintFailures(const std::vector<const TestResult *> &results) {
    if (bHadSuccess || bHadSummary) {
        fprintf(fout,",\n");
    }

    WriteLine("\"Failures\":[");
    PushIndent();
    bool bNeedComma = false;

    for (auto r : results) {
        if (r->Result() != kTestResult_Pass) {
            if (bNeedComma) {
                WriteNoIndent(",\n");
            }
            PrintTestResult(r);
            bNeedComma = true;
        }
    }
    PopIndent();
    WriteLine("");
    Write("]");
    bHadFailures = true;
}

void ResultsReportJSON::PrintPasses(const std::vector<const TestResult *> &results) {
    if (bHadFailures || bHadSummary) {
        WriteNoIndent(",\n");
        //fprintf(fout,",\n");
    }
    WriteLine(R"("Passes":[)");
    PushIndent();
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
            WriteNoIndent(",\n");
        }
        PrintTestResult(r);
        bNeedComma = true;
    }
    PopIndent();
    WriteLine("");
    Write("]");

    bHadSuccess = true;
}

void ResultsReportJSON::PrintTestResult(const TestResult *result) {
    // Only print this the first time if we have any...
    static std::map<kTestResult, std::string> resultToName={
            {kTestResult_Pass, "Pass"},
            {kTestResult_TestFail, "TestFail"},
            {kTestResult_ModuleFail,"ModuleFail"},
            {kTestResult_AllFail, "AllFail"},
            {kTestResult_NotExecuted, "NotExecuted"},
            {kTestResult_InvalidReturnCode, "Invalid Return Code!"},
    };
    WriteLine("{");
    PushIndent();
    WriteLine(R"("Status" : "%s",)", resultToName[result->Result()].c_str());
    WriteLine(R"("Symbol" : "%s",)", result->SymbolName().c_str());
    WriteLine(R"("DurationSec" : %f,)", result->ElapsedTimeSec());
    if (result->AssertError().isValid) {
        WriteLine(R"("IsAssertValid" : true,)");
        WriteLine(R"("Assert" : {)");
        PushIndent();
        WriteLine(R"("File" : "%s",)", result->AssertError().file.c_str());
        WriteLine(R"("Line" : %d,)", result->AssertError().line);
        WriteLine(R"("Message" : "%s")", EscapeString(result->AssertError().message).c_str());
        PopIndent();
        WriteLine("}");
    } else {
        WriteLine(R"("IsAssertValid" : false)");
    }
    PopIndent();
    Write("}");

}

// Quick and dirty escaping special json chars..
std::string ResultsReportJSON::EscapeString(const std::string &str) {
    std::string strIllegal = "\"\\";
    std::string strEscaped;
    for(auto c : str) {
        // Just skip this - we do not forward tab's, bells, new lines and whatever...
        if (c < 31) continue;
        if (strIllegal.find(c) != std::string::npos) {
            strEscaped += '\\';
        }
        strEscaped += c;
    }

    return strEscaped;
}

