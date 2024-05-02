//
// Created by gnilk on 02.05.24.
//

#include "testrunner.h"
#include "moduleexecutors.h"

using namespace trun;

TestModuleExecutorBase &TestModuleExecutorFactory::Create() {
    static TestModuleExecutorSequential sequentialExecutor;
    static TestModuleExecutorParallel parallelExecutor;

    if (Config::Instance().enableParallelTestExecution) {
        return parallelExecutor;
    }
    return sequentialExecutor;
}

bool TestModuleExecutorSequential::Execute(const IDynLibrary::Ref &library, const std::map<std::string, TestModule::Ref> &testModules) {
    bool bRes = true;
    pLogger = gnilk::Logger::GetLogger("TestModExeSeq");

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


        TestRunner::SetCurrentTestModule(testModule);
        auto result = testModule->Execute(library);
        if ((result != nullptr) && (result->CheckIfContinue() == TestResult::kRunResultAction::kAbortAll)) {
            break;
        }
        TestRunner::SetCurrentTestModule(nullptr);
    } // for modules

    return bRes;
}

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

        auto thread = std::thread([&library, &currentTestRunner, &testModule] {
            TestRunner::SetCurrentTestRunner(currentTestRunner);
            TestRunner::SetCurrentTestModule(testModule);
            auto result = testModule->Execute(library);
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

