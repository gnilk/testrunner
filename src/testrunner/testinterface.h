#ifndef __GNILK_TEST_INTERFACE_H__
#define __GNILK_TEST_INTERFACE_H__

#ifdef __cplusplus
extern "C" {
#endif

//
// Callback interface for test reporting
//
typedef struct ITesting ITesting;
struct ITesting {
    void (*Error)(const char *format, ...); // Current test, proceed to next
    void (*Fatal)(const char *format, ...); // Current test, stop module and proceed to next
    void (*Abort)(const char *format, ...); // Current test, stop execution
};

#ifdef __cplusplus
}
#endif

#endif // __GNILK_TEST_INTERFACE_H__
