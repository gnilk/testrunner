/*-------------------------------------------------------------------------
 File    : resultsummary.cpp
 Author  : FKling
 Version : -
 Orginal : 2022-08-17
 Descr   : Result summary and report execution

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
 </pre>

 \History
 - 2022.08.17, FKling, Implementation
 ---------------------------------------------------------------------------*/
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


#ifdef TRUN_HAVE_THREADS
#ifndef WIN32
    #include "unix/IPCFifoUnix.h"
#endif
#include "ipc/IPCBase.h"
#include "IPCMessages.h"
#include "ipc/IPCCore.h"
#include "ipc/IPCBufferedWriter.h"
#include "ipc/IPCEncoder.h"
#endif

using namespace trun;

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

    if (Config::Instance().isSubProcess) {
        SendResultToParentProc();
        return;
    }


    // strutil mutates the incoming string - let's not do that in this instance...
    auto reportingModule = std::string(Config::Instance().reportingModule);
    trun::to_lower(reportingModule);

    if (reportFactories.find(reportingModule) == reportFactories.end()) {
        // not found, or special name..
        if (reportingModule != "list") {
            printf("ERR: No such reporting library '%s'\n", Config::Instance().reportingModule.c_str());
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

#ifdef TRUN_HAVE_THREADS
    std::lock_guard<std::mutex> guard(lock);
#endif

    testFunctions.push_back(tfunc);
    results.push_back(result);

    testsExecuted++;
    if (result->Result() != kTestResult_Pass) {
        testsFailed++;
    }
}

// Really dislike CPP for 'simple' stuff (add <value>, esi)
//
template<typename TTo, typename TFrom>
TTo *PtrAdvanceFromTo(void *base) {
    auto toVoid = static_cast<void *>(static_cast<uint8_t *>(base) + sizeof(TFrom));
    return static_cast<TTo *>(toVoid);
}

void ResultSummary::SendResultToParentProc() {
#ifdef TRUN_HAVE_FORK
    gnilk::IPCFifoUnix ipc;

    // Now, try to connect to the other side...
    if (!ipc.ConnectTo(Config::Instance().ipcName)) {
        return;
    }


    IPCResultSummary summary;
    summary.testsExecuted = testsExecuted;
    summary.testsFailed = testsFailed;
    summary.durationSec = durationSec;
    // Create the test results objects
    for(auto res : results) {
        auto tr = new IPCTestResults(res);
        tr->symbolName = res->SymbolName();
        // add to the ipc summary
        summary.testResults.push_back(tr);
    }

        gnilk::IPCBufferedWriter bufferedWriter(ipc);
        gnilk::IPCBinaryEncoder encoder(bufferedWriter);

        summary.Marshal(encoder);
        // Flush and send...
        bufferedWriter.Flush();
        ipc.Close();
#endif
}


ResultSummary &ResultSummary::Instance() {
    static ResultSummary glbInstance;
    return glbInstance;
}