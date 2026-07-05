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
    if (Config::Instance().printPassSummary) {
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
void ResultsReportJSON::PrintFailures(const std::vector<TestResult::Ref > &results) {
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

void ResultsReportJSON::PrintPasses(const std::vector<TestResult::Ref> &results) {
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

void ResultsReportJSON::PrintTestResult(const TestResult::Ref result) {
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
    WriteLine(R"("Symbol" : "%s",)", EscapeString(result->SymbolName()).c_str());
    WriteLine(R"("DurationSec" : %f,)", result->ElapsedTimeSec());
    if (result->Result() != kTestResult_Pass) {
        WriteLine(R"("ExecutionState": "%s",)", result->FailStateName().c_str());
    }

    if (result->AssertError().IsValid()) {
        PrintAssertError(result);
    } else {
        WriteLine(R"("IsAssertValid" : false)");
    }
    PopIndent();
    Write("}");

}
void ResultsReportJSON::PrintAssertError(const TestResult::Ref result) {
    auto &aerr = result->AssertError().Errors().front();

    WriteLine(R"("IsAssertValid" : true,)");
    // Up to v2.0.0 we only had this
    WriteLine(R"("Assert" : {)");
    PrintAssert(aerr);
    // This array is new from v2.1.0
    if (result->AssertError().NumErrors() > 1) {
        WriteLine("},");
        // NOTE: this is assumed to be last item in the parent object...
        PrintAssertArray(result);
    } else {
        WriteLine("}");
    }
}

void ResultsReportJSON::PrintAssertArray(const TestResult::Ref result) {
    WriteLine(R"("Asserts" : [)");
    PushIndent();
    for(const auto &ass : result->AssertError().Errors()) {
        WriteLine("{");
        PrintAssert(ass);
        // This must be the worst in a long while...
        if (&ass != &result->AssertError().Errors().back()) {
            WriteLine("},");
        } else {
            WriteLine("}");
        }
    }
    PopIndent();
    WriteLine("]");
}

void ResultsReportJSON::PrintAssert(const AssertError::AssertErrorItem &aerr) {
    PushIndent();
    WriteLine(R"("File" : "%s",)", EscapeString(aerr.file).c_str());
    WriteLine(R"("Line" : %d,)", aerr.line);
    WriteLine(R"("Message" : "%s")", EscapeString(aerr.message).c_str());
    PopIndent();
}

// Escape a string so it is a valid JSON string body (the text between the quotes).
// Escapes the two chars that are illegal raw in JSON (" and \), converts control
// chars (< 0x20) to their JSON escape, and passes bytes >= 0x80 through untouched so
// UTF-8 text survives - the old code treated 'char' as signed, so every byte >= 0x80
// compared < 31 and was silently dropped, mangling any non-ASCII message.
std::string ResultsReportJSON::EscapeString(const std::string &str) {
    std::string strEscaped;
    for(unsigned char c : str) {
        switch (c) {
            case '"'  : strEscaped += "\\\""; break;
            case '\\' : strEscaped += "\\\\"; break;
            case '\b' : strEscaped += "\\b"; break;
            case '\f' : strEscaped += "\\f"; break;
            case '\n' : strEscaped += "\\n"; break;
            case '\r' : strEscaped += "\\r"; break;
            case '\t' : strEscaped += "\\t"; break;
            default :
                if (c < 0x20) {
                    // Any other control char -> \u00XX (JSON requires < 0x20 be escaped)
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    strEscaped += buf;
                } else {
                    // Printable ASCII and raw UTF-8 continuation/lead bytes pass through
                    strEscaped += static_cast<char>(c);
                }
                break;
        }
    }

    return strEscaped;
}

