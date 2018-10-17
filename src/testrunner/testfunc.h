#pragma once

#include "config.h"
#include "module.h"
#include "logger.h"
#include <string>

// Internal - used by test runner...
class TestFunc {
public:
    TestFunc();
    TestFunc(std::string symbolName, std::string moduleName, std::string caseName);
    bool IsGlobal();
    bool IsGlobalMain();
    void Execute(IModule *module);
    void SetExecuted();
    bool Executed();
public:
    std::string symbolName;
    std::string moduleName;
    std::string caseName;
private:
    bool isExecuted;
    gnilk::ILogger *pLogger;
};
