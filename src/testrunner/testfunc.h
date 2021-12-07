#pragma once

#include "config.h"
#include "module_mac.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include "testinterface.h"
#include "testhooks.h"
#include <string>



class TestFunc;

class TestModule {
public:
    TestModule(std::string _name) :
        name(_name),
        bExecuted(false),
        mainFunc(nullptr),
        exitFunc(nullptr),
        cbPreHook(nullptr),
        cbPostHook(nullptr) {};
    bool Executed() { return bExecuted; }

public:
    std::string name;
    bool bExecuted;
    TestFunc *mainFunc;
    TestFunc *exitFunc;

    TRUN_PRE_POST_HOOK_DELEGATE *cbPreHook;
    TRUN_PRE_POST_HOOK_DELEGATE *cbPostHook;

    std::vector<TestFunc *> testFuncs;
};

// The core structure defines a testable function which belongs to a test-module
class TestFunc {
public:
    TestFunc();
    TestFunc(std::string symbolName, std::string moduleName, std::string caseName);
    bool IsGlobal();
    bool IsModuleExit();
    bool IsGlobalMain();
    bool IsGlobalExit();
    TestResult *Execute(IModule *module);
    void SetExecuted();
    bool Executed();
    void ExecuteAsync();

    void SetTestModule(TestModule *_testModule) { testModule = _testModule; }
    TestModule *GetTestModule() { return testModule; }

public:
    std::string symbolName;
    std::string moduleName;
    std::string caseName;
private:
    bool isExecuted;
    gnilk::ILogger *pLogger;
    void HandleTestReturnCode();

    TestModule *testModule;
    TestResponseProxy *trp;

    PTESTFUNC pFunc;
    int testReturnCode;
    TestResult *testResult;

};
