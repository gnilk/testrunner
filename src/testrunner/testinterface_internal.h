//
// This is the internal (testrunner) header file for the interface between libraries under test and the runner
// It defines ALL VERSIONS of the ITesting interface.
//
//                  DO NOT USE THIS VERSION FOR TESTABLE CODE!!!!
//
// Best option is to run the installer (make; make install) or via package manager (make package; sudo apt install ./<package>
// if you don't want that - the actual header files to be used for unit-tests in projects are found in: ext_testinterface
//

#ifndef TESTRUNNER_TESTINTERFACE_INTERNAL_H
#define TESTRUNNER_TESTINTERFACE_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>

//
// Include the offical 'testinterface.h' header file
//
#include "testinterface.h"
#include "version_t.h"

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif


//#define STR_TO_VER(ver) ((ver[0]<<24) | (ver[1]<<16) | (ver[2] << 8) | (ver[3]))


#ifdef __cplusplus
extern "C" {
#endif

// These are always the same..
#define kTR_Pass 0x00
#define kTR_Fail 0x10
#define kTR_FailModule 0x20
#define kTR_FailAll 0x30


//
// This is not a problem - I only use this structure internally during inheritance to allow treating all test-interface versions
// in the same manner without type-problems...
//
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextern-c-compat"
#endif
struct ITestingVersioned {};
#ifdef __clang__
#pragma GCC diagnostic pop
#endif


struct ITestingV1;
struct ITestingV2;

typedef void(TRUN_PRE_POST_HOOK_DELEGATE_V1)(ITestingV1 *);
typedef int(TRUN_PRE_POST_HOOK_DELEGATE_V2)(ITestingV2 *);
union CBPrePostHook {
    TRUN_PRE_POST_HOOK_DELEGATE_V1 *cbHookV1;
    TRUN_PRE_POST_HOOK_DELEGATE_V2 *cbHookV2;
};

typedef struct ITestingV2 ITestingInternal;

//
// Callback Version V1 - same as in ext_testinterface/testinterface_v1.h
//
struct ITestingV1 : public ITestingVersioned {
    // Just info output - doesn't affect test execution
    void (*Debug)(int line, const char *file, const char *format, ...);
    void (*Info)(int line, const char *file, const char *format, ...);
    void (*Warning)(int line, const char *file, const char *format, ...);
    // Errors - affect test execution
    void (*Error)(int line, const char *file, const char *format, ...); // Current test, proceed to next
    void (*Fatal)(int line, const char *file, const char *format, ...); // Current test, stop library and proceed to next
    void (*Abort)(int line, const char *file, const char *format, ...); // Current test, stop execution
    // Asserts
    void (*AssertError)(const char *exp, const char *file, const int line);
    // Hooks - this change leads to compile errors for old unit-tests - is that ok?
    void (*SetPreCaseCallback)(void(*)(ITestingV1 *));
    void (*SetPostCaseCallback)(void(*)(ITestingV1 *));

    // Dependency handling
    void (*CaseDepends)(const char *caseName, const char *dependencyList);
};

//
// Callback Version V2 - same as in ext_testinterface/testinterface_v1.h
//
struct ITestingV2 : public ITestingVersioned {
    // Just info output - doesn't affect test execution
    void (*Debug)(int line, const char *file, const char *format, ...);
    void (*Info)(int line, const char *file, const char *format, ...);
    void (*Warning)(int line, const char *file, const char *format, ...);
    // Errors - affect test execution
    void (*Error)(int line, const char *file, const char *format, ...); // Current test, proceed to next
    void (*Fatal)(int line, const char *file, const char *format, ...); // Current test, stop library and proceed to next
    void (*Abort)(int line, const char *file, const char *format, ...); // Current test, stop execution
    // Asserts
    kTRContinueMode (*AssertError)(const int line, const char *file, const char *exp);
    // Hooks - this change leads to compile errors for old unit-tests - is that ok?
    void (*SetPreCaseCallback)(int(*)(ITestingV2 *));         // v2 - must return int - same as test function 'kTR_xxx'
    void (*SetPostCaseCallback)(int(*)(ITestingV2 *));        // v2 - must return int - same as test function 'kTR_xxx'

    // Dependency handling
    void (*CaseDepends)(const char *caseName, const char *dependencyList);
    void (*ModuleDepends)(const char *moduleName, const char *dependencyList);  // v2 - module dependencies

    // This is perhaps a better way, we can extend as we see fit..
    // I think the biggest question is WHAT we need...
    void (*QueryInterface)(uint32_t interface_id, void **outPtr);                 // V2 - Optional, query an interface from the runner...
};



#ifdef __cplusplus
}
#endif

#endif //TESTRUNNER_TESTINTERFACE_INTERNAL_H
