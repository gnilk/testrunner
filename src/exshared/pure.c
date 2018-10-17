#include <stdio.h>
#include "../testrunner/testinterface.h"


void test_pure_main(ITesting *t) {    
    printf("test_pure_main, got called\n");
    t->Error(42);

}
void test_pure_create(void *param) {
    printf("test_pure_create, got called\n");
}
void test_pure_dispose(void *param) {
    printf("test_pure_dispose, got called\n");
}
