#pragma once

#include "platform.h"

#include "config.h"
#include "logger.h"
#include "testfunc.h"
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
        static TestModule *HACK_GetCurrentTestModule();

    private:
        bool ExecuteMain();
        bool ExecuteMainExit();
        bool ExecuteGlobalTests();
        bool ExecuteModuleTests();
        bool ExecuteModuleTestFuncs(TestModule *testModule);
        TestResult *ExecuteModuleMain(TestModule *testModule);
        void ExecuteModuleExit(TestModule *testModule);
        TestResult *ExecuteTest(TestFunc *f);
        void HandleTestResult(TestResult *result);
        TestFunc *CreateTestFunc(std::string sym);

        TestModule *GetOrAddModule(std::string &module);

    private:
        IDynLibrary *library = nullptr;
        ILogger *pLogger = nullptr;
        std::map<std::string, TestModule *> testModules;
        std::vector<TestFunc *> globals;
    };
}