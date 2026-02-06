#include <filesystem>
#include <iostream>
#include <thread>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <unistd.h>
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

#include "logger.h"
#include "CoverageIPCMessages.h"
#include "ipc/IPCDecoder.h"
#include "unix/IPCFifoUnix.h"

static gnilk::IPCFifoUnix tcovIPCServer;

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

// Bla - this should be broken in to a multi-structure one for the 'symbol' (file + function) and one for line+breakpoint
struct MyBreakpoint {
    std::string filename;
    std::string function;
    lldb::addr_t address;
    uint32_t line;
    lldb::SBBreakpoint breakpoint;
};

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

//
// FIXME: clean this mess up a bit...
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

class CoverageRunner {
public:
    CoverageRunner() = default;
    virtual ~CoverageRunner() = default;

    bool Begin(int argc, const char *argv[]);
    void End();
    void Process();
    void Report();
protected:
    bool ParseArgs(int argc, const char *argv[]);
    static void ConvertArgs(std::vector<char *> &out, std::vector<std::string> &args);
    bool CreateIPCServer();
    void ResolveCWD();
    void SuppressSignals();
    bool WaitState(lldb::StateType targetState, uint32_t timeoutMSec);
    bool WasSignalRaised(int expectedSignal);   // note: on macos the signal type for raise is 'int'
    void HandleIPCInterrupt();
    void CheckBreakPointHit(lldb::SBThread &thread);
    void ConsumeIPC();
    trun::CovIPCCmdMsg ReadIPCMessage();

    // More functions
private:
    static const int sig_DYNLIB_LOADED = SIGUSR1;
    static const int sig_IPC_INTERRUPT = SIGUSR1;
    gnilk::IPCFifoUnix ipcServer;
    gnilk::ILogger *logger = {};
    std::string targetPathName = {};
    std::string workingDirectory = {};
    std::vector<std::string> trunArgsVector;
    std::vector<MyBreakpoint> breakpoints;

    // LLDB API variables
    lldb::SBDebugger lldbDebugger;
    lldb::SBTarget target;
    lldb::SBBreakpoint bpMain;
    lldb::SBProcess process;
    pid_t pid;
    int exit_code;
};


bool CoverageRunner::Begin(int argc, const char *argv[]) {
    logger = gnilk::Logger::GetLogger("CoverageRunner");
    ParseArgs(argc, argv);
    for (auto &arg : trunArgsVector) {
        printf("arg=%s\n", arg.c_str());
    }

    if (!CreateIPCServer()) {
        logger->Error("Unable to create IPC Server");
        return false;
    }

    // Add the following so TRUN knows we are running in coverage mode and how to communicate back...
    // '--coverage' is a bit redundant here - but let's keep them separate for now...
    trunArgsVector.push_back("--coverage");
    trunArgsVector.push_back("--tcov-ipc-name");
    trunArgsVector.push_back(ipcServer.FifoName());

    ResolveCWD();

    std::string dbg_argString;
    for (auto &arg : trunArgsVector) {
        dbg_argString += arg + " ";
    }
    logger->Info("Target: %s %s", targetPathName.c_str(), dbg_argString.c_str());
    logger->Info("Working Directory: %s", workingDirectory.c_str());


    // Initialize LLDB
    lldb::SBDebugger::Initialize();

    // Create the debugger
    lldbDebugger = lldb::SBDebugger::Create(false);

    // Remap stdout/stderr
    // FIXME: check if we can redirect to logger instead
    lldbDebugger.SetAsync(false);       // lldb is async by default - which is good, but complicated to start with
    lldbDebugger.SetOutputFileHandle(stdout, false);
    lldbDebugger.SetErrorFileHandle(stderr, false);


    // Create our target
    if (targetPathName.empty()) {
        // FIXME: Only in debug builds...
        targetPathName = "/Users/gnilk/src/github.com/testrunner/cmake-build-debug/trun";
    }

    lldb::SBError error;
    target = lldbDebugger.CreateTarget(targetPathName.c_str(), nullptr, nullptr, true, error);
    if (!target.IsValid()) {
        logger->Error("Invalid target '%s'", argv[0]);
        return false;
    }

    // LLDB starts the program in 'running' mode - so we create this breakpoint to control execution
    // 'main' is present even in release builds, other options would be '_start' or
    // Make sure we control launch - lldb starts in running mode by default - so we insert this..
    bpMain = target.BreakpointCreateByName("main");



    // Convert trunArgs to argv style char *[]
    std::vector<char *> trunArgs;
    ConvertArgs(trunArgs, trunArgsVector);
    // done
    //char **target_argv = args.data();

    //const char *target_argv[]={"--sequential", "--coverage","--tcov-ipc-name",tcovIPCServer.FifoName().c_str(),"-vvv", "-m", "coverage", nullptr};
    lldb::SBLaunchInfo launch_info(const_cast<const char **>(trunArgs.data()));
    launch_info.SetWorkingDirectory(workingDirectory.c_str());   // se should be here and not where the target is located

    // Make sure we duplicate stdout/stderr to the debuggee
    launch_info.AddDuplicateFileAction(STDOUT_FILENO, STDOUT_FILENO);
    launch_info.AddDuplicateFileAction(STDERR_FILENO, STDERR_FILENO);

    // launch target
    process = target.Launch(launch_info, error);
    if (!process.IsValid() || error.Fail()) {
        logger->Error("Launch failed for target '%s': %s", argv[0], error.GetCString());
        return false;
    }
    SuppressSignals();
    // At this point we are running...

    // yield - try for the debugger to kick in - other wise we might have a raise condition on our hands..
    std::this_thread::yield();

    // verify (or at least sanity check we have a PID)
    pid = process.GetProcessID();
    logger->Info("PID: %llu", pid);

    // Wait for the process to become stopped
    if (!WaitState(lldb::eStateStopped, 1000)) {
        logger->Error("Premature exit");
        return false;
    }

    // we have started - and we should be at 'main' now...  verify
    if (!(bpMain.GetHitCount() > 0)) {
        logger->Error("main breakpoint not hit");
        return false;
    }

    // Start again - we now know where we are - next stop is after TRUN has scanned all libraries
    process.Continue();

    if (!WaitState(lldb::eStateStopped, 1000)) {
        logger->Error("Premature exit waiting for TRUN to load libraries");
        return false;
    }

    if (!WasSignalRaised(sig_DYNLIB_LOADED)) {
        logger->Error("Signal missing for loading of dynamic libraries - out of sync");
        return false;
    }
    logger->Info("Libraries scanned - we are good to go...");


    // FIXME: Remove this - test code

    // We can do either one of these..
    //auto breakpoint = target.BreakpointCreateByLocation("dbgme.cpp", 6);
    EnumerateMembers(target, "CTestCoverage");

    // should have a list of these per symbol
    breakpoints = CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    if (breakpoints.size() == 0) {
        logger->Error("Coverage symbol information not resolved");
        return 1;
    }
    logger->Debug("Num Coverage BP's found = %zu", breakpoints.size());

    return true;
}

static void PrintHelpAndExit() {
    printf("tcov - testrunner coverage\n");
    printf("bla\n");
    exit(1);
}

bool CoverageRunner::ParseArgs(int argc, const char *argv[]) {
    for (int i=1;i<argc;i++) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "--help")) {
            PrintHelpAndExit();
        }
        if (arg == "--target") {
            targetPathName = argv[++i];
            goto nextargument;
        }
        // pass this on to trun
        trunArgsVector.push_back(arg);
nextargument:
        ;
    }
    // FIXME: Should point to install directory..
    if (targetPathName.empty()) {
        targetPathName = "./trun";    // '/usr/bin/trun'  - do we need an absolute path?
    }
    return true;
}

// After this the 'trunArgs' contains char * elements pointing to trunArgsVector
// calling trunArgs.Data() will give you a char ** which can be used to argv..

void CoverageRunner::ConvertArgs(std::vector<char *> &out, std::vector<std::string> &args) {
    out.reserve(args.size() + 1);
    for (auto &arg : args) {
        out.push_back(arg.data());
    }
    out.push_back(nullptr);
}
void CoverageRunner::ResolveCWD() {
    char cwd[PATH_MAX+1] = {};
    getcwd(cwd, PATH_MAX);
    workingDirectory = cwd;
}

bool CoverageRunner::CreateIPCServer() {
    // might want to do other things here...
    if (!ipcServer.Open()) {
        return false;
    }
    logger->Info("IPC Server at: %s", ipcServer.FifoName().c_str());
    return true;
}
void CoverageRunner::SuppressSignals() {
    // Try send 'SIGUSR1' when we have done 'dlopen' (no debug info needed)
    auto unixSignals = process.GetUnixSignals();
    unixSignals.SetShouldStop(SIGUSR1, true);
    unixSignals.SetShouldNotify(SIGUSR1, true);
    unixSignals.SetShouldSuppress(SIGUSR1, true);

    unixSignals.SetShouldStop(SIGUSR2, true);
    unixSignals.SetShouldNotify(SIGUSR2, true);
    unixSignals.SetShouldSuppress(SIGUSR2, true);

}

bool CoverageRunner::WaitState(lldb::StateType targetState, uint32_t timeoutMSec) {
    while (targetState != process.GetState()) {
        std::this_thread::yield();
        switch (process.GetState()) {
            case lldb::eStateCrashed :
            case lldb::eStateExited :
                return false;
            default :
                break;
        }
    }
    return true;
}
bool CoverageRunner::WasSignalRaised(int expectedSignal) {

    // Check that we really got SIGUSR1 - raised once dylibs have been loaded...
    auto thread = process.GetSelectedThread();
    if (thread.GetStopReason() != lldb::eStopReasonSignal) {
        return false;
    }
    uint32_t data_count = thread.GetStopReasonDataCount();
    if (data_count > 0) {
        int signo = thread.GetStopReasonDataAtIndex(0);
        logger->Debug("Stopped by signal: %d", signo);
        if (signo == expectedSignal) {
            return true;
        }
    }
    return false;
}

void CoverageRunner::Process() {
    logger->Debug("--> Entering Process Loop");
    // continue until we have exited
    while (true) {
        process.Continue();
        auto state = process.GetState();
        if ((state == lldb::eStateExited) || (state == lldb::eStateCrashed)) {
            logger->Debug("Process exited or crashed");
            break;
        }
        if (state == lldb::eStateStopped) {
            auto thread = process.GetSelectedThread();
            if (WasSignalRaised(sig_IPC_INTERRUPT)) {
                logger->Debug("Stopped by signal: %d", sig_IPC_INTERRUPT);
                HandleIPCInterrupt();
            } else {
                CheckBreakPointHit(thread);
            }
        }
    } // while(true)
    logger->Debug("--> Process Loop Complete");
    // get the exit code from the debuggee
    exit_code = process.GetExitStatus();
    logger->Debug("Target finished with exit code: %d", exit_code);

}

void CoverageRunner::CheckBreakPointHit(lldb::SBThread &thread) {
    auto frame = thread.GetSelectedFrame();
    auto line = frame.GetLineEntry();

    if (line.IsValid()) {
        auto filename = line.GetFileSpec().GetFilename();
        auto lineno = line.GetLine();
        auto pc = frame.GetPC();
        logger->Debug("Hit Breakpoint at %s:%u - PC=%llX", filename, lineno, pc);
    } else {
        logger->Warning("Breakpoint, but line info is bad!");
    }
}

void CoverageRunner::HandleIPCInterrupt() {
    ConsumeIPC();
}

void CoverageRunner::ConsumeIPC() {
    if (!ipcServer.Available()) {
        //printf("ConsumeFIFO: tcovIPCServer not available\n");
        logger->Debug("ConsumeIPC, No IPC data available");
        return;
    }
    printf("ConsumeIPC, data available..\n");
    while (ipcServer.Available()) {
        auto ipcMsg = ReadIPCMessage();
        if (!ipcMsg.IsValid()) {
            continue;;
        }
        // FIXME: handle stuff here...
        logger->Info("BeginCoverage for '%s'",ipcMsg.symbolName.c_str());
    }
}

trun::CovIPCCmdMsg CoverageRunner::ReadIPCMessage() {
    trun::CovIPCCmdMsg ipcMsg;
    gnilk::IPCBinaryDecoder decoder(ipcServer, ipcMsg);
    if (!decoder.Process()) {
        return {};
    }
    return ipcMsg;
}

void CoverageRunner::End() {
    // Close IPC connection
    ipcServer.Close();
    // terminate everything..
    lldb::SBDebugger::Terminate();
}

void CoverageRunner::Report() {
    // TEMPORARY
    size_t linestested = 0;
    for (auto &bp : breakpoints) {

        printf("%llX - %s:%d:%d\n",bp.address, bp.function.c_str(), bp.line, bp.breakpoint.GetHitCount());
        if (bp.breakpoint.GetHitCount() > 0) {
            linestested++;
        }
    }
    auto factor = (float)linestested / (float)breakpoints.size();
    printf("coverage=%f = %d%%\n", factor, (int)(100 * factor));
}

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


// Example of 'debugging' a file programmatically with LLDB...
// and capturing the output of the debuggee...
static int oldmain(int argc, const char *argv[]) {
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