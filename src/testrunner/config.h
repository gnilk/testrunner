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
    uint32_t responseMsgByteLimit;
    std::vector<std::string> modules;
    std::vector<std::string> inputs;
    std::string version;
    std::string description;
    std::string testMain;
    bool testGlobals;
    bool testLogFilter;
    bool skipOnModuleFail;
    bool stopOnAllFail;
    gnilk::ILogger *pLogger;
private:
    Config();
};