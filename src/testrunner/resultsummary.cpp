//
// Created by Fredrik Kling on 17.08.22.
//
#include <map>
#include <functional>
#include <stdio.h>
#include <string_view>

#include "strutil.h"
#include "config.h"
#include "resultsummary.h"
#include "reporting/reportingbase.h"

// Include any reporting library we have
#include "reporting/reportconsole.h"

// This is defined for the console application but not for the embedded library (default)
#if defined(TRUN_HAVE_EXT_REPORTING)
#include "reporting/reportjson.h"
#include "reporting/reportjsonext.h"
#endif

using namespace trun;

// Only one instance...
static ResultSummary *glb_Instance = nullptr;

using ReportFactory = std::function<ResultsReportPinterBase *()>;

// Add new reporting modules here
// NOTE: Must be lowercase for library name - we are converting everything to lower case before lookup
// DO NOT USE THE SPECIAL NAME 'list'
static std::map<std::string_view, ReportFactory > reportFactories = {
        {"console",[] () { return new ResultsReportConsole(); } },
#if defined(TRUN_HAVE_EXT_REPORTING)
        {"json",[] () { return new ResultsReportJSON(); } },
        {"jsonext",[] () { return new ResultsReportJSONExtensive(); } },
#endif
};

void ResultSummary::PrintSummary() {

    // strutil mutates the incoming string - let's not do that in this instance...
    auto reportingModule = std::string(Config::Instance()->reportingModule);
    trun::to_lower(reportingModule);

    if (reportFactories.find(reportingModule) == reportFactories.end()) {
        // not found, or special name..
        if (reportingModule != "list") {
            printf("ERR: No such reporting library '%s'\n", Config::Instance()->reportingModule.c_str());
        }
        ListReportingModules();
        return;
    }

    // Create the reporting instance...
    auto reportInstance = reportFactories[reportingModule]();
    if (reportInstance == nullptr) {
        return;
    }

    reportInstance->Begin();
    reportInstance->PrintReport();
    reportInstance->End();
}

void ResultSummary::ListReportingModules() {
    printf("  Reporting modules:\n");
    for(auto rm : reportFactories) {
        printf("    %s\n", rm.first.data());
    }
}

void ResultSummary::AddResult(const TestFunc::Ref tfunc) {
    auto result = tfunc->Result();
    testFunctions.push_back(tfunc);
    results.push_back(result);

    //results.push_back(tfunc->);

    ResultSummary::Instance().testsExecuted++;
    if (result->Result() != kTestResult_Pass) {
        ResultSummary::Instance().testsFailed++;
    }

}

ResultSummary &ResultSummary::Instance() {
    if (glb_Instance == nullptr) {
        glb_Instance = new ResultSummary;
    }
    return *glb_Instance;
}