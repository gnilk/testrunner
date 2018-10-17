#pragma once

#include "config.h"
#include "module.h"
#include "logger.h"
#include "testfunc.h"
#include <string>

class ModuleTestRunner {
public:
    ModuleTestRunner(IModule *module);
    void ExecuteTests();
private:
//    void ExecuteSingleTest(std::string funcName, std::string moduleName, std::string caseName);
    void ExecuteTest(TestFunc *f);
    void PrepareTests(std::vector<TestFunc *> &globals, std::vector<TestFunc *> &modules);
    TestFunc *CreateTestFunc(std::string sym);

private:
    IModule *module;
    gnilk::ILogger *pLogger;
};

