//
// Example of shared library with test interface
//
#include <stdio.h>
#include <unistd.h>
#include "../testrunner/testinterface.h"

extern "C" {
	int test_shared_sleep(ITesting *t) {
//		printf("test_shared_sleep, got called, sleeping 100ms\n");
		t->Debug(__LINE__, __FILE__, "sleeping for 100ms");
		t->Info(__LINE__, __FILE__, "this is an info message");
		t->Warning(__LINE__, __FILE__, "this is a warning message");
		usleep(1000*100);	// sleep for 100ms - this will test
		return kTR_Pass;
	}
	int test_shared_a_error(ITesting *t) {
//		printf("test_shared_error, got called\n");
		t->Error(__LINE__, __FILE__, "this is just an error");
		return kTR_Fail;
	}
	int test_shared_b_fatal(ITesting *t) {
//		printf("test_shared_dispose, got called\n");
		t->Fatal(__LINE__, __FILE__,"this is a fatal error (stop all further cases for module)");
		return kTR_FailModule;
	}

	int test_shared_c_abort(ITesting *t) {
		t->Abort(__LINE__, __FILE__,"this is an abort error (stop any further testing)");
		return kTR_FailAll;
	}

	int test_mod_main(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_mod_main, got called");
		return kTR_Pass;
	}
	int test_mod_create(ITesting *t) {
		t->Abort(__LINE__, __FILE__, "test_mod_create, got called");
		return kTR_Fail;
	}
	int test_mod_dispose(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_mod_dispose, got called");
		return kTR_Pass;
	}

	int test_(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_, got called\n");
		return kTR_Pass;
	}
	int test_toplevel(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_toplevel, got called");
		return kTR_Pass;
	}

	int test_main(void *param) {
		ITesting *t = (ITesting *)param;
		t->Debug(__LINE__, __FILE__, "test_main, got called: error is: %p", t->Error);		
		return kTR_Pass;
	}

}