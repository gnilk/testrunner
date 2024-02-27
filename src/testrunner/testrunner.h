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
        explicit TestRunner(IDynLibrary *library);
        void PrepareTests();
        void ExecuteTests();
        void DumpTestsToRun();
        static TestModule::Ref HACK_GetCurrentTestModule();

    private:
        enum class kRunResultAction {
            kContinue,
            kAbortModule,
            kAbortAll,
        };

        bool ExecuteMain();
        bool ExecuteMainExit();
        bool ExecuteGlobalTests();
        bool ExecuteModuleTests();
        bool ExecuteModuleTestFuncs(TestModule::Ref testModule);
        kRunResultAction ExecuteTestWithDependencies(const TestModule::Ref &testModule, TestFunc *testCase);
        TestResult::Ref ExecuteModuleMain(TestModule::Ref testModule);
        void ExecuteModuleExit(TestModule::Ref testModule);
        TestResult::Ref ExecuteTest(TestModule::Ref testModule, TestFunc *testCase);
        void HandleTestResult(TestResult::Ref result);
        TestFunc *CreateTestFunc(std::string sym);


        kRunResultAction CheckResultIfContinue(TestResult::Ref result);

        TestModule::Ref GetOrAddModule(std::string &module);

    private:
        IDynLibrary *library = nullptr;
        ILogger *pLogger = nullptr;
        std::map<std::string, TestModule::Ref> testModules;
        std::vector<TestFunc *> globals;
    };
}