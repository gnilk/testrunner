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
// - Better cmd-line handling
//   - Accept multiple symbols (should be a list - as in trun)
//   - Allow wildcards in symbols, like 'pucko::*'
//   ! Make the split between target options and tcov '--' (like lldb/gdb does it)
// - Support for GCOV/LCOV reporting
// + Inline members do not work, see comments in 'Breakpoint.cpp' (should work now)
// + Generate better reports (ideally some file that can be imported in the IDE)
// + Test multi-statement coverage 'if (X && Y)' - if X failed Y might not be evalulated
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

using namespace tcov;

//
// Test like this:
// --target ./trun --symbols pucko::DateTime -- --sequential -m datetime /home/gnilk/src/work/embedded/libraries/PuckoNew/cmake-build-debug/lib/libpucko_utests.so
//

// struct TCOVConfig {
//     std::string target = "trun";
//     int verbose = 0;
//     std::string ipc_name = "tcov-ipc";
//     std::string symbolString = {};
//     std::vector<std::string> symbols = {};
//     std::vector<std::string> target_args = {};
// };
// static TCOVConfig g_config;

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

static void ParseArguments(int argc, const char *argv[]) {
    ArgParser argparser(argc, argv);
    //argparser.TryParse("-h","--help")
    if (argparser.IsPresent("hH?","help")) {
        printf("tcov - coverage tool for LLDB\n");
        printf("Usage: tcov [options]\n");
        printf("Options:\n");
        printf("  -h, --help              Print this help\n");
        printf("  -v, --verbose           Verbose output\n");
        printf("  -t, --target            Target executable to run (default: trun)\n");
        printf("  -s, --symbols           Comma separated list of symbols to track for coverage\n");
//        printf("  -i, --tcov-ipc-name <ipc>  Name of the IPC FIFO to use for communication\n");
        return;
    }
    // TODO: I need a stop condition at '--' because I want '--' as separator between our arguments and target arguments
    Config::Instance().verbose = argparser.CountPresence("-v", "--verbose");
    Config::Instance().target = *argparser.TryParse(Config::Instance().target, "-t","--target");
    Config::Instance().symbolString = *argparser.TryParse(Config::Instance().symbolString, "-s","--symbols");

    ConfigureLogger();

    trun::split(Config::Instance().symbols, Config::Instance().symbolString.c_str(), ',');

    if (argparser.CopyAllAfter(Config::Instance().target_args, "--") < 0) {
        fprintf(stderr, "Unable to parse target arguments\n");
        return;
    }

}

// basically all contained in the class CoverageRunner...
int main(int argc, const char *argv[]) {
    ParseArguments(argc, argv);
    CoverageRunner coverageRunner;
    if (!coverageRunner.Begin()) {
        return 1;
    }
    coverageRunner.Process();
    coverageRunner.Report();
    coverageRunner.End();
    return 0;
}

