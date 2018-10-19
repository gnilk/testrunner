# testrunner

C/C++ Unit Test 'Framework'.

Heavy GOLANG inspired unit test framework for C/C++.
Currently only works on _macOS_

## Basics
The runner looks for exported functions within dynamic libraries. The exported function must match the following pattern:

`test_<module>_<testcase>`

_NOTE:_ In order to use the testrunner on your project you must compile your project as a dynamic library.

### Test Execution
The runner executes tests in the following order:
1) test_main, always executed (can't be switched off)
2) global functions (i.e. test_toplevel, just one underscore in name), can be switched off (-g)
3) any module function

The order of module functions can be controlled via the `-m` switch. Default is to test all modules (`-m -`) but it is possible to change the order like: `-m shared,-` this will first test the module `shared` before proceeding with all other modules.

### Pass/Fail distinction
Each test case should return `kTR_Pass` if no error occured. It is possible to discard the return code (`-r`) and the test runner will deduce the result based on interface call's.

The structure of the pass/fail output is:
`marker, testcase, duration, result code`

*Example:*

`=== PASS:	_test_toplevel, 0.000 sec, 0`

`=== FAIL:	_test_shared_a_error, 0.000 sec, 1`

There are 5 result codes:
- _0_, test pass (`kTestResult_Pass`)
- _1_, test fail (`kTestResult_TestFail`)
- _2_, module fail (`kTestResult_ModuleFail`)
- _3_, all fail (`kTestResult_AllFail`)
- _4_, not executed (`kTestResult_NotExecuted`) - not used

_module fail_ means that a test case aborted any further testing of the current module.

_all fail_ means that a test case aborted all further testing for the current dylib.

### ITesting interface

The ITesting interface (see: `testinterface.h`) is the only argument supplied to the test case.
```
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
```

## Your Code
In your dynamic library export the test cases (following the pattern) you would like to expose. Compile your library and execute the runner on it.

See `src/exshared/exshared.cpp` for details.

### Example Code
```c++
extern "C" {
    // test_main is a special function, always called first
	int test_main(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_main, got called: error is: %p", t->Error);		
		return kTR_Pass;
	}

    // this is a global/top-level function, they are called next after main
	int test_toplevel(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_toplevel, got called");
		return kTR_Pass;
	}

    // module (shared) casde 'sleep' are called afterwards
	int test_shared_sleep(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "sleeping for 100ms");
		t->Info(__LINE__, __FILE__, "this is an info message");
		t->Warning(__LINE__, __FILE__, "this is a warning message");
		usleep(1000*100);	// sleep for 100ms - this will test
		return kTR_Pass;
	}

	int test_shared_a_error(ITesting *t) {
		t->Error(__LINE__, __FILE__, "this is just an error");
		return kTR_Fail;
	}

	int test_shared_b_fatal(ITesting *t) {
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


}
```

See *exshared* library for an example.

## Usage

_NOTE_: TestRunner default input is the current directory. It will search recursively for any testable functions.

<pre>
TestRunner v0.1 - C/C++ Unit Test Runner
Usage: trun [options] input
Options: 
  -v  Verbose, increase for more!
  -d  Dump configuration before starting
  -g  No globals, skip globals (default: off)
  -s  Silent, surpress messages from test cases (default: off)
  -r  Discard return from test case (default: off)
  -c  Continue on module failure (default: off)
  -C  Continue on total failure (default: off)
  -m <list> List of modules to test (default: '-' (all))

Input should be a list dylib's to be tested
</pre>

### Examples
Be silent (`-s`) and continue even if a module fails `(-c)` or a globa case fails (`-C`), test only cases in module `shared`.

`bin/trun -scC -m shared .`

Output:
<pre>
build$ bin/trun -scC -m shared

=== RUN  	_test_main
=== PASS:	_test_main, 0.000 sec, 0

=== RUN  	_test_toplevel
=== PASS:	_test_toplevel, 0.000 sec, 0

=== RUN  	_test_shared_a_error
=== FAIL:	_test_shared_a_error, 0.000 sec, 1

=== RUN  	_test_shared_b_fatal
=== FAIL:	_test_shared_b_fatal, 0.000 sec, 2

=== RUN  	_test_shared_c_abort
=== FAIL:	_test_shared_c_abort, 0.000 sec, 3

=== RUN  	_test_shared_sleep
=== PASS:	_test_shared_sleep, 0.103 sec, 0
</pre>

Be very verbose (`-vv`), dump configuration (`-d`), skip global execution (`-g`) won't skip main.
`bin/trun -vvdg -m mod .`

Same as previous but for just one specific library
`bin/trun -vvdg -m mod lib/libexshared.dylib`
