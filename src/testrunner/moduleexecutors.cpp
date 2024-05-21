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
#include "resultsummary.h"
#include "testfunc.h"
#include <assert.h>
#include <vector>
#include <chrono>

#ifdef TRUN_HAVE_THREADS
    #include <thread>
#endif
#ifdef TRUN_HAVE_FORK
#include "unix/process.h"
#include "unix/subprocess.h"
#include "unix/IPCFifoUnix.h"
#include "ipc/IPCMessages.h"
#include "ipc/IPCDecoder.h"
#endif


using namespace trun;

TestModuleExecutorBase &TestModuleExecutorFactory::Create() {
    static TestModuleExecutorSequential sequentialExecutor;
    static TestModuleExecutorParallel parallelExecutor;     // threaded - that's not good for modules!
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
    static std::vector<SubProcess *> subProcesses;

    pLogger = gnilk::Logger::GetLogger("TestModExeFork");
    pLogger->Debug("Forking module tests");

    auto tStart = pclock::now();
    int threadCounter = 0;

    gnilk::IPCFifoUnix ipcServer;

    if (!ipcServer.Open()) {
        pLogger->Error("Unable to create IPC server!");
        return false;
    }

    for (auto &[name, module] : testModules) {
        if (!module->ShouldExecute()) {
            // Skip, this is not part of the configured filtered..
            continue;
        }
        // Already executed?
        if (!module->IsIdle()) {
            //pLogger->Debug("Tests for '%s' already executed, skipping",testModule->name.c_str());
            continue;
        }

        // Need to allocate outside, if passing reference the references is renewed before the capture picks it up..
        SubProcess *process = new SubProcess();
        printf("Starting module tests '%s' (%d / %zu)\n", module->name.c_str(), threadCounter, testModules.size());


        process->Start(library, module, ipcServer.FifoName());
        subProcesses.push_back(process);
    }

    pLogger->Debug("Waiting for completition - %zu fork threads", subProcesses.size());
    int threadDeadCounter = 0;

    // This works - perhaps not the best way, but still...
    for(auto &p : subProcesses) {
        long tLastDuration = 0;
        while(p->State() != SubProcessState::kFinished) {
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(pclock::now() - p->StartTime()).count();
            if (duration != tLastDuration) {
                printf("\rWaiting for '%s' - %d sec",p->Name().c_str(), (int)duration);
                fflush(stdout);
                tLastDuration = duration;

                // Stop process if it reaches timeout - specify 0 as 'infinity'
                if ((Config::Instance().moduleExecTimeoutSec > 0) && (duration > Config::Instance().moduleExecTimeoutSec)) {
                    printf("\nTimeout reached - stopping '%s'\n", p->Name().c_str());
                    p->Kill();
                }
            }
            std::this_thread::yield();
        }
        p->Wait();
        if (p->WasProcessExecOk()) {
            pLogger->Debug("Dumping output");
            for(auto &s : p->Strings()) {
                printf("%s", s.c_str());
            }
        }

        pLogger->Debug("%d/%zu - completed", threadDeadCounter, subProcesses.size());
        printf("\n");
        threadDeadCounter++;

    }

    // Ok, this works - but needs to be formalized...
    while(ipcServer.Available()) {

        gnilk::IPCResultSummary summary;
        gnilk::IPCBinaryDecoder decoder(ipcServer, summary);
        if (!decoder.Process()) {
            continue;

        }
        // Process message
        for(auto ipcTestResult : summary.testResults) {
            // We need to create a fake test-func here..
            auto tfuncWrapper = TestRunner::CreateTestFunc(ipcTestResult->symbolName);
            tfuncWrapper->SetResultFromSubProcess(ipcTestResult->testResult);

            ResultSummary::Instance().AddResult(tfuncWrapper);
        }
    }
    ipcServer.Close();

    return true;
}
#endif