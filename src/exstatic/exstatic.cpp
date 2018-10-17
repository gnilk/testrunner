//
// Example of static library with test interface
//
#include <stdio.h>

extern "C" {
	void test_static_main(void *param) {
		printf("test_static_main, got called\n");
	}
	void test_static_create(void *param) {
		printf("test_static_create, got called\n");
	}
	void test_static_dispose(void *param) {
		printf("test_static_dispose, got called\n");
	}
	void test_(void *param) {
		printf("test_, got called\n");
	}
	void test_toplevel(void *param) {
		printf("test_toplevel, got called\n");
	}
	void test_static_dispose_and_love(void *param) {
		printf("test_static_dispose_and_love, got called\n");
	}

}