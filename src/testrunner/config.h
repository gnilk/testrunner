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
    int testsExecuted;
    int testsFailed;
    uint32_t responseMsgByteLimit;
    std::vector<std::string> modules;
    std::vector<std::string> testcases;
    std::vector<std::string> inputs;
    std::string version;
    std::string description;
    std::string testMain;
    bool testGlobals;
    bool testGlobalMain;
    bool testLogFilter;
    bool skipOnModuleFail;
    bool stopOnAllFail;
    bool discardTestReturnCode;
    gnilk::ILogger *pLogger;
private:
    Config();
};