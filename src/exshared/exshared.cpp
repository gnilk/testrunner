//
// Example of shared library with test interface
//
#include <stdio.h>

extern "C" {
	void test_shared_main(void *param) {
		printf("test_shared_main, got called\n");
	}
	void test_shared_create(void *param) {
		printf("test_shared_create, got called\n");
	}
	void test_shared_dispose(void *param) {
		printf("test_shared_dispose, got called\n");
	}
	void test_(void *param) {
		printf("test_, got called\n");
	}
	void test_toplevel(void *param) {
		printf("test_toplevel, got called\n");
	}
	void test_shared_dispose_and_love(void *param) {
		printf("test_shared_dispose_and_love, got called\n");
	}

}