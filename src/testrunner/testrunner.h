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
//        bool ExecuteGlobalTests();
//        bool ExecuteModuleTestFuncs(TestModule::Ref testModule);
//        TestResult::Ref ExecuteModuleMain(const TestModule::Ref &testModule);
//        void ExecuteModuleExit(TestModule::Ref testModule);
//        TestResult::Ref ExecuteTest(const TestModule::Ref &testModule, const TestFunc::Ref &testCase);
//        void HandleTestResult(TestResult::Ref result);
        TestFunc::Ref CreateTestFunc(std::string sym);

//        TestResult::Ref ExecuteModulePreHook(const TestModule::Ref &testModule, const TestFunc::Ref &testCase);
//        TestResult::Ref ExecuteModulePostHook(const TestModule::Ref &testModule, const TestFunc::Ref &testCase);
//        TestResult::Ref ExecuteModulePrePostHook(const TestModule::Ref &testModule, const TestFunc::Ref &testCase, kRunPrePostHook runHook);
//        kRunResultAction CheckResultIfContinue(const TestResult::Ref &result) const;
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