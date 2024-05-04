#ifndef GNILK_TRUN_INTERFACE_H
#define GNILK_TRUN_INTERFACE_H

//
// Defines the external interface for the test-runner, you only need to include this and link as a shared-library (DLL)
// in order for the test-runner to pick it up...
//

#include <stdint.h>
#include <stdlib.h>

#ifdef WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #define DLL_EXPORT __declspec(dllexport)
#else
    #define DLL_EXPORT
#endif


//
// This symbol is used to determine the version of the header file - DO NOT redefine this!
//
#ifdef __cplusplus
extern "C" const uint32_t TRUN_MAGICAL_IF_VERSION  __attribute__ ((weak))  = 'GNK2';
#else
const uint32_t TRUN_MAGICAL_IF_VERSION  __attribute__ ((weak))  = 'GNK2';
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Return codes from test functions
#define kTR_Pass 0x00
#define kTR_Fail 0x10
#define kTR_FailModule 0x20
#define kTR_FailAll 0x30

//
// If you embed the trun library you need to specify if you run single threaded or not!
// The default (no config) is multithreading as this is the main use case...
//
// By default, the trunembbeded library is built with without threading support...
//


// Assert macro, checks version and casts to right (hopefully) interface...
// Note: comparing magic here is quite ok - we can't have a difference!
#define TR_ASSERT(t, _exp_) \
    if (!(_exp_)) {                                                    \
        ((ITesting *)t)->AssertError(#_exp_,__FILE__, __LINE__);   \
        return kTR_Fail; \
    }

typedef struct ITesting ITesting;
struct ITesting {
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
    void (*SetPreCaseCallback)(int(*)(ITesting *));         // v2 - must return int - same as test function 'kTR_xxx'
    void (*SetPostCaseCallback)(int(*)(ITesting *));        // v2 - must return int - same as test function 'kTR_xxx'

    // Dependency handling
    void (*CaseDepends)(const char *caseName, const char *dependencyList);
    void (*ModuleDepends)(const char *moduleName, const char *dependencyList);  // v2 - module dependencies

    // This is perhaps a better way, we can extend as we see fit..
    // I think the biggest question is WHAT we need...
    void (*QueryInterface)(uint32_t interface_id, void **outPtr);                 // V2 - Optional, query an interface from the runner...
};


//
// Experimental - built in extension interface for configuration fetching..
//

enum kTRConfigType {
    kTRCfgType_Bool,
    kTRCfgType_Num,
    kTRCfgType_Str,
};

#define TR_CFG_ITEM_NAME_LEN 32

// Not sure this is so great for Rust - will have to verify...
struct TRUN_ConfigItem {
    bool isValid;
    char name[TR_CFG_ITEM_NAME_LEN];
    kTRConfigType value_type;
    union {
        int boolean = 0;
        int32_t num;
        const char *str;    // Readonly..
    } value;
};

#define ITestingConfig_IFace_ID (uint32_t)(0xc07f19)

typedef struct ITestingConfig ITestingConfig;
struct ITestingConfig {
    size_t (*List)(size_t maxItems, TRUN_ConfigItem *outArray);
    void (*Get)(const char *key, TRUN_ConfigItem *outValue);
};

#ifdef __cplusplus
}
#endif

#endif //GNILK_TRUN_INTERFACE_H
