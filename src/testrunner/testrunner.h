#pragma once

#include "config.h"
#include "module.h"
#include "logger.h"
#include <string>

class TestFunc {
public:
    TestFunc() {
        isExecuted = false;
    }
    bool IsGlobal() {
        return (moduleName == "-");
    }
    bool IsGlobalMain() {
        return (IsGlobal() && (caseName == Config::Instance()->testMain));
    }
    void SetExecuted() {
        isExecuted = true;
    }
    bool Executed() {
        return isExecuted;
    }
public:
    std::string symbolName;
    std::string moduleName;
    std::string caseName;
private:
    bool isExecuted;
};

class ModuleTestRunner {
public:
    ModuleTestRunner(IModule *module);
    void ExecuteTests();
private:
    void ExecuteSingleTest(std::string funcName, std::string moduleName, std::string caseName);
    void ExecuteTestFunc(TestFunc *f);
    void PrepareTests(std::vector<TestFunc *> &globals, std::vector<TestFunc *> &modules);
private:
    IModule *module;
    gnilk::ILogger *pLogger;
};

