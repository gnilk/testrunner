/*-------------------------------------------------------------------------
 File    : config.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Holds the configuration for this application (singleton)

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.12.21, FKling, Support for test case specification and skipping test_main
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/
#include "config.h"
#include "logger.h"
#include <string>
#include "resultsummary.h"

static Config *gblConfig = NULL;

using namespace gnilk;


Config *Config::Instance() {
    if (gblConfig == NULL) {
        gblConfig = new Config();
    }
    return gblConfig;
}

Config::Config() {
    // set default
    verbose = 0;        // Not verbose
    inputs.push_back(".");    // Search current directory
    modules.push_back("-");
    testcases.push_back("-");
    version = "1.2-Dev";
    description = "C/C++ Unit Test Runner";
    mainFuncName = "main";
    exitFuncName = "exit";
    reportingModule = "console";
    reportIndent = 8;
    executeTests = true;
    listTests = false;
    printPassSummary = false;
    testModuleGlobals = true;
    testGlobalMain = true;
    testLogFilter = false;
    skipOnModuleFail = true;
    stopOnAllFail = true;
    suppressProgressMsg = false;
    discardTestReturnCode = false;
    linuxUseDeepBinding = true;
    responseMsgByteLimit = 1024 * 8;
    
    //
    // Setup logger
    //
    int logLevel = Logger::kMCDebug;
	Logger::Initialize();
	if (logLevel != Logger::kMCNone) {
		Logger::AddSink(Logger::CreateSink("LogConsoleSink"), "console", 0, NULL);
	}
	Logger::SetAllSinkDebugLevel(logLevel);
    pLogger = Logger::GetLogger("main");
}

void Config::Dump() {
    printf("Current Configuration\n");
    printf("TestRunner v%s - %s\n", version.c_str(), description.c_str());
    printf("  Verbose....: %s (%d)\n",verbose?"yes":"no", verbose);
    printf("  List Tests.: %s\n", listTests?"yes":"no");
    printf("  Run Tests..: %s\n", executeTests?"yes":"no");
    printf("  Pass in summary: %s\n", printPassSummary?"yes":"no");
    printf("  TestMain...: %s\n", mainFuncName.c_str());
    printf("  Test Module Globals: %s\n", testModuleGlobals ? "yes" : "no");
    printf("  Test Main Global: %s\n", testGlobalMain?"yes":"no");
    printf("  TestCase Log Filter: %s\n", testLogFilter?"yes":"no");
    printf("  Response Message Size Limit: %d\n", responseMsgByteLimit);
    printf("  Skip rest on library failure: %s\n", skipOnModuleFail?"yes":"no");
    printf("  Stop on full failure: %s\n", stopOnAllFail?"yes":"no");
    printf("  Silent mode: %s\n", suppressProgressMsg?"yes":"no");
    printf("  Discard test return code: %s\n", discardTestReturnCode?"yes":"no");
    printf("  Reporting module: %s\n", reportingModule.c_str());
    printf("  Reporting indent size: %d\n", reportIndent);
    printf("  Modules:\n");
    for(auto x:modules) {
        printf("    %s\n", x.c_str());
    }
    printf("  Test cases:\n");
    for(auto x:testcases) {
        printf("    %s\n", x.c_str());
    }
    printf("  Inputs:\n");
    for(auto x:inputs) {
        printf("    %s\n", x.c_str());

    }
    ResultSummary::Instance().ListReportingModules();
    printf("\n");

}