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

#include "config.h"
#include "logger.h"
#include "resultsummary.h"

#ifndef TRUN_EMBEDDED
#include "ArgParser.h"
#endif

using namespace trun;

#if defined(TRUN_HAVE_FORK) && defined(TRUN_EMBEDDED)
static std::optional<uint64_t> ParseNumber(const std::string_view &line);
#endif

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

    // for(auto m:modules) {
    //     pLogger->Debug("  %s\n", m.c_str());
    // }

    Config::Instance().modules = modules;
}
static void ParseTestCaseFilters(const char *filterstring) {
    std::vector<std::string> testcases;
    trun::split(testcases, filterstring, ',');
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

// In case we are running on embedded, actually I don't think we use this on embedded - at all
#ifdef TRUN_EMBEDDED
static Config::FromArgRes old_Config_FromArguments(int argc, char **argv);
#endif



// New argument parsing using ArgParser
Config::FromArgRes Config::FromArguments(int argc, const char **argv) {
#ifdef TRUN_EMBEDDED
    //return old_Config_FromArguments(argc, const_cast<char**>(argv));
    return Config::FromArgRes::kError;
#else
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
#endif
}
// Returns false if we should leave the program directly, true if we are to continue
#ifdef TRUN_EMBEDDED
static Config::FromArgRes old_Config_FromArguments(int argc, char **argv) {
    bool firstInput = true;

    auto result = Config::FromArgRes::kSuccess;

    Config::Instance().appName = argv[0];

    for (int i=1;i<argc;i++) {
        if (argv[i][0]=='-') {
            // parse options
            int j=1;
            while((argv[i][j]!='\0')) {
                switch(argv[i][j]) {
                    case 'r' :
                        Config::Instance().discardTestReturnCode = true;
                        break;
                    case 'l' :
                        Config::Instance().listTests = true;
                        break;
                    case 'x' :
                        Config::Instance().executeTests = false;
                        break;
                    case 'S' :
                        Config::Instance().printPassSummary = true;
                        break;
                    case 'c' :
                        Config::Instance().skipOnModuleFail = false;
                        break;
                    case 'C' :
                        Config::Instance().stopOnAllFail = false;
                        break;
                    case 'd' :
                        Config::Instance().dumpConfig = true;
                        break;
                    case 'D' :
                        Config::Instance().linuxUseDeepBinding = false;
                        break;
                    case 's' :
                        Config::Instance().testLogFilter = true;
                        Config::Instance().suppressProgressMsg = true;
                        break;
                    case 'g' :
                        Config::Instance().testModuleGlobals = false;
                        break;
                    case 'G' :
                        Config::Instance().testGlobalMain = false;
                        break;
                    case 't' :
                        ParseTestCaseFilters(argv[++i]);
                        goto next_argument;
                    case 'm' :
                        // Parse library filter
                        ParseModuleFilters(argv[++i]);
                        goto next_argument;
                    case 'v' :
                        Config::Instance().verbose++;
                        break;
                    case 'R' :
                        Config::Instance().reportingModule = std::string(argv[++i]);
                        goto next_argument;
                        break;
                    case 'O' :
                        Config::Instance().reportFile = std::string(argv[++i]);
                        goto next_argument;
                        break;
                    case '-' :
                        // Long argument
                    {
                        std::string longArgument = std::string(&argv[i][++j]);
                        if (longArgument == "version") {
                            return Config::FromArgRes::kVersion;
                        }
                        if (longArgument == "continue_on_assert") {
                            Config::Instance().continueOnAssert = true;
                            goto next_argument;
                        } else if (longArgument == "sequential") {
                            Config::Instance().moduleExecuteType = trun::ModuleExecutionType::kSequential;
                            goto next_argument;
                        } else if (longArgument == "allow-thread-exit") {
                            Config::Instance().testExecutionType = trun::TestExecutiontype::kThreadedWithExit;
                            goto next_argument;
                        } else if (longArgument == "module-timeout") {
#ifdef TRUN_HAVE_FORK
                            auto optNum = ParseNumber(argv[++i]);
                            if (!optNum.has_value()) {
                                fmt::println(stderr, "module-timeout, '{}' not a number", argv[i]);
                                return Config::FromArgRes::kHelp;
                            }
                            Config::Instance().moduleExecTimeoutSec = (uint16_t)optNum.value();
#else
                            fprintf(stderr,"module-timeout only available when compiled with 'TRUN_HAVE_FORK'\n");
#endif
                            goto next_argument;
                        } else if (longArgument == "subprocess") {
                            // HIDDEN (only used internally) - We are started by another trun process
                            Config::Instance().isSubProcess = true;
                            goto next_argument;
                        } else if (longArgument == "ipc-name") {
                            // HIDDEN (only used internally) - this is the IPC name we should when in a subprocess
#ifdef TRUN_HAVE_FORK
                            Config::Instance().ipcName = argv[++i];
#else
                            fprintf(stderr, "ipc-name only available when compiled with 'TRUN_HAVE_FORK'\n");
#endif
                            goto next_argument;
                        } else if (longArgument == "coverage") {
                            // HIDDEN - we are started from 'tcov' and coverage tracking is enabled
                            Config::Instance().isCoverageRunning = true;
                            goto next_argument;
                        } else if (longArgument == "tcov-ipc-name") {
                            Config::Instance().coverageIPCName = argv[++i];
                            goto next_argument;;
                        }
                        printf("Unknown long argument: %s\n", longArgument.c_str());
                        return Config::FromArgRes::kHelp;
                    }
                        break;
                    case '?' :
                    case 'h' :
                    case 'H' :
                        return Config::FromArgRes::kHelp;
                        break;
                    default:
                        return Config::FromArgRes::kHelp;
                        break;

                }
                j++;
            }
        } else {
            if (firstInput) {
                Config::Instance().inputs.clear();
                firstInput = false;
            }
            Config::Instance().inputs.push_back(argv[i]);
        }
        // a bit ugly but does the trick in this case
        next_argument:;
    }
    return Config::FromArgRes::kSuccess;
}
#endif


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

//////////////
///// Helper
#if defined(TRUN_HAVE_FORK) && defined(TRUN_EMBEDDED)
static std::optional<uint64_t> ParseNumber(const std::string_view &line) {

    std::string num;
    auto it = line.begin();

    std::function<bool(const int chr)> isnumber = [](const int chr) -> bool {
        return std::isdigit(chr);
    };

    //
    // We could enhance this with more features normally found in assemblers
    // $<hex> - for address
    // #$<hex> - alt. syntax for hex numbers
    // #<dec>  - alt. syntax for dec numbers
    //
    // '#' is a common denominator for numerical values
    if (*it == '#') {
        ++it;
    }

    enum class TNum {
        Number,
        NumberHex,
        NumberBinary,
        NumberOctal,
    };

    auto numberType = TNum::Number;
    if (*it == '0') {
        num += *it;
        it++;
        // Convert number here or during parsing???
        switch(tolower(*it)) {
            case 'x' : // hex
                num += *it;
                ++it;
                numberType = TNum::NumberHex;
                isnumber = [](const int chr) -> bool {
                    static std::string hexnum = {"abcdef"};
                    return (std::isdigit(chr) || (hexnum.find(tolower(chr)) != std::string::npos));
                };
                break;
            case 'b' : // binary
                num += *it;
                ++it;
                numberType = TNum::NumberBinary;
                isnumber = [](const int chr) -> bool {
                    return (chr=='1' || chr=='0');
                };

                break;
            case 'o' : // octal
                num += *it;
                ++it;
                numberType = TNum::NumberOctal;
                isnumber = [](const int chr) -> bool {
                    static std::string hexnum = {"01234567"};
                    return (hexnum.find(tolower(chr)) != std::string::npos);
                };
                break;
            default :
                if (std::isdigit(*it)) {
                    fprintf(stderr,"WARNING: Numerical tokens shouldn't start with zero!");
                }
                break;
        }
    }

    while(it != line.end() && isnumber(*it)) {
        num += *it;
        ++it;
    }
    if (numberType == TNum::Number) {
        return {trun::to_int32(num)};
    } else if (numberType == TNum::NumberHex) {
        return {uint64_t(trun::hex2dec(num))};
    }

    return {};
}
#endif