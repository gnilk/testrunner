/*-------------------------------------------------------------------------
 File    : main.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : C/C++ Unit Test Runner

 This is the testrunner for the UnitTest 'framework'. 
 Heavily inspired by GOLANG's testing framework.

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
#ifdef WIN32
#include "module_win32.h"
#elif __linux
#include "module_linux.h"
#else
#include "dynlib_linux.h"
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
    printf("  -d  Dump configuration before starting\n");
    printf("  -S  Include success pass in summary when done (default: off)\n");
    printf("  -D  Linux Only - disable RTLD_DEEPBIND\n");
    printf("  -g  Skip module globals (default: off)\n");
    printf("  -G  Skip global main (default: off)\n");
    printf("  -s  Silent, surpress messages from test cases (default: off)\n");
    printf("  -r  Discard return from test case (default: off)\n");
    printf("  -c  Continue on module failure (default: off)\n");
    printf("  -C  Continue on total failure (default: off)\n");
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

static void ParseArguments(int argc, char **argv) {

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
                        break;
                    case 'g' :
                        Config::Instance()->testGlobals = false;
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

    // Setup up logger according to verbose flags
    Logger::SetAllSinkDebugLevel(Logger::kMCError);
    if (Config::Instance()->verbose > 0) {
	    Logger::SetAllSinkDebugLevel(Logger::kMCInfo);
        if (Config::Instance()->verbose > 1) {
	        Logger::SetAllSinkDebugLevel(Logger::kMCDebug);
        }
    }

    if (dumpConfig) {
        Config::Instance()->Dump();
    }
}




static IDynLibrary *GetLibraryLoader() {
#ifdef WIN32
    return new ModuleWin();
#elif __linux
    return new DynLibLinux();
#else
    return new DynLibLinux();
#endif
    return nullptr;
}

static void RunTestsForAllLibraries();
static void RunTestsForLibrary(IDynLibrary &module);

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
    pLogger->Debug("Running tests for");
    ModuleTestRunner testRunner(&module);
    testRunner.PrepareTests();

    testRunner.DumpTestsToRun();
    testRunner.ExecuteTests();
}


int main(int argc, char **argv) {
    // Cache the logger - also creates the Config singleton with default values
    pLogger = Config::Instance()->pLogger;
    ParseArguments(argc, argv);

#ifdef _WIN64
	pLogger->Info("Windows x64 (64 bit) build");
#elif WIN32
	pLogger->Info("Windows x86 (32 bit) build");
#endif

    Timer timer;
    
    timer.Reset();
    ScanLibraries(Config::Instance()->inputs);

    RunTestsForAllLibraries();

    double tSeconds = timer.Sample();

    if (ResultSummary::Instance().testsExecuted > 0) {
        printf("-------------------\n");
        printf("Duration......: %.3f sec\n", tSeconds);
        printf("Tests Executed: %d\n", ResultSummary::Instance().testsExecuted);
        printf("Tests Failed..: %d\n", ResultSummary::Instance().testsFailed);

        ResultSummary::Instance().PrintSummary(Config::Instance()->printPassSummary);
    } else {
        if (!isLibraryFound) {
            printf("No dynamic library with testable modules/functions found!\n");
        } else {
            printf("Testable modules/functions found but no tests executed (check filters)\n");
        }
    }


    return 0;
}