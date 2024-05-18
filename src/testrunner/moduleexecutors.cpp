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
   - TextExecutor using 'fork' that spawns the test-runner (hope to solve problem with parallel modules execution causing crashes)
 </pre>

 \History
 - 2024.05.02, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "testrunner.h"
#include "strutil.h"
#include "moduleexecutors.h"
#include <assert.h>
#include <vector>
#include <chrono>

#ifdef TRUN_HAVE_THREADS
    #include <thread>
#endif
#ifdef TRUN_HAVE_FORK
#include "unix/process.h"
#endif


using namespace trun;

TestModuleExecutorBase &TestModuleExecutorFactory::Create() {
    static TestModuleExecutorSequential sequentialExecutor;

#ifdef TRUN_HAVE_THREADS
    if (Config::Instance().enableParallelTestExecution && !Config::Instance().useForkForModuleParallelExec) {
        static TestModuleExecutorParallel parallelExecutor;
        return parallelExecutor;
    }
#endif
#ifdef TRUN_HAVE_FORK
if (Config::Instance().enableParallelTestExecution && Config::Instance().useForkForModuleParallelExec) {
        static TestModuleExecutorFork forkExecutor;
        return forkExecutor;
    }
#endif
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

#ifdef TRUN_HAVE_THREADS
bool TestModuleExecutorParallel::Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) {
    //
    // 3) all modules, executing according to cmd line library specification
    //
    pLogger = gnilk::Logger::GetLogger("TestModExePar");

    bool bRes = true;
    std::vector<std::thread> threads;

    auto currentTestRunner = TestRunner::GetCurrentRunner();

    for (auto &[name, testModule] : testModules) {

        if (!testModule->ShouldExecute()) {
            // Skip, this is not part of the configured filtered..
            continue;
        }
        // Already executed?
        if (!testModule->IsIdle()) {
            //pLogger->Debug("Tests for '%s' already executed, skipping",testModule->name.c_str());
            continue;
        }

        pLogger->Info("Executing tests for library: %s", testModule->name.c_str());

        //
        // Note: G++ allows capturing of structured binding but CLang not - and the standard prohibits it
        //       But we can use init-bindings (initialize a local variable explicitly during in the capture clause)
        //       and there we are allowed to use the structured binding...  and I am not versed enough to understand
        //       the exact problem why this is prohibited...
        //
        auto thread = std::thread([&library, &currentTestRunner, &tmpModule = testModule] {
            TestRunner::SetCurrentTestRunner(currentTestRunner);
            TestRunner::SetCurrentTestModule(tmpModule);
            auto result = tmpModule->Execute(library);
        });
        threads.push_back(std::move(thread));
    } // for modules

    // Did we run them in parallel - wait for termination...
    pLogger->Debug("Waiting for %zu module threads", threads.size());
    for (auto &t: threads) {
        t.join();
    }
    return bRes;

}
#endif

#ifdef TRUN_HAVE_FORK
class ForkProcDataHandler : public ProcessCallbackBase {
public:
    ForkProcDataHandler() = default;
    virtual ~ForkProcDataHandler() = default;
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

// shortcut for process clock
using pclock = std::chrono::system_clock;

enum class SubProcessState {
    kIdle,
    kRunning,
    kFinished,
};

// FIXME: Probably implement as a proper class - we need more features on this object than just holding some data
struct SubProcess {


    Process *proc = {};
    std::string name = {};
    ForkProcDataHandler dataHandler;
    std::thread thread;

    pclock::time_point tStart;
    SubProcessState state = SubProcessState::kIdle;

    bool processedOk = false;
};


bool TestModuleExecutorFork::Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) {
    static std::vector<SubProcess *> subProcesses;

    pLogger = gnilk::Logger::GetLogger("TestModExeFork");
    pLogger->Debug("Forking module tests");

    auto tStart = pclock::now();
    int threadCounter = 0;

    for (auto &[name, module] : testModules) {
        // Need to allocate outside, if passing reference the references is renewed before the capture picks it up..
        SubProcess *process = new SubProcess();
        auto tmpModule = module;
        printf("Starting module tests '%s' (%d / %zu)\n", module->name.c_str(), threadCounter, testModules.size());

        // Note: CAN'T USE REFERENCES - they will change before capture actually takes place..
        auto thread = std::thread([&library, tmpModule, process]() {
            process->state = SubProcessState::kRunning;

            process->proc = new Process("trun");
            process->proc->SetCallback(&process->dataHandler);
            process->name = tmpModule->name;
            process->proc->AddArgument("-m");
            process->proc->AddArgument(tmpModule->name);
            process->proc->AddArgument(library->Name());
            process->tStart = pclock::now();
            process->processedOk = process->proc->ExecuteAndWait();

            process->state = SubProcessState::kFinished;
        });
        process->thread = std::move(thread);
        subProcesses.push_back(process);
    }

    pLogger->Debug("Waiting for completition - %zu fork threads", subProcesses.size());
    int threadDeadCounter = 0;

    while(threadCounter > 0) {
        for(auto &p : subProcesses) {
            if (p->state == SubProcessState::kFinished) {
                p->thread.join();
                threadCounter--;
                threadDeadCounter++;
                printf("'%s' - Completed (%d/%zu)\n", p->name.c_str(), threadDeadCounter, subProcesses.size());
            }
        }
    }

    for(auto &p : subProcesses) {
        printf("Waiting for '%s'",p->name.c_str());
        fflush(stdout);
        long tLastDuration = 0;
        while(p->state != SubProcessState::kFinished) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(pclock::now() - p->tStart).count();
            if (duration != tLastDuration) {
                printf("\rWaiting for '%s' - %d sec",p->name.c_str(), (int)duration);
                fflush(stdout);
                tLastDuration = duration;

                // Stop process if it reaches timeout - specify 0 as 'infinity'
                if ((Config::Instance().forkModuleExecTimeoutSec > 0) && (duration > Config::Instance().forkModuleExecTimeoutSec)) {
                    printf("\nTimeout reached - stopping '%s'\n", p->name.c_str());
                    p->proc->Kill();
                }
            }
            std::this_thread::yield();
        }
        p->thread.join();
        pLogger->Debug("%d/%zu - completed", threadDeadCounter, subProcesses.size());
        printf(" - Completed (%d/%zu)\n", threadDeadCounter, subProcesses.size());
        threadDeadCounter++;

    }

    // FIXME: Consolidate output!
    // Also - I need to derive test-result structures for all tests
    // Starting to think the forking was the easy part...
    pLogger->Debug("Dumping output");
    for(auto &p : subProcesses) {
        if (!p->processedOk) {
            continue;
        }
        auto strings = p->dataHandler.Data();
        for(auto &s : strings) {
            printf("%s", s.c_str());
        }
    }

    auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(pclock::now() - tStart).count();
    printf("Total Duration: %d sec\n", (int)total_duration);


    return true;
}
#endif