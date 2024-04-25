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
        TestResult::Ref ExecuteModuleMain(const TestModule::Ref &testModule);
        void ExecuteModuleExit(TestModule::Ref testModule);
        TestResult::Ref ExecuteTest(const TestModule::Ref &testModule, const TestFunc::Ref &testCase);
        void HandleTestResult(TestResult::Ref result);
        TestFunc::Ref CreateTestFunc(std::string sym);


        kRunResultAction CheckResultIfContinue(const TestResult::Ref &result) const;
        TestModule::Ref GetOrAddModule(std::string &module);
    private:
        bool ExecuteModuleTestFuncsThreaded(TestModule::Ref testModule);
        kRunResultAction ExecuteTestWithDependencies(const TestModule::Ref &testModule, TestFunc::Ref testCase, std::vector<TestFunc::Ref> &deps);

    private:
        gnilk::ILogger *pLogger = nullptr;
        IDynLibrary::Ref library = nullptr;
        std::map<std::string, TestModule::Ref> testModules;
        std::vector<TestFunc::Ref> globals;
    };
}