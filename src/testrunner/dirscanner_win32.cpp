/*-------------------------------------------------------------------------
 File    : dirscanner_win32.cpp
 Author  : FKling
 Version : -
 Orginal : 2019-10-24
 Descr   : Windows version - Scan's a directory for dynamic libraries

 Part of testrunner
 BSD3 License!
 
 Modified: $Date: $ by $Author: $
 ---------------------------------------------------------------------------
 TODO: [ -:Not done, +:In progress, !:Completed]
 <pre>

 </pre>
 
 
 \History
 - 2019.10.24, FKling, Implementation
 
 ---------------------------------------------------------------------------*/

#include "dirscanner.h"

#include <string>
#include <Windows.h>
#include <winnt.h>


//
// List of supported extensions
//
static const char *lExtensions[]={
    ".dll",     // Windows
    ".dylib",   // macOS
    NULL
};

DirScanner::DirScanner() {
    this->recurse = true;
    pLogger = gnilk::Logger::GetLogger("DirScanner");
}

void DirScanner::DoScan(std::string path) {
	HANDLE hFIND;
	WIN32_FIND_DATA findFileData;
	std::string searchPath = path.append("\\");

	searchPath.append("*.*");
	//pLogger->Debug("Scanning: %s", searchPath.c_str());
	hFIND = FindFirstFile(searchPath.c_str(), &findFileData);
	while (FindNextFile(hFIND, &findFileData))
	{
		std::string child = path;
		child.append(findFileData.cFileName);
		if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if ((strcmp(findFileData.cFileName, ".") != 0) && (strcmp(findFileData.cFileName, "..") != 0))
			{
				//std::string childPath = path;
				//childPath.append(findFileData.cFileName);
				DoScan(child);
			}
		}
		else
		{
			//std::string libName = path;
			//libName.append(findFileData.cFileName);
			CheckAddFile(child);
		}
	}
	FindClose(hFIND);
}

void DirScanner::CheckAddFile(std::string &filename) {
    std::string ext = GetExtension(filename);
    if (IsExtensionOk(ext)) {
		pLogger->Debug("Adding: %s\n", filename.c_str());
        filenames.push_back(filename);
    }
}


//
// Helpers
//

// Static
bool DirScanner::IsDirectory(std::string pathName) {
	DWORD attrib = GetFileAttributes(pathName.c_str());
	if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
		return true;
	}
	return false;
    // TODO: Implement this!!

    // struct stat _stat;
    // lstat(pathName.c_str(), &_stat);
    // if (S_ISDIR(_stat.st_mode)) {
    //     return true;
    // }
    // return false;
}

