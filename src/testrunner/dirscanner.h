#pragma once

#include <string>
#include "logger.h"

namespace trun {
// Note: This class is implemented differently depending on target OS
// Linux/MacOS share implementation
    class DirScanner {
    public:
        DirScanner();
        std::vector<std::string> &Scan(std::string fromdir, bool recurse) {
            this->recurse = recurse;
            DoScan(fromdir);
            return filenames;
        }
        static bool IsDirectory(std::string pathName);
    private:
        void DoScan(std::string fromdir);
        void CheckAddFile(std::string &filename);

        std::string GetExtension(std::string &pathName) {
            std::string ext;
            auto l = pathName.length() - 1;
            while ((l > 0) && (pathName[l] != '.')) {
                ext.insert(ext.begin(), pathName[l]);
                l--;
            }
            if ((l > 0) && (pathName[l] == '.')) {
                ext.insert(0, ".");
            }
            return ext;
        }
        bool IsExtensionOk(std::string &extension) {
            for (auto ext: extensions) {
                if (ext == extension) {
                    return true;
                }
            }
            return false;
        }
    private:
        bool recurse;
        ILogger *pLogger;
        std::vector<std::string> extensions;    // library extensions
        std::vector<std::string> filenames;
    };
}