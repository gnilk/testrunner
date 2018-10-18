#pragma once

#include <string>
#include "logger.h"

class DirScanner {
public:
    DirScanner();
    std::vector<std::string> &Scan(std::string fromdir, bool recurse);
    static bool IsDirectory(std::string pathName);
private:
    void DoScan(std::string fromdir);
    std::string GetExtension(std::string &pathName);
    bool IsExtensionOk(std::string &extension);
    void CheckAddFile(std::string &filename);


private:
    bool recurse;
    gnilk::ILogger *pLogger;
    std::vector<std::string> filenames;
};