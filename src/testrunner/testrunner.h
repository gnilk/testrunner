#pragma once

#include "module.h"
#include "logger.h"
#include <string>

class ModuleTestRunner {
public:
    ModuleTestRunner(IModule *module);
    void ExecuteTests();
private:
    void ExecuteSingleTest(std::string funcName, std::string moduleName, std::string caseName);
private:
    IModule *module;
    gnilk::ILogger *pLogger;
};
