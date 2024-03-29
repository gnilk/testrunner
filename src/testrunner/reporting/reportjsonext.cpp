//
// Created by gnilk on 21.09.22.
//
#include "reportingbase.h"
#include "reportjsonext.h"
#include "../resultsummary.h"
#include "../testfunc.h"
#include "../testresult.h"
using namespace trun;

void ResultsReportJSONExtensive::Begin() {
    ResultsReportJSON::Begin();
}
void ResultsReportJSONExtensive::End() {
    ResultsReportJSON::End();
}

void ResultsReportJSONExtensive::PrintReport() {
    ResultsReportJSON::PrintSummary();
    DumpNewStruct();
}


void ResultsReportJSONExtensive::DumpNewStruct() {

    if (bHadFailures || bHadSummary || bHadSuccess) {
        WriteNoIndent(",\n");
    }

    auto results = ResultSummary::Instance().Results();
    auto testFunctions = ResultSummary::Instance().TestFunctions();

    static std::map<kTestResult, std::string> resultToName={
            {kTestResult_Pass, "Pass"},
            {kTestResult_AllFail, "AllFail"},
            {kTestResult_ModuleFail,"ModuleFail"},
            {kTestResult_TestFail, "TestFail"},
    };

    WriteLine(R"("Results" : [)");
    PushIndent();

    bool bNeedComma = false;
    for (auto testFunction : testFunctions) {
        if (bNeedComma) {
            WriteNoIndent(",\n");
        }
        PrintFuncResult(testFunction);
        bNeedComma = true;
    }
    PopIndent();
    WriteLine("");
    Write("]");
}

void ResultsReportJSONExtensive::PrintFuncResult(const TestFunc::Ref &testFunction) {
    WriteLine("{");
    PushIndent();
    if (testFunction->Library() != nullptr) {
        WriteLine(R"("Library" : "%s",)", testFunction->Library()->Name().c_str());
    }
    WriteLine(R"("Module" : "%s",)", testFunction->ModuleName().c_str());
    WriteLine(R"("Case" : "%s",)", testFunction->CaseName().c_str());
    WriteLine(R"("Result" :)");
    PrintTestResult(testFunction->Result());
    WriteLine("");
    PopIndent();
    Write("}");
}
