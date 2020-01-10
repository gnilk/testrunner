#pragma once

#include "config.h"
#include "module.h"
#include "logger.h"
#include "testresult.h"
#include "responseproxy.h"
#include <string>

// Internal - used by test runner...
class TestFunc {
public:
    TestFunc();
    TestFunc(std::string symbolName, std::string moduleName, std::string caseName);
    bool IsGlobal();
    bool IsGlobalMain();
    TestResult *Execute(IModule *module);
    void SetExecuted();
    bool Executed();

    void ExecuteAsync();

public:
    std::string symbolName;
    std::string moduleName;
    std::string caseName;
private:
    bool isExecuted;
    gnilk::ILogger *pLogger;
    void HandleTestReturnCode();

    TestResponseProxy *trp;

    PTESTFUNC pFunc;
    int testReturnCode;
    TestResult *testResult;

};
