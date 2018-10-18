#ifndef __GNILK_TEST_INTERFACE_H__
#define __GNILK_TEST_INTERFACE_H__

#ifdef __cplusplus
extern "C" {
#endif

// Return codes from test functions
#define kTR_Pass 0x00
#define kTR_Fail 0x10
#define kTR_FailModule 0x20
#define kTR_FailAll 0x30

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
    void (*Fatal)(int line, const char *file, const char *format, ...); // Current test, stop module and proceed to next
    void (*Abort)(int line, const char *file, const char *format, ...); // Current test, stop execution
};

#ifdef __cplusplus
}
#endif

#endif // __GNILK_TEST_INTERFACE_H__
