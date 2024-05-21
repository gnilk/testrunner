//
// Created by gnilk on 20.05.2024.
//

#ifndef TESTRUNNER_SUBPROCESS_H
#define TESTRUNNER_SUBPROCESS_H

#include <stdint.h>

#include <string>
#include <vector>
#include <chrono>

#include "dynlib.h"
#include "testmodule.h"
#include "process.h"

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
        virtual ~SubProcess() = default;

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
        bool WasProcessExecOk() const {
            return wasProcessExecOk;
        }
        const std::vector<std::string> &Strings() {
            return dataHandler.Data();
        }
    protected:
        Process *proc = {};
        std::string name = {};
        SubProcessDataHandler dataHandler;
        std::thread thread;
        TestModule::Ref module;

        pclock::time_point tStart;
        SubProcessState state = SubProcessState::kIdle;

        bool wasProcessExecOk = false;
    };

}

#endif //TESTRUNNER_SUBPROCESS_H
