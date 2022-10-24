#ifndef __GNILK_TEST_INTERFACE_H__
#define __GNILK_TEST_INTERFACE_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#include <Windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif


// Return codes from test functions
#define kTR_Pass 0x00
#define kTR_Fail 0x10
#define kTR_FailModule 0x20
#define kTR_FailAll 0x30

//#define TR_ASSERT(__t,_exp_) (!(_exp_)?t->AssertFail(#_exp_, __FILE__, __LINE__))
#define TR_ASSERT(t, _exp_) \
    ((void) ((_exp_) ? ((void)0) : ((ITesting *)t)->AssertError(#_exp_,__FILE__, __LINE__)))


//
// Callback interface for test reporting
//
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
    // Hooks
    void (*SetPreCaseCallback)(void(*)(ITesting *));
    void (*SetPostCaseCallback)(void(*)(ITesting *));
    // Dependency handling
    void (*CaseDepends)(const char *caseName, const char *dependencyList);
};

#ifdef __cplusplus
}
#endif

#endif // __GNILK_TEST_INTERFACE_H__
