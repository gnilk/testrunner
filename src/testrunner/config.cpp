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
#include <string>
#include <stdint.h>
#include <inttypes.h>
#include <functional>
#include <optional>

#include <fmt/format.h>

#include "config.h"
#include "logger.h"
#include "resultsummary.h"

#include "ArgParser.h"

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
    // Test cases always run in their own thread. Module-level parallelism is fork-only
    // (one subprocess per module) and desktop-only; without fork the module loop is sequential.
    testExecutionType = TestExecutiontype::kThreaded;
#ifdef TRUN_HAVE_FORK
    moduleExecuteType = ModuleExecutionType::kParallel;
#else
    moduleExecuteType = ModuleExecutionType::kSequential;
#endif

    dumpConfig = false;

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

static void ParseModuleFilters(const char *filterstring) {
    std::vector<std::string> modules;
    trun::split(modules, filterstring, ',');

    // split() drops empty fields, so an all-separator/whitespace filter ('-m ,,,' or
    // '-m "  "') yields an empty list. Overwriting the '-' match-all default with that
    // filtered out every module and ran nothing, silently. Keep the current filter and
    // tell the user instead.
    if (modules.empty()) {
        fprintf(stderr, "Warning: module filter '%s' has no usable entries - ignored (keeping current filter)\n", filterstring);
        return;
    }

    Config::Instance().modules = modules;
}
static void ParseTestCaseFilters(const char *filterstring) {
    std::vector<std::string> testcases;
    trun::split(testcases, filterstring, ',');
    if (testcases.empty()) {
        fprintf(stderr, "Warning: test-case filter '%s' has no usable entries - ignored (keeping current filter)\n", filterstring);
        return;
    }
    Config::Instance().testcases = testcases;
}

/*
static void ConfigureLogger() {
    // Setup up logger according to verbose flags
    gnilk::Logger::SetAllSinkDebugLevel(gnilk::LogLevel::kError);
    if (Config::Instance().verbose > 0) {
        gnilk::Logger::SetAllSinkDebugLevel(gnilk::LogLevel::kInfo);
        if (Config::Instance().verbose > 1) {
            gnilk::Logger::SetAllSinkDebugLevel(gnilk::LogLevel::kDebug);
        }
    }
}
*/

// Argument parsing using ArgParser. The embedded front-end doesn't call this (it sets
// Config directly via RunTests()), but it compiles into every target all the same.
Config::FromArgRes Config::FromArguments(int argc, const char **argv) {
    ArgParser argParser(argc, argv);
    Config::Instance().appName = argv[0];
    if (argParser.IsPresent("-hH?","--help")) {
        return FromArgRes::kHelp;
    }

    // Support both spellings; the underscore form was renamed to the dashed one in 3.0.2.
    bool deprecatedContinueOnAssert = argParser.IsPresent("","--continue_on_assert");
    if (deprecatedContinueOnAssert) {
        fmt::println(stderr, "Warning: --continue_on_assert is deprecated, use --continue-on-assert instead");
    }
    Config::Instance().continueOnAssert = argParser.IsPresent("","--continue-on-assert") || deprecatedContinueOnAssert;
    Config::Instance().discardTestReturnCode = argParser.IsPresent("-r");
    Config::Instance().dumpConfig = argParser.IsPresent("-d");
    Config::Instance().executeTests = !argParser.IsPresent("-x");
    Config::Instance().linuxUseDeepBinding = !argParser.IsPresent("-D");
    Config::Instance().listTests = argParser.IsPresent("-l");
    Config::Instance().printPassSummary = argParser.IsPresent("-S");
    Config::Instance().skipOnModuleFail = !argParser.IsPresent("-c");
    Config::Instance().stopOnAllFail = !argParser.IsPresent("-C");
    Config::Instance().testModuleGlobals = !argParser.IsPresent("-g");
    Config::Instance().testGlobalMain = !argParser.IsPresent("-G");

    if (argParser.IsPresent("-s")) {
        Config::Instance().testLogFilter = true;
        Config::Instance().suppressProgressMsg = true;
    }
    Config::Instance().verbose = argParser.CountPresence("-v","--verbose");

    Config::Instance().reportingModule = *argParser.TryParse(Config::Instance().reportingModule,"-R","");
    Config::Instance().reportFile = *argParser.TryParse(Config::Instance().reportFile,"-O","");

    if (argParser.IsPresent("", "--version")) {
        return Config::FromArgRes::kVersion;
    }


    if (argParser.IsPresent("-t")) {
        std::string testCases;
        testCases = *argParser.TryParse(testCases, "-t");
        if (!testCases.empty()) {
            ParseTestCaseFilters(testCases.c_str());
        }
    }
    if (argParser.IsPresent("-m")) {
        std::string testModules;
        testModules = *argParser.TryParse(testModules, "-m");
        if (!testModules.empty()) {
            ParseModuleFilters(testModules.c_str());
        }
    }

    // special stuff

    if (argParser.IsPresent("", "--sequential")) {
        Config::Instance().moduleExecuteType = trun::ModuleExecutionType::kSequential;
    }
    if (argParser.IsPresent("", "--allow-thread-exit")) {
        Config::Instance().testExecutionType = trun::TestExecutiontype::kThreadedWithExit;
    }
#ifdef TRUN_HAVE_FORK
    Config::Instance().moduleExecTimeoutSec = *argParser.TryParse(Config::Instance().moduleExecTimeoutSec, "", "--module-timeout");
    Config::Instance().moduleExecConcurrency = *argParser.TryParse(Config::Instance().moduleExecConcurrency, "", "--max-concurrency");
    Config::Instance().ipcName = *argParser.TryParse(Config::Instance().ipcName, "", "--ipc-name");
#endif

    // Hidden
    if (argParser.IsPresent("", "--subprocess")) {
        // HIDDEN (only used internally) - We are started by another trun process
        Config::Instance().isSubProcess = true;
    }
    Config::Instance().isCoverageRunning = argParser.IsPresent("", "--coverage");
    Config::Instance().coverageIPCName = *argParser.TryParse(Config::Instance().coverageIPCName, "", "--tcov-ipc-name");


    if (argParser.CopyEndArgs(Config::Instance().inputs, false) < 0) {
        fmt::println(stderr, "Error: Failed to parse arguments");
        return Config::FromArgRes::kError;
    }

    return Config::FromArgRes::kSuccess;
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
    printf("  Response Message Size Limit: %" PRIu32 "\n", responseMsgByteLimit);
    printf("  Skip rest on library failure: %s\n", skipOnModuleFail?"yes":"no");
    printf("  Stop on full failure: %s\n", stopOnAllFail?"yes":"no");
    printf("  Silent mode: %s\n", suppressProgressMsg?"yes":"no");
    printf("  Discard test return code: %s\n", discardTestReturnCode?"yes":"no");
    printf("  Reporting module: %s\n", reportingModule.c_str());
    printf("  Reporting indent size: %d\n", reportIndent);
    printf("  Module execution policy: %s\n", ModuleExecutionTypeToStr(moduleExecuteType).c_str());
#ifdef TRUN_HAVE_FORK
    printf("  Module exec timeout (sec): %d\n", moduleExecTimeoutSec);
    printf("  Module exec concurrency: %d (0 = auto)\n", moduleExecConcurrency);
#endif
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
