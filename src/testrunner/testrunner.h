#pragma once

#include "config.h"
#include "logger.h"
#include "testfunc.h"
#include "testresult.h"
#include "testhooks.h"
#include <string>
#include <vector>
#include <map>


//
// Module is a DLL
//
class ModuleTestRunner {
public:
    explicit ModuleTestRunner(IDynLibrary *module);
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

private:
    IDynLibrary *module = nullptr;
    gnilk::ILogger *pLogger = nullptr;
    std::map<std::string, TestModule *> testModules;
    std::vector<TestFunc *> globals;
};

