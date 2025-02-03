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
 TO-DO: [ -:Not done, +:In progress, !:Completed]
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

using namespace trun;


Config &Config::Instance() {
    static Config glbConfig;
    return glbConfig;
}

Config::Config() {
    // set default
    inputs.push_back(".");    // Search current directory
    modules.push_back("-");
    testcases.push_back("-");
#ifdef TRUN_VERSION
    version = TRUN_VERSION;
#else
    version = "<unknown>";
#endif
    description = "C/C++ Unit Test Runner";
#ifdef TRUN_HAVE_FORK
    testExecutionType = TestExecutiontype::kThreaded;
    moduleExecuteType = ModuleExecutionType::kParallel;
#else
    testExecutionType = TestExecutiontype::kSequential;
    moduleExecuteType = ModuleExecutionType::kSequential;
#endif

    //
    // Setup logger
    //
    auto logLevel = gnilk::LogLevel::kDebug;
	gnilk::Logger::Initialize();
	//if (logLevel != gnilk::LogLevel::kNone) {
        // Note: Console already added
        //auto consoleSink = gnilk::LogConsoleSink::Create();
        //gnilk::Logger::AddSink(consoleSink, "Console");
		//gnilk::Logger::AddSink(gnilk::Logger::CreateSink("LogConsoleSink"), "console", 0, NULL);
	//}
	gnilk::Logger::SetAllSinkDebugLevel(logLevel);
    pLogger = gnilk::Logger::GetLogger("main");
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
    printf("  Module execution policy: %s\n", ModuleExecutionTypeToStr(moduleExecuteType).c_str());
    printf("  Testcase execution policy: %s\n", TestExecutionTypeToStr(testExecutionType).c_str());
    printf("  Continue on assert: %s\n", continueOnAssert?"yes":"no");
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