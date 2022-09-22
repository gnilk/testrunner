//
// Created by gnilk on 9/22/2022.
//

#ifndef TESTRUNNER_PLATFORM_H
#define TESTRUNNER_PLATFORM_H

// Note: This file is more or less included everywhere - add platform specific includes/defines/quirks here..


//
// This file contains (mainly) things needed to compile the project from VStudio on Windows
// It compiles fine from cmd-line. I simply don't know how to configure Visual Studio to build properly without
// having to include "Windows.h" in every file...  (getting ton's of errors down the rabbit hole about RPC not having
// 'byte' defined and so forth)...
//

#if defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

#endif //TESTRUNNER_PLATFORM_H
