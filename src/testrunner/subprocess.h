//
// Created by gnilk on 20.05.2024.
//

#ifndef TESTRUNNER_SUBPROCESS_H
#define TESTRUNNER_SUBPROCESS_H

#include <stdint.h>

#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>

#include "dynlib.h"
#include "testmodule.h"
#include "../shared/procspawn.h"

namespace trun {
    // shortcut for process clock
    using pclock = std::chrono::system_clock;


    enum class SubProcessState {
        kIdle,
        kRunning,
        kFinished,
    };

    class SubProcessDataHandler : public ProcessCallbackBase {
    public:
        SubProcessDataHandler() = default;
        virtual ~SubProcessDataHandler() = default;
        void OnStdOutData(std::string data) {
            strings.push_back(data);
            //printf("%s", data.c_str());
        }
        void OnStdErrData(std::string data) {
            strings.push_back(data);
        }
        const std::vector<std::string> &Data() {
            return strings;
        }
    protected:
        std::vector<std::string> strings;

    };


    class SubProcess {
    public:
        SubProcess() = default;
        virtual ~SubProcess();

        void Start(const IDynLibrary::Ref &library, TestModule::Ref useModule, const std::string &ipcName);
        void Wait();
        void Kill();

        const std::string &Name() const {
            return name;
        }
        SubProcessState State() const {
            return state;
        }
        const pclock::time_point StartTime() const {
            return tStart;
        }
        long Duration() const {
            return std::chrono::duration_cast<std::chrono::seconds>(pclock::now() - tStart).count();
        }
        ProcessExitStatus GetExitStatus() const {
            return exitStatus;
        }
        const std::vector<std::string> &Strings() {
            return dataHandler.Data();
        }
    protected:
        std::unique_ptr<Process> proc;
        std::string name = {};
        SubProcessDataHandler dataHandler;
        std::thread thread;
        TestModule::Ref module;

        pclock::time_point tStart;
        // state and exitStatus are written by the worker thread and read by the
        // parent poll loop -> must be atomic. exitStatus needs a defined value
        // before the worker has produced one.
        std::atomic<SubProcessState> state = SubProcessState::kIdle;
        std::atomic<ProcessExitStatus> exitStatus = ProcessExitStatus::kUnknown;
        bool wasProcessExecOk = false;
    };

}

#endif //TESTRUNNER_SUBPROCESS_H
