/*-------------------------------------------------------------------------
 File    : moduleexecutors.cpp
 Author  : FKling
 Version : -
 Orginal : 2024-05-02
 Descr   : Implementation of different execution 'strategies' for modules

 Part of testrunner
 BSD3 License!

 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TO-DO: [ -:Not done, +:In progress, !:Completed]
 <pre>
   ! TextExecutor using 'fork' that spawns the test-runner (hope to solve problem with parallel modules execution causing crashes)
 </pre>

 \History
 - 2024.05.02, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "testrunner.h"
#include "../shared/strutil.h"
#include "moduleexecutors.h"
#include "resultsummary.h"
#include "testfunc.h"
#include <assert.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include "std_backport.h"

#ifndef WIN32
    #ifdef TRUN_HAVE_FORK
        #include "unix/process.h"
        #include "subprocess.h"
        #include "unix/IPCFifoUnix.h"
        #include "IPCMessages.h"
#include "ipc/IPCDecoder.h"
    #endif
#endif

#ifdef WIN32
    #include <process.h>
#endif

// std::this_thread::sleep_for() + hardware_concurrency() in the fork wait-loop
// need <thread>; the fork path is the only user.
#ifdef TRUN_HAVE_FORK
#include <thread>
#endif

using namespace trun;

TestModuleExecutorBase &TestModuleExecutorFactory::Create() {
    static TestModuleExecutorSequential sequentialExecutor;
#ifdef TRUN_HAVE_FORK
    static TestModuleExecutorFork forkExecutor;
#endif

    switch(Config::Instance().moduleExecuteType) {
        case ModuleExecutionType::kSequential :
            return sequentialExecutor;
#ifdef TRUN_HAVE_FORK
        case ModuleExecutionType::kParallel :
            return forkExecutor;
#endif
        default:
            printf("Unknown or unsupported execution protocol, using default\n");
    }
    return sequentialExecutor;
}


enum class kMatchResult {
    List,
    Single,
    NegativeSingle,
};
static kMatchResult ModuleMatch(std::vector<TestModule::Ref> &outMatches, const std::string &tcPattern, const std::vector<TestModule::Ref> &caseList) {
    kMatchResult result = kMatchResult::List;
    for (auto &module: caseList) {
        if (tcPattern == "-") {
            outMatches.push_back(module);
            continue;
        }
        if (tcPattern[0]=='!') {
            auto negTC = tcPattern.substr(1);
            auto isMatch = trun::match(module->name, negTC);
            if (isMatch) {
                // not sure...
                //executeFlag = 0;
                outMatches.push_back(module);
                result = kMatchResult::NegativeSingle;
                goto leave;
            }
        } else {
            auto isMatch = trun::match(module->name, tcPattern);
            if (isMatch) {
                outMatches.push_back(module);
                result = kMatchResult::Single;
                goto leave;
            }
        }
    }
    leave:
    return result;
}



bool TestModuleExecutorSequential::Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) {
    bool bRes = true;
    pLogger = gnilk::Logger::GetLogger("TestModExeSeq");

    // Convert to vector and sort..
    std::vector<TestModule::Ref> testModulesList;
    for(auto &[k,v] : testModules) {
        testModulesList.push_back(v);
    }
    std::sort(testModulesList.begin(), testModulesList.end(), [](const TestModule::Ref &a, const TestModule::Ref &b) {
        return (a->name < b->name);
    });


    // For every argument on the command line
    for(auto argModuleName : Config::Instance().modules) {
        // Match cases
        std::vector<TestModule::Ref> matches;
        auto matchResult = ModuleMatch(matches, argModuleName, testModulesList);
        // In case this is negative (i.e. don't execute) we remove it from the sorted LOCAL list
        // NOTE: DO NOT change the state - if can be that another module depends on this one - in that case we need to execute...
        if (matchResult == kMatchResult::NegativeSingle) {
            assert(matches.size() == 1);
            auto tmToRemove = matches[0];
            // Remove this from the execution list...
            std::erase_if(testModulesList,[tmToRemove](const TestModule::Ref &m)->bool{
               return (tmToRemove->name == m->name);
            });
            continue;
        }

        // If here, we should execute anything in the matches list...
        for(auto &testModule : matches) {
            // Already executed?
            if (!testModule->IsIdle()) {
                //pLogger->Debug("Tests for '%s' already executed, skipping",testModule->name.c_str());
                continue;
            }

            pLogger->Info("Executing tests for library: %s", testModule->name.c_str());

            TestRunner::SetCurrentTestModule(testModule);
            auto result = testModule->Execute(library);
            if ((result != nullptr) && (result->CheckIfContinue() == TestResult::kRunResultAction::kAbortAll)) {
                break;
            }
            TestRunner::SetCurrentTestModule(nullptr);
        } // for modules

    }

    return bRes;
}

#ifdef TRUN_HAVE_FORK



// FIXME: Probably implement as a proper class - we need more features on this object than just holding some data
// To-Do
// + Fix IPC between forked TRUN and other
//   !Transmission solution (i.e. pipe/fifo/shm/other?)
//      ! FIFO; has name
//   - Serialization, do we need it? => YES
//   - one pipe per fork or multiplex (how to pass the FD?)
// ! Make Forking the main module-parallel technique
// ! in Config, remove the various booleans and create enums for module/test execution selection




bool TestModuleExecutorFork::Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) {
    pLogger = gnilk::Logger::GetLogger("TestModExeFork");
    pLogger->Debug("Forking module tests");

    gnilk::IPCFifoUnix ipcServer;
    if (!ipcServer.Open()) {
        pLogger->Error("Unable to create IPC server!");
        return false;
    }

    // Drain whatever results the children have written so far. Called repeatedly
    // during the run (not only at the end): with a bounded window the run is
    // long-lived, and unread results could back up the FIFO and stall a writer.
    auto drainResults = [&ipcServer]() {
        while (ipcServer.Available()) {
            IPCResultSummary summary;
            gnilk::IPCBinaryDecoder decoder(ipcServer, summary);
            if (!decoder.Process()) {
                continue;
            }
            for (auto &ipcTestResult : summary.testResults) {
                // We need to create a fake test-func here..
                auto tfuncWrapper = TestRunner::CreateTestFunc(ipcTestResult->symbolName);
                tfuncWrapper->SetResultFromSubProcess(ipcTestResult->testResult);
                ResultSummary::Instance().AddResult(tfuncWrapper);
            }
        }
    };

    // Deterministic list of modules to run (std::map iterates name-sorted).
    std::vector<TestModule::Ref> pending;
    for (auto &[name, module] : testModules) {
        if (!module->ShouldExecute()) {
            // Skip, this is not part of the configured filter..
            continue;
        }
        if (!module->IsIdle()) {
            // Already executed (e.g. pulled in as a dependency)
            continue;
        }
        pending.push_back(module);
    }

    // Bound how many module subprocesses run at once. Each child re-runs the
    // (heavy) global test_main; forking one per module all at once oversubscribes
    // the machine and makes every process cross the timeout deadline together.
    unsigned int maxConcurrency = Config::Instance().moduleExecConcurrency;
    if (maxConcurrency == 0) {
        maxConcurrency = std::max(1u, std::thread::hardware_concurrency());
    }
    const long timeoutSec = Config::Instance().moduleExecTimeoutSec;

    // Only live processes; reaped ones are erased. 'timedOut' makes the timeout
    // log/kill/record happen exactly once per process.
    struct Running {
        std::unique_ptr<SubProcess> proc;
        bool timedOut = false;
    };
    std::vector<Running> inflight;
    size_t nextIdx = 0;
    size_t started = 0;

    pLogger->Debug("Running %zu modules, max %u concurrent", pending.size(), maxConcurrency);

    while ((nextIdx < pending.size()) || !inflight.empty()) {
        // Top up the window with the next pending modules.
        while ((inflight.size() < maxConcurrency) && (nextIdx < pending.size())) {
            auto &module = pending[nextIdx++];
            auto process = std::make_unique<SubProcess>();
            started++;
            printf("Starting module tests '%s' (%zu / %zu)\n", module->name.c_str(), started, pending.size());
            process->Start(library, module, ipcServer.FifoName());
            inflight.push_back({std::move(process), false});
        }

        // Poll the window: reap finished, time out runaways (once each).
        for (auto it = inflight.begin(); it != inflight.end(); ) {
            auto &p = it->proc;

            // Stop a runaway module once it exceeds the timeout (0 == infinity).
            // Log/kill/record exactly once; the killed process reaches kFinished
            // on a later spin when its worker observes the SIGKILL.
            if (!it->timedOut && (timeoutSec > 0) && (p->Duration() > timeoutSec)) {
                pLogger->Error("Process for '%s' timed out after %lds, killing", p->Name().c_str(), timeoutSec);
                p->Kill();
                it->timedOut = true;
                ResultSummary::Instance().AddIncompleteModule(p->Name(), "timed out");
            }

            if (p->State() != SubProcessState::kFinished) {
                ++it;
                continue;
            }

            // Finished: join the worker (publishes its writes) then handle once.
            p->Wait();
            if (it->timedOut) {
                // Already logged + recorded; drop the partial output.
            } else if (p->GetExitStatus() == ProcessExitStatus::kNormal) {
                for (auto &s : p->Strings()) {
                    printf("%s", s.c_str());
                }
            } else {
                pLogger->Error("Process for '%s' crashed (abnormal exit)", p->Name().c_str());
                ResultSummary::Instance().AddIncompleteModule(p->Name(), "crashed");
            }
            it = inflight.erase(it);
        }

        drainResults();
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // Final drain for results written between the last poll and the last reap.
    drainResults();
    ipcServer.Close();

    return true;
}
#endif