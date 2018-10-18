#include <stdio.h>
#include "../testrunner/testinterface.h"


void test_pure_main(ITesting *t) {    
    printf("test_pure_main, got called\n");
    t->Error(__LINE__, __FILE__,"pure_main error message: %p",t);

}
void test_pure_create(void *param) {
    printf("test_pure_create, got called\n");
}
void test_pure_dispose(void *param) {
    printf("test_pure_dispose, got called\n");
}
