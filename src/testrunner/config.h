#pragma once

#include "logger.h"
#include <string>
#include <vector>
//
//
//

class Config {
public:
    static Config *Instance();
    void Dump();
public:
    int verbose;
    uint32_t responseMsgByteLimit;
    std::vector<std::string> modules;
    std::vector<std::string> testcases;
    std::vector<std::string> inputs;
    std::string version;
    std::string description;
    // the main func name is only for the global (all modules) main: 'test_main'
    // the module main is simply the 'test_module()' - without a test case..
    std::string mainFuncName;   // Expected main function name after spliiting (default: 'main')
    // the module exit function is: 'test_module_exit()' the global exit is 'test_exit'
    std::string exitFuncName;   // Expected exit function name after spliiting (default: 'exit')
    bool executeTests;
    bool listTests;
    bool printPassSummary;
    bool testModuleGlobals;
    bool testGlobalMain;
    bool testLogFilter;
    bool skipOnModuleFail;
    bool stopOnAllFail;
    bool suppressProgressMsg;
    bool discardTestReturnCode;
    bool linuxUseDeepBinding;     // Causes dlopen to use RTLD_DEEPBIND
    gnilk::ILogger *pLogger;
private:
    Config();
};