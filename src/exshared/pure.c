#include <stdio.h>
#include "../testrunner/testinterface.h"


#ifdef WIN32
#include <Windows.h>
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT 
#include <unistd.h>
#endif


DLL_EXPORT int test_pure_main(ITesting *t) {    
    printf("test_pure_main, got called\n");
    t->Error(__LINE__, __FILE__,"pure_main error message: %p",t);
	return kTR_Pass;
}
DLL_EXPORT int test_pure_create(void *param) {
    printf("test_pure_create, got called\n");
	return kTR_Pass;
}
DLL_EXPORT int test_pure_dispose(void *param) {
    printf("test_pure_dispose, got called\n");
	return kTR_Pass;
}
