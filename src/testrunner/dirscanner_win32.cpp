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
#define WIN32_LEAN_AND_MEAN
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

std::vector<std::string> &DirScanner::Scan(std::string fromdir, bool recurse) {
    this->recurse = recurse;
    DoScan(fromdir);
    return filenames;
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


    // TODO: Implement this!

    // DIR *pDir = opendir(path.c_str());
    // if (pDir == NULL) {
    //     pLogger->Error("Unable to open directory: %s", path.c_str());
    //     return;
    // }
    // struct dirent *dp;
    // while((dp = readdir(pDir)) != NULL) {
    //     struct stat _stat;
    //     std::string entryName(path + "/" + dp->d_name);
    //     lstat(entryName.c_str(), &_stat);
    //     if (recurse && S_ISDIR(_stat.st_mode)) {
    //         if ((strcmp(dp->d_name,".")!=0) && (strcmp(dp->d_name,"..")!=0)) {
    //             DoScan(entryName);
    //         }
    //     } else {
    //         CheckAddFile(entryName);
    //     }
    // }
}

void DirScanner::CheckAddFile(std::string &filename) {
    std::string ext = GetExtension(filename);
    if (IsExtensionOk(ext)) {
        // Need proper path to file
//        char *fullPath = realpath(filename.c_str(),NULL);
		pLogger->Debug("Adding: %s\n", filename.c_str());
        filenames.push_back(filename);
    }
}

// returns the extension of a file name
std::string DirScanner::GetExtension(std::string &pathName)
{
	std::string ext;
	int l = pathName.length()-1;
	while ((l>0) && (pathName[l]!='.'))
	{
		ext.insert(ext.begin(),pathName[l]);
		l--;
	}
	if ((l > 0) && (pathName[l]=='.'))
	{
		ext.insert(0,".");
	}
	return ext;
}

// Check extension from the list of supported extensions
bool DirScanner::IsExtensionOk(std::string &extension)
{
	int i;
	for (i=0;lExtensions[i]!=NULL;i++)
	{
		if (!strcmp(extension.c_str(),lExtensions[i]))
		{
			return true;
		}
	}
	return false;
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

