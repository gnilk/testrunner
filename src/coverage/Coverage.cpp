//
// Created by gnilk on 06.02.2026.
//
#include <stdio.h>
#include <unistd.h>

#include "Coverage.h"
#include "CoverageIPCMessages.h"
#include "strutil.h"
#include "ipc/IPCDecoder.h"
#include <lldb/SBUnixSignals.h>
#include <lldb/SBThread.h>


using namespace tcov;


// Initialize the session
// this is quite a lengthy function and could be split
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

    // Create breakpoint from symbols...
    for (auto &s : symbols) {
        breakpointManager.CreateCoverageBreakpoints(target, s);
    }


    // FIXME: Remove this - test code

    // We can do either one of these..
    //auto breakpoint = target.BreakpointCreateByLocation("dbgme.cpp", 6);
    //EnumerateMembers(target, "CTestCoverage");

    // should have a list of these per symbol
    // breakpointManager.CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    // breakpoints = CreateCoverageBreakpoints(target, "CTestCoverage::SomeFunc");
    // if (breakpoints.size() == 0) {
    //     logger->Error("Coverage symbol information not resolved");
    //     return 1;
    // }
    // logger->Debug("Num Coverage BP's found = %zu", breakpoints.size());

    return true;
}

static void PrintHelpAndExit() {
    printf("tcov - testrunner coverage\n");
    printf("usage:  tcov [options] [trun options]\n");
    printf("options:\n");
    printf("-h/-H/--help             This cruft...\n");
    printf("--target <path to trun>  Specify where trun is (unless it's in the path)\n");
    printf("--symbols <list>         List of debugging symbols to track for coverage\n");
    printf("You can use the 'symbols' argument if you don't fancy changing your unit-tests\n");
    printf("Any other argument is passed on to the target, while any known argument is removed\n");
    exit(1);
}

// Parse arguments, capture the ones belonging to us pass the rest to the target (trun)
bool CoverageRunner::ParseArgs(int argc, const char *argv[]) {
    bool bSequentialFound = false;
    for (int i=1;i<argc;i++) {
        std::string arg = argv[i];
        if ((arg == "-h") || (arg == "-H") || (arg == "--help")) {
            PrintHelpAndExit();
        }
        if (arg == "--target") {
            targetPathName = argv[++i];
            goto nextargument;
        } else if (arg == "--symbols") {
            auto symbolsString = argv[++i];
            trun::split(symbols, symbolsString, ',');
            goto nextargument;
        } else if (arg == "--sequential") {
            bSequentialFound = true;
        }
        // otherwise, just pass this on to trun
        trunArgsVector.push_back(arg);
nextargument:
        ;   // Note: we need this as labels are attached to statements
    }
    // Verify if this is actually needed..
    if (!bSequentialFound) {
        logger->Info("'--sequential' missing from trun args, adding even though not specified...\n");
        trunArgsVector.push_back("--sequential");
    }
    // FIXME: Should point to install directory..
    if (targetPathName.empty()) {
        targetPathName = "./trun";    // '/usr/bin/trun'  - do we need an absolute path?
    }
    return true;
}

//
// Convert a vector of std::string to vector of char *
// as this is continous memory the 'data' member of vector is a char **
// the data is only alive as long as the original std::string vector is alive
//
void CoverageRunner::ConvertArgs(std::vector<char *> &out, std::vector<std::string> &args) {
    out.reserve(args.size() + 1);
    for (auto &arg : args) {
        out.push_back(arg.data());
    }
    out.push_back(nullptr);
}

// fills in the class member 'workingDirectory' (will be different on Windows)
void CoverageRunner::ResolveCWD() {
    char cwd[PATH_MAX+1] = {};
    getcwd(cwd, PATH_MAX);
    workingDirectory = cwd;
}

// Opens the IPC server
// the server address is defined by the IPC class
// as '/tmp/testrunner_pid
bool CoverageRunner::CreateIPCServer() {
    // might want to do other things here...
    if (!ipcServer.Open()) {
        return false;
    }
    logger->Info("IPC Server at: %s", ipcServer.FifoName().c_str());
    return true;
}

// Configure signals handling within lldb
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

//
// Wait for a specific target state to happen in the lldb context
// if the target process has crashed or exited we return false..
// FIXME: impelement timeout
//
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

//
// Check if a specific signal was raised from the target
//
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

// Process - this assumes we are all setup and ready to run
// In case the debuggee tries to talk to us they have to raise the IPC_INTERRUPT signal
// if that happens we reach out and try to read the IPC and act
// during this time the lldb processing is halted (as async is false)
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

// Might extend this in the future - but currently we just process IPC
void CoverageRunner::HandleIPCInterrupt() {
    ConsumeIPC();
}

// Consume the IPC data - this will empty the pipe
void CoverageRunner::ConsumeIPC() {
    if (!ipcServer.Available()) {
        //printf("ConsumeFIFO: tcovIPCServer not available\n");
        logger->Debug("ConsumeIPC, No IPC data available");
        return;
    }
    logger->Debug("ConsumeIPC, data available..");
    while (ipcServer.Available()) {
        auto ipcMsg = ReadIPCMessage();
        if (!ipcMsg.IsValid()) {
            continue;
        }
        logger->Debug("BeginCoverage for '%s'",ipcMsg.symbolName.c_str());
        breakpointManager.CreateCoverageBreakpoints(target, ipcMsg.symbolName);
    }
}

// Read a single IPC message
trun::CovIPCCmdMsg CoverageRunner::ReadIPCMessage() {
    trun::CovIPCCmdMsg ipcMsg;
    gnilk::IPCBinaryDecoder decoder(ipcServer, ipcMsg);
    if (!decoder.Process()) {
        return {};
    }
    return ipcMsg;
}

// End the processing, note: we need to kill the IPC before terminating the lldb
// otherwise the IPC connection will stay open
void CoverageRunner::End() {
    // Close IPC connection
    ipcServer.Close();
    // terminate everything..
    lldb::SBDebugger::Terminate();
}

// Generate the coverage report
// FIXME: extend this - we should drop a file with all details
void CoverageRunner::Report() {
    breakpointManager.Report();
    // // TEMPORARY
    // size_t linestested = 0;
    // for (auto &bp : breakpoints) {
    //
    //     printf("%llX - %s:%d:%d\n",bp.address, bp.function.c_str(), bp.line, bp.breakpoint.GetHitCount());
    //     if (bp.breakpoint.GetHitCount() > 0) {
    //         linestested++;
    //     }
    // }
    // auto factor = (float)linestested / (float)breakpoints.size();
    // printf("coverage=%f = %d%%\n", factor, (int)(100 * factor));
}
