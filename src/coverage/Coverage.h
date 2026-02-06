//
// Created by gnilk on 06.02.2026.
//

#ifndef TESTRUNNER_COVERAGE_H
#define TESTRUNNER_COVERAGE_H

#include <signal.h>
#include <stdint.h>
#include <vector>
#include <string>
#include <lldb/SBTarget.h>
#include <lldb/SBDebugger.h>
#include <lldb/SBBreakpoint.h>
#include <lldb/SBProcess.h>

#include "logger.h"
#include "unix/IPCFifoUnix.h"
#include "Breakpoint.h"
#include "CoverageIPCMessages.h"

namespace tcov {
    // Coverage runner - responsible for executing trun within lldb
    // and setup breakpoint to monitor all lines of code being used
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

        BreakpointManager breakpointManager;

        // LLDB API variables
        lldb::SBDebugger lldbDebugger;
        lldb::SBTarget target;
        lldb::SBBreakpoint bpMain;
        lldb::SBProcess process;
        pid_t pid;
        int exit_code;
    };

}

#endif //TESTRUNNER_COVERAGE_H