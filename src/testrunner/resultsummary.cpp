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

// Include any reporting module we have
#include "reporting/reportjson.h"
#include "reporting/reportconsole.h"

// Only one instance...
static ResultSummary *glb_Instance = nullptr;

using ReportFactory = std::function<ResultsReportPinterBase *()>;

// Add new reporting modules here
// NOTE: Must be lowercase for module name - we are converting everything to lower case before lookup
// DO NOT USE THE SPECIAL NAME 'list'
static std::map<std::string_view, ReportFactory > reportFactories = {
        {"console",[] () { return new ResultsReportConsole(); } },
        {"json",[] () { return new ResultsReportJSON(); } },
};

void ResultSummary::PrintSummary(bool bPrintSuccess) {

    // strutil mutates the incoming string - let's not do that in this instance...
    auto reportingModule = std::string(Config::Instance()->reportingModule);
    strutil::to_lower(reportingModule);

    if (reportFactories.find(reportingModule) == reportFactories.end()) {
        // not found, or special name..
        if (reportingModule != "list") {
            printf("ERR: No such reporting module '%s'\n", Config::Instance()->reportingModule.c_str());
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
    reportInstance->PrintSummary();
    reportInstance->PrintFailures(results);
    if (bPrintSuccess) {
        reportInstance->PrintPasses(results);
    }
    reportInstance->End();
}

void ResultSummary::ListReportingModules() {
    printf("Reporting modules:\n");
    for(auto rm : reportFactories) {
        printf("  %s\n", rm.first.data());
    }
}


ResultSummary &ResultSummary::Instance() {
    if (glb_Instance == nullptr) {
        glb_Instance = new ResultSummary;
    }
    return *glb_Instance;
}