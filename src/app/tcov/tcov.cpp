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
// - Inline members do not work, see comments in 'Breakpoint.cpp'
// - Generate better reports (ideally some file that can be imported in the IDE)
// - Test multi-statement coverage 'if (X && Y)' - if X failed Y might not be evalulated
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

using namespace tcov;

// basically all contained in the class CoverageRunner...
int main(int argc, const char *argv[]) {
    CoverageRunner coverageRunner;
    if (!coverageRunner.Begin(argc, argv)) {
        return 1;
    }
    coverageRunner.Process();
    coverageRunner.Report();
    coverageRunner.End();
    return 0;
}

// ============
// only old test code beyond this code

static gnilk::IPCFifoUnix tcovIPCServer;
// Bla - this should be broken in to a multi-structure one for the 'symbol' (file + function) and one for line+breakpoint
struct MyBreakpoint {
    std::string filename;
    std::string function;
    lldb::addr_t address;
    uint32_t line;
    lldb::SBBreakpoint breakpoint;
};

//
// This will create coverage breakpoint based on addresses instead of lines and then map back to lines
// thus, we will have a few more breakpoints so we need to sort/unique this a bit...
//
static std::vector<MyBreakpoint> CreateCoverageBreakpoints(lldb::SBTarget &target, const std::string &symbol) {
    std::vector<MyBreakpoint> breakpoints;

    auto symbollist = target.FindFunctions(symbol.c_str());
    if (symbollist.IsValid()) {
        printf("Dumping symbols, num=%u\n", symbollist.GetSize());
        for (size_t i=0;i<symbollist.GetSize();i++) {
            auto ctx = symbollist.GetContextAtIndex(i);
            auto compileUnit = ctx.GetCompileUnit();
            auto func = ctx.GetFunction();
            auto startAddr = ctx.GetFunction().GetStartAddress();
            auto endAddr = ctx.GetFunction().GetEndAddress();
            auto startLine = startAddr.GetLineEntry().GetLine();

            auto name = ctx.GetSymbol().GetDisplayName();
            printf("%zu:%s @ %s:%u at address: %llX (to: %llX) \n", i, name, compileUnit.GetFileSpec().GetFilename(), startLine, startAddr.GetFileAddress(), endAddr.GetFileAddress());


            auto numLines = compileUnit.GetNumLineEntries();
            for (uint32_t line = 0; line < numLines; line++) {
                auto lineEntry = compileUnit.GetLineEntryAtIndex(line);
                if (!lineEntry.IsValid()) {
                    continue;
                }

                // 'invalid'?
                if (lineEntry.GetLine() == 0) {
                    printf("lineEntry.GetLine() == 0, invalid\n");
                    continue;
                }
                if (!lineEntry.GetFileSpec().IsValid()) {
                    printf("Filespec for line entry invalid\n");
                    continue;
                }
                // Filter out lines starting at column 0 - normally does not count...
                // FIXME: Put this on a flag - aggressive filtering (might be good for large projects)
                //        For 'normal' code the 'Proluge' check will detect this...
                // if (lineEntry.GetColumn() == 0) {
                //     printf("lineEntry.GetColumn() == 0, invalid (for line=%u)\n", lineEntry.GetLine());
                //     continue;
                // }

                auto addr = lineEntry.GetStartAddress();
                if (!addr.IsValid()) {
                    continue;
                }
                if (addr.GetLoadAddress(target) == LLDB_INVALID_ADDRESS) {
                    printf("INVALID address\n");
                    continue;;
                }
                // FIXME: Put this on a flag - aggressive filtering
                if (addr.GetLoadAddress(target) == func.GetStartAddress().GetLoadAddress(target)) {
                    printf("Prolouge - skipping\n");
                    continue;
                }

                // FIXME: Don't create breakpoints here - just gather the coverage addresses
                // seems DWARF data can contain multiple addresses pointing to same line etc...

                if (addr.GetLoadAddress(target) >= startAddr.GetLoadAddress(target) &&
                    addr.GetLoadAddress(target) < endAddr.GetLoadAddress(target)) {
                    // Not sure which one is best - or if one translates to the other..
                    //auto bp = target.BreakpointCreateByAddress(addr.GetLoadAddress(target));
                    auto bp = target.BreakpointCreateBySBAddress(addr);
                    //bp.SetAutoContinue(true);

                    auto bp_line = addr.GetLineEntry().GetLine();
                    MyBreakpoint my_breakpoint = {
                        compileUnit.GetFileSpec().GetFilename(),
                        name,
                        addr.GetLoadAddress(target),
                            bp_line,
                            bp,
                        };



                    breakpoints.push_back(my_breakpoint);
                }

            }
        }
    } else {
        printf("No symbols...\n");
    }
    return breakpoints;

}


static lldb::SBBreakpoint CreateSingleBreakPoint(lldb::SBTarget &target, const std::string &symbol) {
    std::vector<MyBreakpoint> breakpoints;

    auto symbollist = target.FindFunctions(symbol.c_str());
    if (symbollist.IsValid()) {
        printf("Dumping symbols, num=%u\n", symbollist.GetSize());
        for (size_t i=0;i<symbollist.GetSize();i++) {
            auto ctx = symbollist.GetContextAtIndex(i);
            auto compileUnit = ctx.GetCompileUnit();
            auto startAddr = ctx.GetFunction().GetStartAddress();
            auto endAddr = ctx.GetFunction().GetEndAddress();
            auto startLine = startAddr.GetLineEntry().GetLine();

            // Offset the end by '-1' to move it into the scope which will cause the line entry to point to the last instruction...
            endAddr.OffsetAddress(-1);
            auto endLine = endAddr.GetLineEntry().GetLine();
            auto name = ctx.GetSymbol().GetDisplayName();
            printf("%zu:%s @ %s (%u -> %u) at address: %llX (to: %llX) \n", i, name, compileUnit.GetFileSpec().GetFilename(), startLine, endLine, startAddr.GetFileAddress(), endAddr.GetFileAddress());
            return target.BreakpointCreateBySBAddress(startAddr);
        }
    }
    printf("CreateSingleBreakPoint, Unable to resolve '%s'\n", symbol.c_str());
    return {};
}

static void signal_handler(int sig) {
    if (sig == SIGHUP) {
        printf("Got SIG USR1\n");
    }
}

static void EnumerateMembers(lldb::SBTarget &target, const std::string &className) {
    auto classtype = target.FindTypes(className.c_str());

    //auto classctx = target.FindSymbols("Dummy");
    if (classtype.IsValid()) {
        printf("class type ok - size=%u\n", classtype.GetSize());
        for (size_t i=0;i<classtype.GetSize();i++) {
            auto ct = classtype.GetTypeAtIndex(i);
            printf("%zu:%s\n",i,ct.GetDisplayTypeName());

            if (!ct.IsValid()) {
                printf("Invalid - skipping\n");
                continue;
            }

            if (ct.IsPolymorphicClass()) {
                printf("Class found - good!\n");
                for (size_t j=0; j<ct.GetNumberOfMemberFunctions();j++) {
                    auto member = ct.GetMemberFunctionAtIndex(j);
                    printf("  %zu:%s\n",j, member.GetName());
                }
            }

        }
    }
}


// This calculates coverage breakpoints by using line-numbers - but there is some trickery involved and this might not always
// work...
static std::vector<MyBreakpoint> CreateCoverageBreakpoints_old(lldb::SBTarget &target, const std::string &symbol) {
    std::vector<MyBreakpoint> breakpoints;

    auto symbollist = target.FindFunctions("Dummy::SomeFunc");
    if (symbollist.IsValid()) {
        printf("Dumping symbols, num=%u\n", symbollist.GetSize());
        for (size_t i=0;i<symbollist.GetSize();i++) {
            auto ctx = symbollist.GetContextAtIndex(i);
            auto compileUnit = ctx.GetCompileUnit();
            auto startAddr = ctx.GetFunction().GetStartAddress();
            auto endAddr = ctx.GetFunction().GetEndAddress();
            auto startLine = startAddr.GetLineEntry().GetLine();


            // Offset the end by '-1' to move it into the scope which will cause the line entry to point to the last instruction...
            endAddr.OffsetAddress(-1);
            auto endLine = endAddr.GetLineEntry().GetLine();
            auto name = ctx.GetSymbol().GetDisplayName();
            printf("%zu:%s @ %s (%u -> %u) at address: %llX (to: %llX) \n", i, name, compileUnit.GetFileSpec().GetFilename(), startLine, endLine, startAddr.GetFileAddress(), endAddr.GetFileAddress());
            target.BreakpointCreateBySBAddress(startAddr);

            // Create a break point for each line...
            for (uint32_t bp_line = startLine; bp_line < (endLine+1); bp_line++) {
                auto bp = target.BreakpointCreateByLocation(compileUnit.GetFileSpec().GetFilename(), bp_line);

                bp.SetAutoContinue(true);   // just push on...
                MyBreakpoint my_breakpoint = {
                    compileUnit.GetFileSpec().GetFilename(),
            name,
                    startAddr.GetLoadAddress(target),      // bogus - we don't have this in this version...
                        bp_line,
                        bp,
                    };


                breakpoints.push_back(my_breakpoint);
            }

        }
    } else {
        printf("No symbols...\n");
    }
    return breakpoints;
}

static void ConsumeFIFO() {
    if (!tcovIPCServer.Available()) {
        //printf("ConsumeFIFO: tcovIPCServer not available\n");
        return;
    }
    printf("ConsumeFIFO, data available..\n");
    while (tcovIPCServer.Available()) {
        trun::CovIPCCmdMsg ipcMsg;
        gnilk::IPCBinaryDecoder decoder(tcovIPCServer, ipcMsg);
        if (!decoder.Process()) {
            continue;
        }

        printf("BeginCoverage for '%s'\n",ipcMsg.symbolName.c_str());
    }
}

// Example of 'debugging' a file programmatically with LLDB...
// and capturing the output of the debuggee...
static int oldmain (int argc, const char *argv[]) {
    auto my_pid = getpid();
    printf("tcov - start, pid=%d\n",my_pid);

    // Setup IPC Fifo - this is what we use..
    if (!tcovIPCServer.Open()) {
        fprintf(stderr, "Unable to create IPC Server\n");
        return 1;
    }
    printf("TCOV IPC Server at: %s\n", tcovIPCServer.FifoName().c_str());
    // while (true) {
    //     ConsumeFIFO();
    //     std::this_thread::yield();
    // }
    // exit(1);

    char cwd[1024] = {};
    getcwd(cwd, 1023);
    printf("CWD: %s\n", cwd);

    lldb::SBDebugger::Initialize();
    auto lldbDebugger = lldb::SBDebugger::Create(false);
    // these are important...
    lldbDebugger.SetAsync(false);       // lldb is async by default - which is good, but complicated to start with
    lldbDebugger.SetOutputFileHandle(stdout, false);
    lldbDebugger.SetErrorFileHandle(stderr, false);

    // Create our target
    static std::string targetPathName = "/Users/gnilk/src/github.com/testrunner/cmake-build-debug/trun";

    lldb::SBError error;
    auto target = lldbDebugger.CreateTarget(targetPathName.c_str(), nullptr, nullptr, true, error);
    if (!target.IsValid()) {
        fprintf(stderr, "Invalid target '%s'\n", argv[0]);
        return 1;
    }

    // Make sure we control launch - lldb starts in running mode by default - so we insert this..
    auto bpMain = target.BreakpointCreateByName("main");



    //printf("Creating breakpoint for 'RunTestsForAllLibraries'\n");

    // IF 'testrunner' have debug symbols - we can do this...
    // auto bpAfterLibrariesScanned = CreateSingleBreakPoint(target, "RunTestsForAllLibraries");
    // if (!bpAfterLibrariesScanned.IsValid()) {
    //     fprintf(stderr, "breakpoint 'RunTestsForAllLibraries' failed - can't continue\n");
    //     return 1;
    // }


    // How can we split trun and coverage analysis?
    // trun --coverage <everything else>
    //   => trun is front-end (which is good, we like that - things stay they way they are)
    //      trun would launch coverage application which in turn run's trun with specific stuff (how do these communication??)
    //
    //

    // in the unit test we can add a function for coverage (in the main file)
    // like:
    //      t->MeasureCoverageForClass("Dummy");
    // however, we would also like to:
    //      t->MeasureCoverageForFile("my_file_with_static_functions");
    // Or perhaps a specific function:
    //      t->MeasureCoverageForFunction("myfunction");
    // or:
    //      t->MeasureCoverageForFunction("MyClass::SomeFunction");
    //
    // This could go into 'test_<module>'
    //  TRUN_Coverage *coverage = {};
    //  if (t->QueryInterface(TRUN_Coverage, &coverage)) {
    //     coverage->MeasureClass("DateTime");
    //     coverage->MeasureFile("SomeFile.cpp");
    //     // etc...
    //  }
    //

    // this can be used to create a coverage tracking structure for a function (class related or not)
    // Set a breakpoint here...



    // Create launch-info, we need this in order to redirect stdout/stderr

    // use this to forward arguments to the target...
    // TODO:
    //  1) create the specific 'tcov' arguments (--coverage, --tcov-ipc-name <ipc>)
    //  2) take all other arguments from cmd-line and append
    const char *target_argv[]={"--sequential", "--coverage","--tcov-ipc-name",tcovIPCServer.FifoName().c_str(),"-vvv", "-m", "coverage", nullptr};
    lldb::SBLaunchInfo launch_info(target_argv);
    launch_info.SetWorkingDirectory(cwd);   // se should be here and not where the target is located

    // Make sure we duplicate stdout/stderr to the debuggee
    launch_info.AddDuplicateFileAction(STDOUT_FILENO, STDOUT_FILENO);
    launch_info.AddDuplicateFileAction(STDERR_FILENO, STDERR_FILENO);

    // launch target
    auto process = target.Launch(launch_info, error);
    if (!process.IsValid() || error.Fail()) {
        fprintf(stderr, "Launch failed for target '%s': %s\n", argv[0], error.GetCString());
        return 1;
    }

    // Try send 'SIGUSR1' when we have done 'dlopen' (no debug info needed)
    auto unixSignals = process.GetUnixSignals();
    unixSignals.SetShouldStop(SIGUSR1, true);
    unixSignals.SetShouldNotify(SIGUSR1, true);
    unixSignals.SetShouldSuppress(SIGUSR1, true);

    unixSignals.SetShouldStop(SIGUSR2, true);
    unixSignals.SetShouldNotify(SIGUSR2, true);
    unixSignals.SetShouldSuppress(SIGUSR2, true);


    auto state = process.GetState();
    printf("Start State = %d\n", state);

    // verify (or at least sanity check we have a PID)
    auto pid = process.GetProcessID();
    printf("PID: %llu\n", pid);

    // Verify the broadcaster is valid - this is needed if you want to process messages in some kind of event loop
    // we don't do this now...
    if (!process.GetBroadcaster().IsValid()) {
        fprintf(stderr, "Invalid broadcaster\n");
        return 1;
    }


    // we have started - and we should be at 'main' now...  verify
    if (!(bpMain.GetHitCount() > 0)) {
        fprintf(stderr, "main breakpoint not hit\n");
        return 1;
    }

    state = process.GetState();
    if (lldb::eStateStopped != state) {
        fprintf(stderr, "wrong state, state=%d\n", state);
        return 1;
    }

    printf("@main - continue to 'RunTestsForAllLibraries'\n");

    // This will continue until 'SIGUSR1' is raised
    process.Continue();

    state = process.GetState();
    if (lldb::eStateStopped != state) {
        fprintf(stderr, "wrong state, state=%d\n", state);
        return 1;
    }
    // Check that we really got SIGUSR1 - raised once dylibs have been loaded...
    auto thread = process.GetSelectedThread();
    if (thread.GetStopReason() == lldb::eStopReasonSignal) {
        uint32_t data_count = thread.GetStopReasonDataCount();
        if (data_count > 0) {
            int signo = thread.GetStopReasonDataAtIndex(0);
            printf("Stopped by signal: %d\n", signo);
            if (signo == SIGUSR1) {
                printf("Dynlibs should now be loaded - looking for details\n");
            } else {
                fprintf(stderr, "Out of sync signal - bailing\n");
                return 1;
            }
        }
    }


    // printf("Ok - @'RunTestsForAllLibraries' = %u\n", bpAfterLibrariesScanned.GetHitCount());
    printf("Dynlibs should now be loaded - looking for details\n");



    //
    // Resolve ALL module based breakpoints
    //

    // We can do either one of these..
    //auto breakpoint = target.BreakpointCreateByLocation("dbgme.cpp", 6);
    EnumerateMembers(target, "CTestCoverage");

    // should have a list of these per symbol
    auto breakpoints = CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    if (breakpoints.size() == 0) {
        fprintf(stderr, "Coverage symbol information not resolved\n");
        return 1;
    }
    printf("Num Coverage BP's found = %zu\n", breakpoints.size());

    // files are 'flat' no hierarchy...  (makes sense - public symbols can't have same name)
    // wonder how it deals with same filename in different directories...
    //auto breakpoint = target.BreakpointCreateByName("another_test_func");
    // auto breakpoint = target.BreakpointCreateByName("Dummy::SomeFunc");
    // //auto breakpoint = target.BreakpointCreateByLocation("dbgme_src2.cpp", 11);
    // if (!breakpoint.IsValid()) {
    //     fprintf(stderr, "Invalid breakpoint '%s'\n", argv[0]);
    //     return 1;
    // }
    // IF auto-continue is enabled - the breakpoint will get a hit - but won't stop there
    // which could be nice for coverage tracing...
    //breakpoint.SetAutoContinue(true);
    lldb::SBStream desc;
    // breakpoint.GetDescription(desc);
    // printf("BP Desc: %s\n", desc.GetData());


    printf("Now running...\n");
    // continue until we have exited
    while (true) {
        process.Continue();
        state = process.GetState();
        if ((state == lldb::eStateExited) || (state == lldb::eStateCrashed)) {
            printf("tcov - Done!\n");
            break;
        }
        if (state == lldb::eStateStopped) {
            thread = process.GetSelectedThread();
            // Check if stopped by signal...
            if (thread.GetStopReason() == lldb::eStopReasonSignal) {
                printf("Stopped by signal\n");
                uint32_t data_count = thread.GetStopReasonDataCount();
                if (data_count > 0) {
                    int signo = thread.GetStopReasonDataAtIndex(0);
                    if (signo == SIGUSR1) {
                        process.SetSelectedThread(thread);
                        printf("Stopped by SIGUSR2 - read IPC Server Data!\n");
                        ConsumeFIFO();
                    }
                }
                std::this_thread::yield();
            } else {
                auto frame = thread.GetSelectedFrame();
                auto line = frame.GetLineEntry();

                if (line.IsValid()) {
                    auto filename = line.GetFileSpec().GetFilename();
                    auto lineno = line.GetLine();
                    auto pc = frame.GetPC();
                    printf("Breakpoint at %s:%u - PC=%llX\n", filename, lineno, pc);
                } else {
                    printf("Breakpoint, but line info is bad!\n");
                }
            }
        }
    }

    // get the exit code from the debuggee
    auto exit_code = process.GetExitStatus();
    printf("Target finished with exit code: %d\n", exit_code);

    size_t linestested = 0;
    for (auto &bp : breakpoints) {

        printf("%llX - %s:%d:%d\n",bp.address, bp.function.c_str(), bp.line, bp.breakpoint.GetHitCount());
        if (bp.breakpoint.GetHitCount() > 0) {
            linestested++;
        }
    }
    auto factor = (float)linestested / (float)breakpoints.size();
    printf("coverage=%f = %d%%\n", factor, (int)(100 * factor));

    // terminate everything..
    lldb::SBDebugger::Terminate();
    return 0;
}