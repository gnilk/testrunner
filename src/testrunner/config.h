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
    std::vector<std::string> modules;
    std::vector<std::string> inputs;
    std::string version;
    std::string description;
    std::string testMain;
    bool testGlobals;
    gnilk::ILogger *pLogger;
private:
    Config();
};