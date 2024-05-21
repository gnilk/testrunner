#pragma once

#include "platform.h"

#include "config.h"
#include "logger.h"
#include "testfunc.h"
#include "testmodule.h"
#include "testresult.h"
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

        // ITesting can call-us (any time) to set this...
        void AddDependenciesForModule(const std::string &moduleName, const std::string &dependencyList);

        // These fetch from either thread_local Context or just Context - depending on IF we are compiled with threads or not
        static void SetCurrentTestModule(TestModule::Ref currentTestModule);
        static void SetCurrentTestRunner(TestRunner *currentTestRunner);
        static TestModule::Ref GetCurrentTestModule();
        static TestRunner *GetCurrentRunner();
        static TestFunc::Ref CreateTestFunc(const std::string &sym);

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
        TestModule::Ref GetOrAddModule(const std::string &module);
        TestModule::Ref ModuleFromName(const std::string &moduleName);
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