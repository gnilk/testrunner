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
 </pre>

 \History
 - 2024.05.02, FKling, Implementation
 ---------------------------------------------------------------------------*/
#include "testrunner.h"
#include "moduleexecutors.h"

#ifdef TRUN_HAVE_THREADS
    #include <thread>
#endif

using namespace trun;

TestModuleExecutorBase &TestModuleExecutorFactory::Create() {
    static TestModuleExecutorSequential sequentialExecutor;

#ifdef TRUN_HAVE_THREADS
    if (Config::Instance().enableParallelTestExecution) {
        static TestModuleExecutorParallel parallelExecutor;
        return parallelExecutor;
    }
#endif
    return sequentialExecutor;
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

    // Now execute...
    for(auto &testModule : testModulesList) {
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


        TestRunner::SetCurrentTestModule(testModule);
        auto result = testModule->Execute(library);
        if ((result != nullptr) && (result->CheckIfContinue() == TestResult::kRunResultAction::kAbortAll)) {
            break;
        }
        TestRunner::SetCurrentTestModule(nullptr);
    } // for modules

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
