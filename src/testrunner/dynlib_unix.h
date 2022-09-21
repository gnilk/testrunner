#pragma once

#include "logger.h"
#include "testinterface.h"
#include "dynlib.h"


#include <stdint.h>
#include <vector>
#include <string>
#include <map>


class DynLibLinux : public IDynLibrary {
public:
    DynLibLinux();
    ~DynLibLinux();

public: // IDynLibrary
    bool Scan(std::string pathName) override;
    void *Handle() override;
    const std::vector<std::string> &Exports() const override;
    void *FindExportedSymbol(std::string funcName) override;
    const std::string &Name() const override { return pathName; };

private:
    bool Open();
    bool Close();
    bool IsValidTestFunc(std::string funcName);

    std::string pathName;
    void *handle;
    int idxLib;
    // Set this global variable - for now, so we can resolve relative offsets
    //uint8_t *ptrModuleStart;
    std::vector<std::string> exports;
    std::map<std::string, int> symbols;

    gnilk::ILogger *pLogger;
};


