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
    std::string testMain;
    std::string testExit;
    bool printPassSummary;
    bool testGlobals;
    bool testGlobalMain;
    bool testLogFilter;
    bool skipOnModuleFail;
    bool stopOnAllFail;
    bool discardTestReturnCode;
    bool linuxUseDeepBinding;     // Causes dlopen to use RTLD_DEEPBIND
    gnilk::ILogger *pLogger;
private:
    Config();
};