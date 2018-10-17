//
// Example of shared library with test interface
//
#include <stdio.h>
#include <unistd.h>
#include "../testrunner/testinterface.h"

extern "C" {
	void test_shared_sleep(void *param) {
//		printf("test_shared_sleep, got called, sleeping 100ms\n");
		usleep(1000*100);	// sleep for 100ms - this will test
	}
	void test_shared_a_error(ITesting *t) {
//		printf("test_shared_error, got called\n");
		t->Error("this is just an error");
	}
	void test_shared_b_fatal(ITesting *t) {
//		printf("test_shared_dispose, got called\n");
		t->Fatal("this is a fatal error (stop all further cases for module)");
	}

	void test_shared_c_abort(ITesting *t) {
		t->Abort("this is an abort error (stop any further testing)");
	}

	void test_mod_main(void *param) {
		printf("test_mod_main, got called\n");
	}
	void test_mod_create(void *param) {
		printf("test_mod_create, got called\n");
	}
	void test_mod_dispose(void *param) {
		printf("test_mod_dispose, got called\n");
	}

	void test_(void *param) {
		printf("test_, got called\n");
	}
	void test_toplevel(void *param) {
		printf("test_toplevel, got called\n");
	}

	void test_main(void *param) {
		ITesting *t = (ITesting *)param;
		printf("test_main, got called\n");
		t->Error("failed system initialization, missing resources..");
	}

}