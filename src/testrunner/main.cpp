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

 test_<module>      - the main for a testable module (a module can be seen as a group of function)
                      module main is called before any test case in the module/group
 test_<module>_exit - the exit for a testable module/group. called after all tests have been executed
 test_<module>_<case>   - a regular test case


 All code is BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.12.21, FKling, Support for test case specification and skipping of test_main
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/
#ifdef WIN32
#include <Windows.h>
#endif

#include "logger.h"
#include "testrunner.h"
#include "dynlib.h"
#include <unistd.h>

#ifdef WIN32
#include "dynlib_win32.h"
#elif __linux
#include "dynlib_unix.h"
#else
#include "dynlib_unix.h"
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


using namespace gnilk;
ILogger *pLogger = NULL;

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
        Config::Instance()->version.c_str(),
        platform,
        Config::Instance()->description.c_str());

    printf("Usage: trun [options] input\n");
    printf("Options: \n");
    printf("  -v  Verbose, increase for more!\n");
    printf("  -l  List all available tests\n");
    printf("  -d  Dump configuration before starting\n");
    printf("  -S  Include success pass in summary when done (default: off)\n");
    printf("  -D  Linux Only - disable RTLD_DEEPBIND\n");
    printf("  -g  Skip module globals (default: off)\n");
    printf("  -G  Skip global main (default: off)\n");
    printf("  -s  Silent, surpress messages from test cases (default: off)\n");
    printf("  -r  Discard return from test case (default: off)\n");
    printf("  -c  Continue on module failure (default: off)\n");
    printf("  -C  Continue on total failure (default: off)\n");
    printf("  -x  Don't execute tests (default: off)\n");
    printf("  -R  <name> Use reporting module (default: console)\n");
    printf("  -m <list> List of modules to test (default: '-' (all))\n");
    printf("  -t <list> List of test cases to test (default: '-' (all))\n");
    printf("\n");
    printf("Input should be a directory or list of dylib's to be tested, default is current directory ('.')\n");
    printf("\n");
}
static void ParseModuleFilters(char *filterstring) {
    std::vector<std::string> modules;
    strutil::split(modules, filterstring, ',');

    // for(auto m:modules) {
    //     pLogger->Debug("  %s\n", m.c_str());
    // }

    Config::Instance()->modules = modules;
}
static void ParseTestCaseFilters(char *filterstring) {
    std::vector<std::string> testcases;
    strutil::split(testcases, filterstring, ',');
    Config::Instance()->testcases = testcases;
}

static void ConfigureLogger() {
    // Setup up logger according to verbose flags
    Logger::SetAllSinkDebugLevel(Logger::kMCError);
    if (Config::Instance()->verbose > 0) {
        Logger::SetAllSinkDebugLevel(Logger::kMCInfo);
        if (Config::Instance()->verbose > 1) {
            Logger::SetAllSinkDebugLevel(Logger::kMCDebug);
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
                        Config::Instance()->discardTestReturnCode = true;
                        break;
                    case 'l' :
                        Config::Instance()->listTests = true;
                        break;
                    case 'x' :
                        Config::Instance()->executeTests = false;
                        break;
                    case 'S' :
                        Config::Instance()->printPassSummary = true;
                        break;
                    case 'c' :
                        Config::Instance()->skipOnModuleFail = false;
                        break;
                    case 'C' :
                        Config::Instance()->stopOnAllFail = false;
                        break;
                    case 'd' :
                        dumpConfig = true;
                        break;
                    case 'D' :
                        Config::Instance()->linuxUseDeepBinding = false;
                        break;
                    case 's' :
                        Config::Instance()->testLogFilter = true;
                        Config::Instance()->suppressProgressMsg = true;
                        break;
                    case 'g' :
                        Config::Instance()->testModuleGlobals = false;
                        break;
                    case 'G' :
                        Config::Instance()->testGlobalMain = false;
                        break;
                    case 't' :
                        ParseTestCaseFilters(argv[++i]);
                        goto next_argument;
                    case 'm' :
                        // Parse module filter
                        ParseModuleFilters(argv[++i]);                        
                        goto next_argument;
                    case 'v' :
                        Config::Instance()->verbose++;
                        break;
                    case 'R' :
                        Config::Instance()->reportingModule = std::string(argv[++i]);
                        goto next_argument;
                        break;
                    default:
                        printf("Unknown option: %c\n", argv[i][j]);
                    case '?' :
                    case 'h' :
                    case 'H' :
                        Help();
                        exit(1);
                        break;
                }
                j++;
            }
        } else {
            if (firstInput) {
                Config::Instance()->inputs.clear();
                firstInput = false;
            }
            Config::Instance()->inputs.push_back(argv[i]);
        }
// a bit ugly but does the trick in this case        
next_argument:;
    }
    ConfigureLogger();

    bool bContinue = true;
    if (dumpConfig || (Config::Instance()->verbose > 1)) {
        Config::Instance()->Dump();
        bContinue = false;
    }
    // Special case here - if we specify list as the reporting module we just dump them and leave
    if (Config::Instance()->reportingModule == "list") {
        ResultSummary::Instance().ListReportingModules();
        bContinue = false;
    }
    return bContinue;
}




static IDynLibrary *GetLibraryLoader() {
#ifdef WIN32
    return new DynLibWin();
#elif __linux
    return new DynLibLinux();
#else
    return new DynLibLinux();
#endif
    return nullptr;
}

static void RunTestsForAllLibraries();
static void RunTestsForLibrary(IDynLibrary &module);

// Populated by ScanLibraries
static std::vector<IDynLibrary *> librariesToTest;


static void ScanLibraries(std::vector<std::string> &inputs) {
    // Process all inputs
    for (auto x: inputs) {
        if (DirScanner::IsDirectory(x)) {
            DirScanner dirscan;
            std::vector<std::string> subs = dirscan.Scan(x, true);
            ScanLibraries(subs);
        } else {
            IDynLibrary *scanner = GetLibraryLoader();

            auto res = scanner->Scan(x);
            if (res) {
                if (scanner->Exports().size() > 0) {

                    isLibraryFound = true;   // we found at least one module..
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
    for(auto m : librariesToTest) {
        RunTestsForLibrary(*m);
    }
}

static void RunTestsForLibrary(IDynLibrary &module) {
    TestRunner testRunner(&module);
    testRunner.PrepareTests();

    if (Config::Instance()->executeTests) {
        pLogger->Debug("Running tests for: %s", module.Name().c_str());
        testRunner.ExecuteTests();
    }
}
static void DumpTestsForLibrary(IDynLibrary &module) {
    TestRunner testRunner(&module);
    testRunner.PrepareTests();
        printf("=== Library: %s\n", module.Name().c_str());
    testRunner.DumpTestsToRun();
}

static void DumpTestsForAllLibraries() {
    for(auto m : librariesToTest) {
        DumpTestsForLibrary(*m);
    }
}

int main(int argc, char **argv) {
    // Cache the logger - also creates the Config singleton with default values
    pLogger = Config::Instance()->pLogger;
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
    if (Config::Instance()->suppressProgressMsg) {
        saveStdout = dup(STDOUT_FILENO);
        // Note: We don't really care about the result here, at least not for now
        freopen("/dev/null", "w+", stdout);
    }


    Timer timer;
    
    timer.Reset();
    ScanLibraries(Config::Instance()->inputs);

    if (Config::Instance()->listTests) {
        DumpTestsForAllLibraries();
    }

    printf("--> Start Global\n");
    RunTestsForAllLibraries();
    printf("<-- End Global\n");

    // Restore stdout - in order for reporting to work...
    if (Config::Instance()->suppressProgressMsg) {
        if ((stdout = fdopen(saveStdout, "w")) == nullptr) {
            fprintf(stderr, "Unable to restore stdout: %d\n", errno);
            return 0;
        }
    }

    // Reporting
    ResultSummary::Instance().durationSec = timer.Sample();
    if (Config::Instance()->executeTests) {
        if (ResultSummary::Instance().testsExecuted > 0) {
            ResultSummary::Instance().PrintSummary(Config::Instance()->printPassSummary);
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


    return 0;
}