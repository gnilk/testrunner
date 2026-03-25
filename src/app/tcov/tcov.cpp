//
// tcov - testrunner code-coverage analyzer
//
// tcov acts as frontend to testrunner, running the testrunner through
// lldb, in debugging mode, setting breakpoints on classes/functions
// test-coverage is then calculated by checking how many breakpoints
// where hit during execution.
//
// this is the first version and while it works fairly fine there are
// probably quite a few gaps.
//
// to generate coverage information - run tcov instead of trun - use same arguments
//
// You need to compile with debug information (-g) but should otherwise work with
// release and debug builds..
//
// TODO:
// ! Accept coverage class/function information via cmd line
// ! Better cmd-line handling
//   ! Accept multiple symbols (should be a list - as in trun)
//   ! Allow wildcards in symbols, like 'pucko::*'
//   ! Make the split between target options and tcov '--' (like lldb/gdb does it)
// ! Support for GCOV/LCOV reporting
// + Inline members do not work, see comments in 'Breakpoint.cpp' (should work now)
// ! Generate better reports (ideally some file that can be imported in the IDE)
//   LCOV and DIFF implemented
// ! Ability to run multiple reports in one go (i.e combine LCOV with DIFF)
// + Test multi-statement coverage 'if (X && Y)' - if X failed Y might not be evalulated
// - Prolouge statemens are still reported as untested (single lines with '}' for instance)
// - Ability to pass arguments to the report generator
//   This will be tricky with the comma separated list of reports..
// - If we allow multiple -R arguments we need to enhance the ArgParser to continue parsing
//   after the inital has been hit. The ArgParser currently expects only one instance of each
//   Which is not quite true for "Count" functions...
// - Verify how this works outside 'trun', we must disable the signal trapping in that case
//   See comments in 'Coverage.cpp' (RunInitialLLDBPhase)
//
//
#include <filesystem>
#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>
#include <sys/stat.h>


#ifdef APPLE
#include <lldb/SBDebugger.h>
#include <lldb/SBTarget.h>
#include <lldb/SBProcess.h>
#include <lldb/SBTrace.h>
#include <lldb/SBThread.h>
#include <lldb/SBBroadcaster.h>
#include <lldb/SBListener.h>
#include <lldb/SBStream.h>
#include <lldb/SBEvent.h>
#include <lldb/SBUnixSignals.h>
#include <lldb/SBLineEntry.h>
#else
#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBTrace.h>
#include <lldb/API/SBThread.h>
#include <lldb/API/SBBroadcaster.h>
#include <lldb/API/SBListener.h>
#include <lldb/API/SBStream.h>
#include <lldb/API/SBEvent.h>
#include <lldb/API/SBUnixSignals.h>
#include <lldb/API/SBLineEntry.h>
#endif

#include "logger.h"
#include "CoverageIPCMessages.h"
#include "ipc/IPCDecoder.h"
#include "unix/IPCFifoUnix.h"
#include "Coverage.h"
#include "ArgParser.h"
#include "strutil.h"
#include "Config.h"
#include "timer.h"


using namespace tcov;

//
// Test like this:
// --target ./trun --symbols pucko::DateTime -- --sequential -m datetime /home/gnilk/src/work/embedded/libraries/PuckoNew/cmake-build-debug/lib/libpucko_utests.so
//
#ifdef LINUX
static bool IsLLDBServerPresent();
static std::string TryDetectLLDBServer();
static std::string TryDetectFile(const std::string &name);
static bool IsValidLLDBServer(const std::string &lldbServer);
static bool IsExecutable(const std::string& path);

#endif
static void PrepareCoverageSymbols();


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

static void PrintUsage(const char *prgname) {
#ifdef _WIN64
    std::string strPlatform = "Windows x64 (64 bit)";
#elif WIN32
    std::string strPlatform = "Windows x86 (32 bit)";
#elif __linux
    std::string strPlatform = "Linux";
#else
    std::string strPlatform = "macOS";
#endif

    printf("Coverage tool v%s - %s - %s\n",
        Config::Instance().version.c_str(),
        strPlatform.c_str(),
        Config::Instance().description.c_str());

    //printf("%s - coverage tool for LLDB\n", prgname);
    printf("Usage: %s [options] -- <target cmd line>\n", prgname);
    printf("Options:\n");
    printf("  -h, --help              Print this help\n");
    printf("  -v, --verbose           Verbose output\n");
    printf("  -t, --target            Target executable to run (default: trun)\n");
    printf("  -R, --Report            Comma separated list of report engines (base, lcov, diff)\n");
    printf("  -s, --symbols           Comma separated list of symbols to track for coverage\n");
    printf("Linux\n");
    printf("  --lldb-server <path>    Set the full path to the lldb-server binary\n");
    printf("\n");
    printf("Examples:\n");
    // -vvv --target ./trun --symbols pucko::DateTime --  -m datetime /home/gnilk/src/work/embedded/libraries/PuckoNew/cmake-build-debug/lib/libpucko_utests.so
    printf("Run locally (same directory) compiled 'trun' generate coverage for 'MyClass' pass '-m myclass ./libunittests.so' to trun\n");
    printf("  %s --target ./trun --symbols MyClass -- -m myclass ./libunittests.so\n", prgname);
    printf("\n");
    printf("Run same as above but use both diff and lcov\n");
    printf("  %s -R diff,lcov --target ./trun --symbols MyClass -- -m myclass ./libunittests.so\n", prgname);

}

typedef enum {
    kExit,
    kContinue,
} kParseArgRes;

// -v --target ./trun --symbols pucko::DateTime -- --sequential -m datetime /home/gnilk/src/work/embedded/libraries/PuckoNew/cmake-build-debug/lib/libpucko_utests.so
static kParseArgRes ParseArguments(int argc, const char *argv[]) {
    ArgParser argparser(argc, argv);
    //argparser.TryParse("-h","--help")
    if (argparser.IsPresent("-hH?","--help")) {
        PrintUsage(argv[0]);
//        printf("  -i, --tcov-ipc-name <ipc>  Name of the IPC FIFO to use for communication\n");
        return kExit;
    }

    // Check if cache directory is specified on the cmd-line, if it is we use it, otherwise we resolve it...
    std::string cache_dir;
    cache_dir = *argparser.TryParse(cache_dir, "", "--cache-dir");
    if (cache_dir.empty()) {
        cache_dir = Config::Instance().ResolveCacheDir();
    }
    Config::Instance().cache_dir = cache_dir;

    Config::Instance().verbose = argparser.CountPresence("-v", "--verbose");
    Config::Instance().target = *argparser.TryParse(Config::Instance().target, "-t","--target");
    Config::Instance().symbolString = *argparser.TryParse(Config::Instance().symbolString, "-s","--symbols");
    Config::Instance().lldb_server_path = *argparser.TryParse(Config::Instance().lldb_server_path, "","--lldb-server");
    Config::Instance().internal_test_startup = argparser.IsPresent("", "--test-startup");
    if (argparser.IsPresent("-R","--Report")) {
        // Save the default
        std::string reportArg = Config::Instance().reportEngines[0];
        Config::Instance().reportEngines.clear();

        reportArg = *argparser.TryParse(reportArg, "-R","--Report");
        trun::split(Config::Instance().reportEngines, reportArg.c_str(), ',');
        if (Config::Instance().reportEngines.empty()) {
            fprintf(stderr, "ERR: Invalid report engine '%s' (should command separated list)\n", reportArg.c_str());
            return kExit;
        }
    }

    // Ensure we run the test with highest verbose level
    if (Config::Instance().internal_test_startup) {
        Config::Instance().verbose = 3;
    }

    ConfigureLogger();

    PrepareCoverageSymbols();

    //trun::split(Config::Instance().symbols, Config::Instance().symbolString.c_str(), ',');

    if (!Config::Instance().internal_test_startup && (argparser.CopyAllAfter(Config::Instance().target_args, "--") < 0)) {
        fprintf(stderr, "Unable to parse target arguments\n");
        PrintUsage(argv[0]);
        return kExit;
    }

#ifdef LINUX
    // Note: this is not needed on macOS as the LLDB Server process is already running - at least if you have xcode installed
    if (!IsLLDBServerPresent()) {
        fprintf(stderr, "Unable to find or detect the lldb server, you can specify path to the 'lldb-server' binary with '--lldb-server <path to binary>'\n");
        return kExit;
    };

    auto logger = gnilk::Logger::GetLogger("CoverageRunner");
    logger->Info("Setting LLDB Server Path Env: %s", Config::Instance().lldb_server_path.c_str());
    setenv("LLDB_DEBUGSERVER_PATH", Config::Instance().lldb_server_path.c_str(), 1);
#endif

    return Config::Instance().internal_test_startup ? kExit : kContinue;
}

static void PrepareCoverageSymbols() {
    std::vector<std::string> symbols;
    trun::split(symbols, Config::Instance().symbolString.c_str(), ',');
    for (auto &s : symbols) {
        Config::CoverageSymbol symbol;
        symbol.name = s;
        if ((s.find_first_of("*") != std::string::npos) || (s.find_first_of("?") != std::string::npos)) {
            symbol.isGlob = true;

            if (s.find_first_of("*") != std::string::npos) {
                symbol.globPrefix = s.substr(0, s.find_first_of("*"));
            } else {
                symbol.globPrefix = s.substr(0, s.find_first_of("?"));
            }
        }
        Config::Instance().symbols.push_back(symbol);
    }
}

// basically all contained in the class CoverageRunner...
int main(int argc, const char *argv[]) {
    // Initialize the logger - we need this to set some default values
    gnilk::Logger::Initialize();
    auto res = ParseArguments(argc, argv);
    if (res == kExit) {
        return 1;
    }

    CoverageRunner coverageRunner;
    trun::Timer timer;
    timer.Reset();
    double tStart = timer.Sample();
    if (!coverageRunner.Begin()) {
        return 1;
    }
    coverageRunner.Process();
    double tElapsed = timer.Sample() - tStart;
    coverageRunner.Report(tElapsed);
    coverageRunner.End();
    return 0;
}

//
// Helpers
//

//
// This tries to find any installation of LLDB on your system...
// Basically by executing 'which <filename>' and checking the result...
// It will enumerate: lldb-server, lldb-server-20, lldb-server-19, lldb-server-18,...  down to lldb-server-16
// I've just added a bunch of number's without much consideration - should at least not need updating for a year or two..
// One can always override with '--lldb-server'
//
#ifdef LINUX
static bool IsLLDBServerPresent() {
    auto logger = gnilk::Logger::GetLogger("CoverageRunner");

    // If someone is using this variable - they know what they are doing, but we verify anyway..
    const char *currentLLDB = getenv("LLDB_DEBUGSERVER_PATH");
    if (currentLLDB != nullptr) {
        auto currentLLDBPath = std::string();
        if (!currentLLDBPath.empty() && IsValidLLDBServer(currentLLDBPath)) {
            logger->Info("'LLDB_DEBUGSERVER_PATH' env found and verified - using");
            Config::Instance().lldb_server_path = currentLLDBPath;
            return true;
        }
    }

    // Verify currently configured pathname
    if (IsValidLLDBServer(Config::Instance().lldb_server_path)) {
        logger->Info("Configured 'lldb-server' path (%s) is valid - using", Config::Instance().lldb_server_path.c_str());
        return true;
    }

    logger->Warning("No valid 'lldd-server' found - trying well known candidates");

    auto lldbServer = TryDetectLLDBServer();
    if (!lldbServer.empty()) {
        logger->Info("Detected valid lldb-server: '%s' - using", lldbServer.c_str());
        Config::Instance().lldb_server_path = lldbServer;
        return true;
    }

    return false;
}

static bool IsValidLLDBServer(const std::string &lldbServer) {
    std::filesystem::path lldbPathName = lldbServer;

    // Verify are good...
    if (exists(lldbPathName) && is_regular_file(lldbPathName) && IsExecutable(lldbPathName)) {
        return true;
    }
    return false;
}

static bool IsExecutable(const std::string& path) {
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) != 0) {
        return false; // File does not exist
    }
    return (fileStat.st_mode & S_IXUSR) || (fileStat.st_mode & S_IXGRP) || (fileStat.st_mode & S_IXOTH);
}


static std::string TryDetectLLDBServer() {
    static std::vector<std::string> possibleFiles = {
        "lldb-server",
        "lldb-server-20",
        "lldb-server-19",
        "lldb-server-18",
        "lldb-server-17",
        "lldb-server-16",
        "lldb-server-15",
    };
    auto logger = gnilk::Logger::GetLogger("CoverageRunner");

    for (auto &fileName : possibleFiles) {
        logger->Debug("Trying: %s", fileName.c_str());
        auto result = TryDetectFile(fileName);
        if (!result.empty() && IsValidLLDBServer(result)) {
            return result;
        }
    }
    return {};
}
static std::string TryDetectFile(const std::string &name) {
    char cmd[256] = {};
    snprintf(cmd, 255, "which %s 2>/dev/null", name.c_str());
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return "";

    char buffer[256] = {};
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe)) {
        result = buffer;
        result.erase(result.find_last_not_of(" \n\r\t") + 1);
    }
    pclose(pipe);
    return result;
}
#endif
