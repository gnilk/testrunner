#include <stdio.h>
#include "../testrunner/testinterface.h"


int test_pure_main(ITesting *t) {    
    printf("test_pure_main, got called\n");
    t->Error(__LINE__, __FILE__,"pure_main error message: %p",t);
	return kTR_Pass;
}
int test_pure_create(void *param) {
    printf("test_pure_create, got called\n");
	return kTR_Pass;
}
int test_pure_dispose(void *param) {
    printf("test_pure_dispose, got called\n");
	return kTR_Pass;
}
