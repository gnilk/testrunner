#pragma once

#include "config.h"
#include "module_mac.h"
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
    ModuleTestRunner(IModule *module);
    void ExecuteTests();

    static TestModule *HACK_GetCurrentTestModule();

private:
    bool ExecuteMain();
    bool ExecuteMainExit();
    bool ExecuteGlobalTests();
    bool ExecuteModuleTests();
    bool ExecuteModuleTestFuncs(TestModule *testModule);
    TestResult *ExecuteTest(TestFunc *f);
    void HandleTestResult(TestResult *result);
    void PrepareTests();
    TestFunc *CreateTestFunc(std::string sym);

private:
    IModule *module;
    gnilk::ILogger *pLogger;
    std::map<std::string, TestModule *> testModules;
    std::vector<TestFunc *> globals;
};

