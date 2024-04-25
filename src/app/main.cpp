/*-------------------------------------------------------------------------
 File    : main.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : C/C++ Unit Test Runner

 This is the testrunner for the UnitTest 'framework'.
 Heavily inspired by GOLANG's testing framework.

 Default execution is scanning all dynamic libraries starting from the current directory and going down
 the directory tree.
 Any exported function with name 'test_' will be considered a test function.

 The following logic applies to how names are mapped.
 test_main  - reserved, if present it is the first function called, use for global context setup
 test_exit  - reserved, if present it is the last function called, global tear down

 test_<library>      - the main for a testable library (a library can be seen as a group of function)
                      library main is called before any test case in the library/group
 test_<library>_exit - the exit for a testable library/group. called after all tests have been executed
 test_<library>_<case>   - a regular test case


 All code is BSD3 License!
 
 ---------------------------------------------------------------------------*/
#ifdef WIN32
#include <Windows.h>
#include <io.h>
// ok, the windows console handling is quite horrible if compared to macOS/Linux...
// this ought to solve it for my purposes
#ifndef STDOUT_FILENO
    #define STDOUT_FILENO 1
#endif
#else
#include <unistd.h>
#endif

#include "logger.h"
#include "testrunner.h"
#include "dynlib.h"



#ifdef WIN32
#include "win32/dynlib_win32.h"
#elif __linux
#include "unix/dynlib_unix.h"
#else
#include "unix/dynlib_unix.h"
#endif
#include "strutil.h"
#include "config.h"
#include "timer.h"
#include "dirscanner.h"
#include "resultsummary.h"

#include <iostream>
#include <string>
#include <regex>
#include <functional>


using namespace trun;
gnilk::ILogger *pLogger = nullptr;

static bool isLibraryFound = false;

static void Help() {

#ifdef _WIN64
    std::string strPlatform = "Windows x64 (64 bit)";
#elif WIN32
    std::string strPlatform = "Windows x86 (32 bit)";
#elif __linux
    std::string strPlatform = "Linux";
#else
    std::string strPlatform = "macOS";
#endif

    const char *platform = strPlatform.c_str();

    printf("TestRunner v%s - %s - %s\n", 
        Config::Instance().version.c_str(),
        platform,
        Config::Instance().description.c_str());

    printf("Usage: trun [options] input\n");
    printf("Options: \n");
    printf("  -v  Verbose, increase for more!\n");
    printf("  -l  List all available tests\n");
    printf("  -d  Dump configuration before starting\n");
    printf("  -S  Include success pass in summary when done (default: off)\n");
    printf("  -D  Linux Only - disable RTLD_DEEPBIND\n");
    printf("  -g  Skip library globals (default: off)\n");
    printf("  -G  Skip global main (default: off)\n");
    printf("  -s  Silent, surpress messages from test cases (default: off)\n");
    printf("  -r  Discard return from test case (default: off)\n");
    printf("  -c  Continue on library failure (default: off)\n");
    printf("  -C  Continue on total failure (default: off)\n");
    printf("  -x  Don't execute tests (default: off)\n");
    printf("  -R  <name> Use reporting library (default: console)\n");
    printf("  -O  <file> Report final result to this file, use '-' for stdout (default: -)\n");
    printf("  -m  <list> List of modules to test (default: '-' (all))\n");
    printf("  -t  <list> List of test cases to test (default: '-' (all))\n");
    printf("  --no-threads\n");
    printf("      Disable threaded execution of tests\n");
    printf("\n");
    printf("Input should be a directory or list of dylib's to be tested, default is current directory ('.')\n");
    printf("Module and test case list can use wild cards, like: -m encode -t json*\n");
    printf("\n");
}
static void ParseModuleFilters(char *filterstring) {
    std::vector<std::string> modules;
    trun::split(modules, filterstring, ',');

    // for(auto m:modules) {
    //     pLogger->Debug("  %s\n", m.c_str());
    // }

    Config::Instance().modules = modules;
}
static void ParseTestCaseFilters(char *filterstring) {
    std::vector<std::string> testcases;
    trun::split(testcases, filterstring, ',');
    Config::Instance().testcases = testcases;
}

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

// Returns false if we should leave the program directly, true if we are to continue
static bool ParseArguments(int argc, char **argv) {

    bool firstInput = true;
    bool dumpConfig = false;

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
                        dumpConfig = true;
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
                            if (longArgument == "no-threads") {
                                Config::Instance().enableThreadTestExecution = false;
                                goto next_argument;
                            } else if (longArgument == "parallel") {
                                Config::Instance().enableParallelTestExecution = true;
                                printf("WARNING - enabling parallel execution - ONLY for development!!!\n");
                                goto next_argument;
                            }
                            printf("Unknown long argument: %s\n", longArgument.c_str());
                            Help();
                            exit(1);
                        }
                        break;
                    case '?' :
                    case 'h' :
                    case 'H' :
                        Help();
                        exit(1);
                        break;
                    default:
                        printf("Unknown option: %c\n", argv[i][j]);
                        Help();
                        exit(1);

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
    ConfigureLogger();

    bool bContinue = true;
    if (dumpConfig || (Config::Instance().verbose > 1)) {
        Config::Instance().Dump();
        if (dumpConfig) {
            bContinue = false;
        }
    }
    // Special case here - if we specify list as the reporting library we just dump them and leave
    if (Config::Instance().reportingModule == "list") {
        ResultSummary::Instance().ListReportingModules();
        bContinue = false;
    }
    return bContinue;
}

static IDynLibrary::Ref GetLibraryLoader() {
#ifdef WIN32
    return DynLibWin::Create();
#elif __linux
    return DynLibLinux::Create();
#else
    return DynLibLinux::Create();
#endif
    return {};
}

static void RunTestsForAllLibraries();
static void RunTestsForLibrary(IDynLibrary::Ref library);

// Consider refactoring this away from here - global variables and reference counting stuff doesn't play nice.
// Populated by ScanLibraries
static std::vector<IDynLibrary::Ref> librariesToTest;


static void ScanLibraries(std::vector<std::string> &inputs) {
    // Process all inputs
    for (auto x: inputs) {
        if (DirScanner::IsDirectory(x)) {
            DirScanner dirscan;
            std::vector<std::string> subs = dirscan.Scan(x, true);
            ScanLibraries(subs);
        } else {
            IDynLibrary::Ref scanner = GetLibraryLoader();
            if (scanner == nullptr) {
                pLogger->Error("No library loader/scanner - unsupported platform?");
                exit(1);
            }

            auto res = scanner->Scan(x);
            if (res) {
                if (scanner->Exports().size() > 0) {

                    isLibraryFound = true;   // we found at least one library..
                    //pLogger->Info("Executing tests for %s", x.c_str());
                    librariesToTest.push_back(scanner);
                }
            } else {
                pLogger->Error("Scan failed on '%s'", x.c_str());
            }
        }
    }
}
static void RunTestsForAllLibraries() {
    pLogger->Info("Running tests for all modules");
    for(auto lib : librariesToTest) {
        RunTestsForLibrary(lib);
    }
}

static void RunTestsForLibrary(IDynLibrary::Ref library) {
    TestRunner testRunner(library);
    testRunner.PrepareTests();

    if (Config::Instance().executeTests) {
        pLogger->Debug("Running tests for: %s", library->Name().c_str());
        testRunner.ExecuteTests();
    }
}
static void DumpTestsForLibrary(IDynLibrary::Ref library) {
    TestRunner testRunner(library);
    testRunner.PrepareTests();
        printf("=== Library: %s\n", library->Name().c_str());
    testRunner.DumpTestsToRun();
}

static void DumpTestsForAllLibraries() {
    for(auto m : librariesToTest) {
        DumpTestsForLibrary(m);
    }
}

int main(int argc, char **argv) {
    // Cache the logger - also creates the Config singleton with default values
    pLogger = Config::Instance().pLogger;
    if (!ParseArguments(argc, argv)) {
        return 0;
    }

#ifdef _WIN64
	pLogger->Info("Windows x64 (64 bit) build");
#elif WIN32
	pLogger->Info("Windows x86 (32 bit) build");
#endif
    // Suppressing messages? - kill stdout but save it for later...
    int saveStdout = -1;
    if (Config::Instance().suppressProgressMsg) {
        saveStdout = dup(STDOUT_FILENO);
        // Note: We don't really care about the result here, at least not for now
#ifdef WIN32
        freopen("nul", "w+", stdout);
#else
        if (freopen("/dev/null", "w+", stdout) == nullptr) {
            fprintf(stderr, "ERR: Unable to open /dev/null\n");
            return -1;
        }
#endif
    }

    Timer timer;
    
    timer.Reset();
    ScanLibraries(Config::Instance().inputs);

    if (Config::Instance().listTests) {
        DumpTestsForAllLibraries();
    }

    printf("--> Start Global\n");
    RunTestsForAllLibraries();
    printf("<-- End Global\n");

    // Restore stdout - in order for reporting to work...
    if (Config::Instance().suppressProgressMsg) {
        // On macOS/Linux we need to flush what-ever is in the cache before we restore/duplicate the stdout
        // otherwise we might have the console data written once a proper file-desc is attached to stdout
        fflush(stdout);
        if (dup2(saveStdout, STDOUT_FILENO) < 0) {
            fprintf(stderr, "Unable to restore stdout: %d\n", errno);
            librariesToTest.clear();
            return 0;
        }
    }

    // Reporting
    ResultSummary::Instance().durationSec = timer.Sample();
    if (Config::Instance().executeTests) {
        // This should probably go away - as we want full reporting in headless mode...
        if (ResultSummary::Instance().testsExecuted > 0) {
            ResultSummary::Instance().PrintSummary();
        } else {
            // This should be made available in the report - in case we are running headless we want this in the JSON
            // output...
            if (!isLibraryFound) {
                printf("No dynamic library with testable modules/functions found!\n");
            } else {
                printf("Testable modules/functions found but no tests executed (check filters)\n");
            }
        }
    }

    // Need to clear this in order to invoke DTOR on scanner as an instance is kept from 'scanlibraries'
    // Would not have been required if the App would have been contained in a class...  anyway...
    librariesToTest.clear();
    return 0;
}