/*-------------------------------------------------------------------------
 File    : dirscanner.cpp
 Author  : FKling
 Version : -
 Orginal : 2018-10-18
 Descr   : Scan's a directory for dynamic libraries

    In order for this project to be X-Platform this must be replaced

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2018.10.18, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "dirscanner.h"
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <dlfcn.h>


//
// List of supported extensions
//
//static const char *lExtensions[]={
//    ".dll",     // Windows
//    ".dylib",   // macOS
//    NULL
//};

DirScanner::DirScanner() {
    this->recurse = true;
#ifdef APPLE
    extensions.push_back(".dylib");
#else
    extensions.push_back(".so");
#endif
    pLogger = gnilk::Logger::GetLogger("DirScanner");
}


void DirScanner::DoScan(std::string path) {
    DIR *pDir = opendir(path.c_str());
    if (pDir == NULL) {
        pLogger->Error("Unable to open directory: %s", path.c_str());
        return;
    }
    struct dirent *dp;
    while((dp = readdir(pDir)) != NULL) {
        struct stat _stat;
        std::string entryName(path + "/" + dp->d_name);
        lstat(entryName.c_str(), &_stat);
        if (recurse && S_ISDIR(_stat.st_mode)) {
            if ((strcmp(dp->d_name,".")!=0) && (strcmp(dp->d_name,"..")!=0)) {
                DoScan(entryName);
            }
        } else {
            CheckAddFile(entryName);
        }
    }
}

void DirScanner::CheckAddFile(std::string &filename) {
    std::string ext = GetExtension(filename);
    if (IsExtensionOk(ext)) {
        // Need proper path to file
        char *fullPath = realpath(filename.c_str(),NULL);
        filenames.push_back(std::string(fullPath));
    }
}

//
// Helpers
//

// Static
bool DirScanner::IsDirectory(std::string pathName) {
    struct stat _stat;
    lstat(pathName.c_str(), &_stat);
    if (S_ISDIR(_stat.st_mode)) {
        return true;
    }
    return false;
}

