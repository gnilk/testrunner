#pragma once

#include "platform.h"

#include "config.h"
#include "logger.h"
#include "testfunc.h"
#include "testmodule.h"
#include "testresult.h"
#include "testhooks.h"
#include <string>
#include <vector>
#include <map>


namespace trun {
//
// Runs test for a specific dynamic library
//
    class TestRunner {
    public:
        explicit TestRunner(IDynLibrary::Ref library);
        void PrepareTests();
        void ExecuteTests();
        void DumpTestsToRun();
        static TestModule::Ref HACK_GetCurrentTestModule();
        static void HACK_SetCurrentTestModule(TestModule::Ref currentTestModule);

    private:
        enum class kRunResultAction {
            kContinue,
            kAbortModule,
            kAbortAll,
        };
        enum class kRunPrePostHook {
            kRunPreHook,
            kRunPostHook,
        };

        bool ExecuteMain();
        bool ExecuteMainExit();
        bool ExecuteModuleTests();
        TestFunc::Ref CreateTestFunc(std::string sym);
        void AddDependencyForModule(const std::string &moduleName, const std::string &dependencyList);
        TestModule::Ref GetOrAddModule(std::string &module);
    private:
        kRunResultAction ExecuteTestWithDependencies(const TestModule::Ref &testModule, TestFunc::Ref testCase, std::vector<TestFunc::Ref> &deps);

    private:
        gnilk::ILogger *pLogger = nullptr;
        IDynLibrary::Ref library = nullptr;
        std::map<std::string, TestModule::Ref> testModules;
        //std::vector<TestFunc::Ref> globals;
        TestFunc::Ref globalMain = nullptr;
        TestFunc::Ref globalExit = nullptr;
    };
}