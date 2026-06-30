/*-------------------------------------------------------------------------
 File    : trun.cpp
 Author  : FKling
 Version : -
 Original: 2018-10-18
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
// Disable warning of POSIX deprecated stuff - level 3 => error, and we don't want this..
#pragma warning(disable : 4996)
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
#include "../../shared/dynlib.h"



#ifdef WIN32
#include "win32/dynlib_win32.h"
#elif __linux
#include "unix/dynlib_unix.h"
#else
#include "unix/dynlib_unix.h"
#endif
#include "../../shared/strutil.h"
#include "config.h"
#include "../../shared/timer.h"
#include "../../shared/dirscanner.h"
#include "resultsummary.h"

#include <iostream>
#include <string>
#include <regex>
#include <functional>
#include <optional>


// FIXME: TEMP for tcov dev
#include <signal.h>

using namespace trun;
gnilk::ILogger *pLogger = nullptr;

// -v --sequential -m datetime /home/gnilk/src/work/embedded/libraries/PuckoNew/cmake-build-debug/lib/libpucko_utests.so
//  -vv --sequential -m !pafs,wavwriter /home/gnilk/src/work/embedded/libraries/PuckoNew/cmake-build-debug/lib/libpucko_utests.so
// -vv --sequential -m exception ./lib/libtrun_utests.so

static bool isLibraryFound = false;
static void PrintSummaryIfNeeded();

static void PrintVersion() {
    printf("TestRunner v%s\n",Config::Instance().version.c_str());
}

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
    printf("  -G  Skip global main (default: off)\n");
    printf("  -s  Silent, surpress messages from test cases (default: off)\n");
    printf("  -r  Discard return from test case (default: off)\n");
    printf("  -c  Continue on module failure - kTR_FailModule - (default: off)\n");
    printf("  -C  Continue on total failure - kTR_FailAll -  (default: off)\n");
    printf("  -x  Don't execute tests (default: off)\n");
    printf("  -R  <name> Use reporting library (default: console)\n");
    printf("  -O  <file> Report final result to this file, use '-' for stdout (default: -)\n");
    printf("  -m  <list> List of modules to test (default: '-' (all))\n");
    printf("  -t  <list> List of test cases to test (default: '-' (all))\n");
    printf("  --sequential\n");
    printf("      Disable parallel execution (use this if you debug through trun, default: off)\n");
    printf("  --continue-on-assert\n");
    printf("      Continue test execution on assert errors (default: off)\n");
#ifdef TRUN_HAVE_FORK    
    printf("  --module-timeout <sec>\n");
    printf("      Set timeout (in seconds) for forked execution, 0 - infinity (default: 30)\n");
    printf("  --max-concurrency <n>\n");
    printf("      Max module subprocesses running at once (0 = auto, ~CPU cores)\n");
#endif
    printf("  --allow-thread-exit\n");
    printf("      Test cases execution thread will self-terminate on assert/error/fatal\n");

    printf("\n");
    printf("Input should be a directory or list of dylib's to be tested, default is current directory ('.')\n");
    printf("Module and test case list can use wild cards, like: -m encode -t json*\n");
    printf("\n");
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

static bool ParseArguments(int argc, const char **argv) {

    auto res = Config::Instance().FromArguments(argc, argv);
    if (res == Config::FromArgRes::kVersion) {
        PrintVersion();
        exit(0);
    }
    if (res == Config::FromArgRes::kHelp) {
        Help();
        exit(0);
    }
    if (res == Config::FromArgRes::kError) {
        exit(1);
    }

    // Remove from 'ParseArguments'
    ConfigureLogger();


    bool bContinue = true;
    if (Config::Instance().dumpConfig || (Config::Instance().verbose > 1)) {
        Config::Instance().Dump();
        if (Config::Instance().dumpConfig) {
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
    IDynLibrary::Ref lib;
#ifdef WIN32
    return DynLibWin::Create();
#elif __linux
    return DynLibLinux::Create(Config::Instance().linuxUseDeepBinding);
#else
    return DynLibLinux::Create(Config::Instance().linuxUseDeepBinding);
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

// We should remove this and wait for a signal (or something) to be raised!
static void RunTestsForAllLibraries() {
    if (Config::Instance().isCoverageRunning) {
        pLogger->Debug("RunTestsForAllLibraries, sending signal to parent, about to run tests!");
        raise(SIGUSR1);
    }

    pLogger->Info("Running tests for all modules");
    for(auto lib : librariesToTest) {
        RunTestsForLibrary(lib);
    }
    // Once done - let the logger catch up...
    gnilk::Logger::Consume();
}

static void RunTestsForLibrary(IDynLibrary::Ref library) {
    TestRunner testRunner(library);
    testRunner.PrepareTests();
    // Since we might be running threads from here - I just want the logger to catch up (flushing it's outgoing queue)
    // this makes logging and printf statements somewhat aligned...
    gnilk::Logger::Consume();
    if (Config::Instance().executeTests) {
        pLogger->Debug("Running tests for: %s", library->Name().c_str());
        testRunner.ExecuteTests();
    } else {
        pLogger->Info("Didn't execute tests for: %s", library->Name().c_str());
    }
}
static void DumpTestsForLibrary(IDynLibrary::Ref library) {
    TestRunner testRunner(library);
    testRunner.PrepareTests();
        printf("=== Library: %s (%s)\n", library->Name().c_str(), library->GetVersion().AsString().c_str());
    testRunner.DumpTestsToRun();
}

static void DumpTestsForAllLibraries() {
    for(auto m : librariesToTest) {
        DumpTestsForLibrary(m);
    }
}
static std::string workingDirectory = {};
static void ResolveCWD() {
    char cwd[PATH_MAX+1] = {};
    getcwd(cwd, PATH_MAX);
    workingDirectory = cwd;
}

int main(int argc, const char **argv) {
    // Cache the logger - also creates the Config singleton with default values
    pLogger = Config::Instance().pLogger;
    if (!ParseArguments(argc, argv)) {
        //return 0;
        exit(0);
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

    ResolveCWD();
    pLogger->Info("Working directory: %s\n", workingDirectory.c_str());

    Timer timer;

    timer.Reset();
    ScanLibraries(Config::Instance().inputs);

    if (Config::Instance().listTests) {
        DumpTestsForAllLibraries();
    }


    // Remove this in 'headless' mode...
    if (!Config::Instance().isSubProcess) {
        printf("--> Start Global\n");
    }

    RunTestsForAllLibraries();

    if (!Config::Instance().isSubProcess) {
        printf("<-- End Global\n");
    }

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

    // Try to flush the logger...
    gnilk::Logger::Consume();
    ResultSummary::Instance().durationSec = timer.Sample();

    PrintSummaryIfNeeded();

    // Need to clear this in order to invoke DTOR on scanner as an instance is kept from 'scanlibraries'
    // Would not have been required if the App would have been contained in a class...  anyway...
    librariesToTest.clear();

    // Non-zero exit when the runner couldn't finish work (a module timed out or
    // crashed). Ordinary test-assertion failures still exit 0 - that path feeds
    // the JSON-consumed CI workflow and must not change.
    return ResultSummary::Instance().incompleteModules.empty() ? 0 : 1;
}

static void PrintSummaryIfNeeded() {
    // Did we execute?
    if (!Config::Instance().executeTests) {
        return;
    }

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
