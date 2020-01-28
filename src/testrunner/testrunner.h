#pragma once

#include "config.h"
#include "module_mac.h"
#include "logger.h"
#include "testfunc.h"
#include "testresult.h"
#include <string>

class ModuleTestRunner {
public:
    ModuleTestRunner(IModule *module);
    void ExecuteTests();  
private:
    bool ExecuteMain(std::vector<TestFunc *> &globals);
    bool ExecuteGlobalTests(std::vector<TestFunc *> &globals);
    bool ExecuteModuleTests(std::vector<TestFunc *> &modules);
    TestResult *ExecuteTest(TestFunc *f);
    void HandleTestResult(TestResult *result);
    void PrepareTests(std::vector<TestFunc *> &globals, std::vector<TestFunc *> &modules);
    TestFunc *CreateTestFunc(std::string sym);

private:
    IModule *module;
    gnilk::ILogger *pLogger;
};

