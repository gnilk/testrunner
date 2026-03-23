//
// Created by gnilk on 06.02.2026.
//
#include <stdio.h>
#include <unistd.h>

#include "Coverage.h"
#include "CoverageIPCMessages.h"
#include "strutil.h"
#include "ipc/IPCDecoder.h"
#ifdef APPLE
#include <lldb/SBUnixSignals.h>
#include <lldb/SBThread.h>
#include <lldb/SBBreakpointLocation.h>
#include <lldb/API/SBCommandInterpreter.h>
#include <lldb/API/SBCommandReturnObject.h>
#else
#include <lldb/API/SBUnixSignals.h>
#include <lldb/API/SBThread.h>
#include <lldb/API/SBBreakpointLocation.h>
#include <lldb/API/SBStringList.h>
#include <lldb/API/SBCommandInterpreter.h>
#include <lldb/API/SBCommandReturnObject.h>
#endif

#include "timer.h"
#include "reporting/ReportConsole.h"
#include "reporting/ReportLCOV.h"
#include "reporting/ReportDiff.h"
#include "Config.h"
#include "unordered_map"

using namespace tcov;


// Initialize the session
// this is quite a lengthy function and could be split
bool CoverageRunner::Begin() {
    logger = gnilk::Logger::GetLogger("CoverageRunner");

    // Let's bring up the IPC - which handles communication between TRUN and TCOV
    if (!CreateIPCServer()) {
        logger->Error("Unable to create IPC Server");
        return false;
    }

    // Add the following so TRUN knows we are running in coverage mode and how to communicate back...
    std::vector <std::string> trunArgsVectorInternal;
    if (std::find_if(Config::Instance().target_args.begin(), Config::Instance().target_args.end(), [](auto &v) -> bool { return (v == "--sequential"); }) == Config::Instance().target_args.end()) {
        logger->Info("Adding '--sequential' to target arguments");
        trunArgsVectorInternal.push_back("--sequential");
    }
    trunArgsVectorInternal.push_back("--coverage");
    trunArgsVectorInternal.push_back("--tcov-ipc-name");
    trunArgsVectorInternal.push_back(ipcServer.FifoName());
    Config::Instance().target_args.insert(Config::Instance().target_args.begin(), trunArgsVectorInternal.begin(), trunArgsVectorInternal.end());


    // FIXME: Better resolver here!
    // Create our target
    if (Config::Instance().target.empty()) {
        // FIXME: Only in debug builds...
#ifdef DEBUG
        logger->Info("Debug build - setting to internal dev-path");
        Config::Instance().target = "/Users/gnilk/src/github.com/testrunner/cmake-build-debug/trun";
#endif
    }

    ResolveCWD();

    std::string dbg_argString;
    for (auto &arg : Config::Instance().target_args) {
        dbg_argString += arg + " ";
    }
    logger->Info("Target: %s %s", Config::Instance().target.c_str(), dbg_argString.c_str());
    logger->Info("Working Directory: %s", workingDirectory.c_str());

    // Startup the LLDB Debugger Process...
    if (!StartLLDBDebugger()) {
        return false;
    }

    // verify (or at least sanity check we have a PID)
    pid = process.GetProcessID();
    logger->Info("PID: %llu", pid);

    if (!RunInitialLLDBPhase()) {
        return false;
    }

    // We now have everything we need to set breakpoint for the symbols we want to monitor
    logger->Info("Libraries scanned - we are good to go...");
    // Create breakpoints from symbols coming from cmd-line..
    for (auto &s : Config::Instance().symbols) {
        breakpointManager.CreateCoverageBreakpoints(target, s);
    }
    return true;
}

//
// Kicks of the LLDB process and set's it up properly
// ASYNC = false (we really don't want to have an event loop to worry about)
// Supress stdout/stderr for LLDB
// Create breakpoints for main (this symbol is ALWAYS present) of TRUN
// Redirect stdout/stderr for TRUN so we get them
// Supress signals from TRUN to LLDB - this allows us to trap on them...
//
bool CoverageRunner::StartLLDBDebugger() {
    // Initialize LLDB
    lldb::SBDebugger::Initialize();

    // Create the debugger
    lldbDebugger = lldb::SBDebugger::Create(false);
    logger->Info("LLDB Debugger Version: %s\n",lldbDebugger.GetVersionString());

    // Remap stdout/stderr
    // FIXME: check if we can redirect to logger instead
    lldbDebugger.SetAsync(false);       // lldb is async by default - which is good, but complicated to start with
    lldbDebugger.SetOutputFileHandle(stdout, false);
    lldbDebugger.SetErrorFileHandle(stderr, false);
    lldb::SBError error;

    target = lldbDebugger.CreateTarget(Config::Instance().target.c_str(), nullptr, nullptr, true, error);
    if (!target.IsValid()) {
        logger->Error("Invalid target '%s'", Config::Instance().target.c_str());
        return false;
    }

    // LLDB starts the program in 'running' mode - so we create this breakpoint to control execution
    // 'main' is present even in release builds, other options would be '_start' or
    // Make sure we control launch - lldb starts in running mode by default - so we insert this..
    bpMain = target.BreakpointCreateByName("main");

    // Convert trunArgs to argv style char *[]
    std::vector<char *> trunArgs;
    ConvertArgs(trunArgs, Config::Instance().target_args);

    //const char *target_argv[]={"--sequential", "--coverage","--tcov-ipc-name",tcovIPCServer.FifoName().c_str(),"-vvv", "-m", "coverage", nullptr};
    lldb::SBLaunchInfo launch_info(const_cast<const char **>(trunArgs.data()));
    launch_info.SetWorkingDirectory(workingDirectory.c_str());   // se should be here and not where the target is located

#ifdef APPLE
    // Make sure we duplicate stdout/stderr to the debuggee
    launch_info.AddDuplicateFileAction(STDOUT_FILENO, STDOUT_FILENO);
    launch_info.AddDuplicateFileAction(STDERR_FILENO, STDERR_FILENO);
#endif
    // launch target
    process = target.Launch(launch_info, error);
    if (!process.IsValid() || error.Fail()) {
        logger->Error("Launch failed for target '%s': %s", Config::Instance().target.c_str(), error.GetCString());
        return false;
    }
    SuppressSignals();
    // At this point we are running...

    // yield - try for the debugger to kick in - other wise we might have a raise condition on our hands..
    std::this_thread::yield();
    return true;
}

// This will run until 'sig_DYNLIB_LOADED' and we know that the testrunner has scanned
// all dynlibs and have all symbols...
// Basically - we trap the 'main' breakpoint, then we continue until we get the 'sig_DYNLIB_LOADED' signal
// this is raised after TRUN has loaded all dynlibs - thus they are in the debuggee process memory and we
// can create breakpoints according to our coverage symbol list...
bool CoverageRunner::RunInitialLLDBPhase() {
    // Wait for the process to become stopped
    // The timeouts here are set very high due to caching on the initial run (from the OS)
    // Second run normally just takes a couple of 100ms...
    if (!WaitState(lldb::eStateStopped, 10000)) {
        logger->Error("Premature exit");
        return false;
    }

    ConsumeProcessOutput();

    // we have started - and we should be at 'main' now...  verify
    if (!(bpMain.GetHitCount() > 0)) {
        logger->Error("main breakpoint not hit");
        return false;
    }

    // Start again - we now know where we are - next stop is after TRUN has scanned all libraries
    process.Continue();

    if (!WaitState(lldb::eStateStopped, 10000)) {
        logger->Error("Premature exit waiting for TRUN to load libraries");
        return false;
    }

    ConsumeProcessOutput();

    if (!WasSignalRaised(sig_DYNLIB_LOADED)) {
        logger->Error("Signal missing for loading of dynamic libraries - out of sync");
        return false;
    }
    return true;
}

// Internal - when debugging this is a helper that allows us to debug into LLDB - not needed
// for debugging the CoverageRunner
bool CoverageRunner::EnableSelfDebugging() {
    // this little scripts enables internal logging for lldb - good when debugging API usage

    lldb::SBCommandInterpreter interp = lldbDebugger.GetCommandInterpreter();
    lldb::SBCommandReturnObject result;

    logger->Info("Trying to enable self-debugging");

    interp.HandleCommand(
        //"settings set plugin.process.gdb-remote.server-path /usr/lib/llvm-18/bin/lldb-server",
        //"log enable lldb gdb-remote process platform host",
        "log enable gdb-remote all",
        result
    );

    if (!result.Succeeded()) {
        logger->Error("Failed 'enable gdb-remote all', result=%s", result.GetError());
        return false;
    }
    interp.HandleCommand(
        //"settings set plugin.process.gdb-remote.server-path /usr/lib/llvm-18/bin/lldb-server",
        //"log enable lldb gdb-remote process platform host",
        "log enable lldb process platform host api",
        result
    );

    if (!result.Succeeded()) {
        logger->Error("Failed 'enable lldb process platform host api', result=%s", result.GetError());
        return false;
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

// On Linux this consumes the process output and sends it to print/sanitization
// on macos this is not needed - redirection works fine - but on Linux it stopped working..
// I have no clue why it stopped - even after reverting to an earlier working version (before starting cleanup) it
// was still broken - I could never find a version where it worked again - no clue...
void CoverageRunner::ConsumeProcessOutput() {
#ifdef LINUX
    char buffer[1024];
    size_t nBytes;
    while ((nBytes = process.GetSTDOUT(buffer, sizeof(buffer))) > 0) {
        SanitizeAndPrint(stdout, buffer, nBytes);
    }
    while ((nBytes = process.GetSTDERR(buffer, sizeof(buffer))) > 0) {
        SanitizeAndPrint(stderr, buffer, nBytes);
    }
    fflush(stdout);
    fflush(stderr);
#endif
}

// This will sanitize the debuggee output and write to our stdout
void CoverageRunner::SanitizeAndPrint(FILE *out, const char *buffer, size_t nBytes) {
    // Ok, will be initialized the first time
    static std::string tabstr(Config::Instance().tab_size, ' ');

    for (size_t i = 0; i < nBytes; i++) {
        unsigned char c = buffer[i];
        if (c == '\n') {
            fprintf(out, "\n");
        } else if (c == '\r') {
            // ignore \r
        } else if (c == '\t') {
            // Wow - this is ugly
            fprintf(out, "%s", tabstr.c_str());
        } else if (c < 32 || c > 126) {
            // sanitize non-printable
            fprintf(out, ".");
        } else {
            fprintf(out, "%c", c);
        }
    }
}

//
// Wait for a specific target state to happen in the lldb context
// if the target process has crashed or exited we return false..
//
bool CoverageRunner::WaitState(lldb::StateType targetState, uint32_t timeoutMSec) {
    trun::Timer timer;
    timer.Reset();
    while (targetState != process.GetState()) {
        std::this_thread::yield();
        switch (process.GetState()) {
            case lldb::eStateCrashed :
                logger->Error("lldb Crashed (state=%d)",(int)process.GetState());
                return false;
            case lldb::eStateExited :
                logger->Error("lldb Exited (state=%d)",(int)process.GetState());
                // logger->Error("lldb Exit Description: %s", process.GetExitDescription());
                return false;
            default :
                break;
        }
        if (timer.Sample() > (timeoutMSec / 1000.0)) {
            logger->Error("lldb wait state timed out");
            return false;
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
    size_t nHits = 0;
    while (true) {
        process.Continue();

        ConsumeProcessOutput();

        auto state = process.GetState();
        if ((state == lldb::eStateExited) || (state == lldb::eStateCrashed)) {
            logger->Debug("Process exited or crashed");
            break;
        } else if (state == lldb::eStateStopped) {
            auto thread = process.GetSelectedThread();
            auto stopReason = thread.GetStopReason();
            if ((stopReason == lldb::eStopReasonSignal) && WasSignalRaised(sig_IPC_INTERRUPT)) {
                HandleIPCInterrupt();
            } else if (stopReason == lldb::eStopReasonBreakpoint) {
                CheckBreakPointHit(thread);
                nHits++;
            } else {
                logger->Error("Unhandled stop reson = %d\n", (int)stopReason);
            }
        }
        //logger->Debug("Number of hits: %d", nHits);
        std::this_thread::yield();
    } // while(true)
    logger->Debug("--> Process Loop Complete");

    // get the exit code from the debuggee
    exit_code = process.GetExitStatus();
    logger->Debug("Target finished with exit code: %d", exit_code);

}

void CoverageRunner::CheckBreakPointHit(lldb::SBThread &thread) {
    auto bp_id = thread.GetStopReasonDataAtIndex(0);
    auto loc_id = thread.GetStopReasonDataAtIndex(1);

    // FIXME: disable location only as a breakpoint can have multiple locations in case of inlining
    //        another option is to use 'loc.SetOneShot(true)'
    auto bp = target.FindBreakpointByID(bp_id);
    if (!bp.IsValid()) {
        logger->Error("Invalid breakpoint with ID: %lld", bp_id);
        return;
    }
    // FIXME: According to ChatGPT we can remove this and just disble the location
    bp.SetEnabled(false);
    auto loc = bp.FindLocationByID(loc_id);
    if (!loc.IsValid()) {
        logger->Error("Invalid breakpoint location with ID: %lld", loc_id);
    }
    loc.SetEnabled(false);
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
        // Do I really need to do this???
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
using ReportFactory = std::function<ReportBase *()>;

static std::unordered_map<std::string, ReportFactory> reportMap = {
    {"lcov", []{ return new ReportLCOV(); } },
    {"base", []{ return new ReportConsole(); } },
    {"diff", []{ return new ReportDiff(); } },
};
static ReportBase *CreateReportEngine(const std::string &reportEngine) {
    if (!reportMap.contains(reportEngine)) {
        return nullptr;
    }
    return reportMap[reportEngine]();
}
void CoverageRunner::Report(double durationSec) {
    //ReportConsole reportConsole;

    // This 'line' diffrentiates from trun which is using '----'
    printf("===================\n");
    printf("Coverage Report\n");
    printf("Duration......: %.3f sec\n", durationSec);

    for (auto &reportEngineName : Config::Instance().reportEngines) {
        auto reportEngine = CreateReportEngine(reportEngineName);
        if (reportEngine == nullptr) {
            fprintf(stderr, "Invalid report engine '%s'\n", reportEngineName.c_str());
            return;
        }
        reportEngine->GenerateReport(breakpointManager);

        // not really needed, we quite the process directly after the report has been printed...
        delete reportEngine;
    }

}
