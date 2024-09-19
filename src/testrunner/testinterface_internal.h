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
#define STR_TO_VER(ver) (((uint64_t)ver[0]<<56) | ((uint64_t)ver[1]<<48) | ((uint64_t)ver[2] << 40) | ((uint64_t)ver[3] << 32) | ((uint64_t)ver[4] << 24) | ((uint64_t)ver[5] << 16) | ((uint64_t)ver[6] << 8) | ((uint64_t)ver[7]))

// This is the raw version type
typedef uint64_t version_t;


static constexpr version_t TRUN_MAGICAL_IF_VERSION1 = STR_TO_VER("GNK_0100");        // This version doesn't formally exist - it is assumed if no other version is found...
static constexpr version_t TRUN_MAGICAL_IF_VERSION2 = STR_TO_VER("GNK_0200");


#ifdef __cplusplus
extern "C" {
#endif

// These are always the same..
#define kTR_Pass 0x00
#define kTR_Fail 0x10
#define kTR_FailModule 0x20
#define kTR_FailAll 0x30

typedef struct ITestingV2 ITesting;

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

typedef enum {
    kTRLeave,
    kTRContinue,
} kTRContinueMode;

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




//
// V2 extensions
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


/*
static bool TRUN_ContinueOnAssert(ITesting *t) {
    ITestingConfig *trConfig = {};
    t->QueryInterface(ITestingConfig_IFace_ID, (void **)&trConfig);
    if (trConfig == nullptr) {
        return false;   // default
    }
    TRUN_ConfigItem continueOnAssert = {};
    trConfig->Get("continue_on_assert", &continueOnAssert);
    if ((continueOnAssert.isValid) && (continueOnAssert.value_type == kTRCfgType_Bool)) {
        return continueOnAssert.value.boolean;
    }
    return false;
}
 */

// Assert macro, checks version and casts to right (hopefully) interface...
// Note: comparing magic here is quite ok - we can't have a difference!
#define TR_ASSERT(t, _exp_) \
    if (!(_exp_)) {                                                    \
        auto tr_temp_res = ((ITesting *)t)->AssertError(__LINE__, __FILE__, #_exp_);   \
        if (tr_temp_res == kTRContinueMode::kTRLeave) return kTR_Fail; \
    }

#define TR_REQUIRE(t, _exp_, _msg_) \
    if (!(_exp_)) {                 \
        ((ITesting *)t)->Error(__LINE__, __FILE__, #_msg_); \
        return kTR_Fail;                            \
    }

#ifdef __cplusplus
}
#endif


#endif //TESTRUNNER_TESTINTERFACE_INTERNAL_H
