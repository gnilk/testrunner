# testrunner
[![Build](https://github.com/gnilk/testrunner/actions/workflows/cmake.yml/badge.svg)](https://github.com/gnilk/testrunner/actions/workflows/cmake.yml)

C/C++ Unit Test 'Framework'.

Heavy GOLANG inspired unit test framework for C/C++.
Currently works on macOS(arm/x86)/Linux/Windows (x86/x64)

<b>NEW:</b> Also works on certain embedded platforms (tested on ESP32)

# Building
You need CMake and GCC/Clang or Visual Studio (Windows). Tested with Visual Studio 17 and 19. The Windows version can be built in a 32 or 64 bit mode. Do note that the 32 bit don't support 64 bit DLL's and vice verse. 

1) Clone repository 'git clone https://github.com/gnilk/testrunner.git'
2) Create build directory (mkdir build)
3) Enter build directory (cd build)
4) Run cmake 'cmake ..'
5) Run 'make' or 'msbuild ALL_BUILD.vcxproj'

## Dependencies
On Linux and macOS you need 'nm' installed - and the development headers - this come from binutils. On Linux just do:
```shell
sudo apt install binutils binutils-dev
```

## Apple macOS
Just run 'make; sudo make install'. The binary (trun) will be installed in /usr/local/bin and the testinterface.h in /usr/local/include.
<b>Note:</b> The macOs version depends on 'nm' (from binutils) when scanning a library for test functions.

## Linux
Just run 'make; sudo make install'. The binary (trun) will be installed in /usr/local/bin and the testinterface.h in /usr/local/include
<b>Note:</b> The linux version depends on 'nm' (from binutils) when scanning a library for test functions. 

## Windows
Launch a 'Developer Command Prompt' from your Visual Studio installation.
To build release version: 'msbuild ALL_BUILD.vcxproj -p:Configuration=Release'.
The default will build 64bit with Visual Studio 2019 and 32bit with Visual Studio 2017.

As Windows don't have a default place to store 3rd party include files you need to copy the 'testinterface.h' file somewhere common on your environment. You want to include this file in your unit tests (note: It's optional).

## Embedded
<b>Only tested with PlatformIO as build system</b>

Clone the repository into your `lib_extras_dir`, if you use Arduino as underlying framework this is the library 
directory for Arduino. Add it as a dependency in your `platformio.ini` file.
```text
lib_extra_dirs =
    ${sysenv.HOMEPATH}/Documents/Arduino/libraries

lib_deps =
    testrunner
```
You must then, in source, add your test cases and kick it off manually, like this:
```c++
// Assuming Arduino style application...
void setup() {
    // Add some test cases
    trun::AddTestCase("test_main", test_main);
    trun::AddTestCase("test_emb", test_emb);
    trun::AddTestCase("test_emb_exit", test_emb_exit);
    trun::AddTestCase("test_emb_func1",  test_emb_func1);
    trun::AddTestCase("test_emb_func2", test_emb_func2);

    // Run some tests...
    trun::RunTests("-", "-");
}
``` 

### Limitations on embedded
- No threading, all tests are executed on the main thread
- Internal logging has been disabled (not possible to execute in verbose mode)
- Assumes `stdout` is mapped to console serial port (or similar)
- Only console reporting available 

# Usage

## Basics
The runner looks for exported functions within dynamic libraries. The exported function must match the following pattern:

`test_<library>_<testcase>`

_NOTE:_ In order to use the testrunner on your project you must compile your project as a dynamic library. And for Windows you must explicitly mark functions for export.

### Rules:
* Any test cased must be prefixed with 'test_' otherwise the runner will not pick them up
* test_main, reserved for first function called in a test run (see test execution)
* test_exit, reserved for last function called in a test run (see test execution)
* test_abc (only one underscore), called library main, called first for a library
* test_module_exit (special test case name), called last in a library run

Do NOT use these reserved names (_main, _exit) for anything else.

### Test Execution
The runner executes tests in the following order:
1. test_main, always executed
2. global functions (i.e. test_toplevel, just one underscore in name), can be switched off (-g)
3. library functions
   - library main (test_module)
   - any other library function (test_module_XYZ)
   - library exit (test_module_exit) 
4. test_exit, always executed last

The order of library functions can be controlled via the `-m` switch. Default is to test all modules (`-m -`) but it is possible to change the order like: `-m shared,-` this will first test the library `shared` before proceeding with all other modules.

### Pass/Fail distinction
Each test case should return `kTR_Pass` if no error occured. It is possible to discard the return code (`-r`) and the test runner will deduce the result based on interface call's.

The structure of the pass/fail output is:
`marker, testcase, duration, result code, errros, assert failures`

*Example:*

`=== PASS:	_test_toplevel, 0.000 sec, 0`

`=== FAIL:	_test_shared_a_error, 0.000 sec, 1, 0, 1`

There are 5 result codes:
- _0_, test pass (`kTestResult_Pass`)
- _1_, test fail (`kTestResult_TestFail`)
- _2_, library fail (`kTestResult_ModuleFail`)
- _3_, all fail (`kTestResult_AllFail`)
- _4_, not executed (`kTestResult_NotExecuted`) - not used

_module fail_ means that a test case aborted any further testing of the current library.

_all fail_ means that a test case aborted all further testing for the current dylib.

### ITesting interface

The ITesting interface (see: `testinterface.h`) is the only argument supplied to the test case.
```cpp
struct ITesting {
    // Just info output - doesn't affect test execution
    void (*Debug)(int line, const char *file, const char *format, ...); 
    void (*Info)(int line, const char *file, const char *format, ...); 
    void (*Warning)(int line, const char *file, const char *format, ...); 
    // Errors - affect test execution
    void (*Error)(int line, const char *file, const char *format, ...); // Current test, proceed to next
    void (*Fatal)(int line, const char *file, const char *format, ...); // Current test, stop library and proceed to next
    void (*Abort)(int line, const char *file, const char *format, ...); // Current test, stop execution
    // Hooks
    void (*SetPreCaseCallback)(void(*)(ITesting *));
    void (*SetPostCaseCallback)(void(*)(ITesting *));
    // Dependency handling
    void (*CaseDepends)(const char *caseName, const char *dependencyList);
};
```

### Assert Macro
There is an assert macro (TR_ASSERT) which works like a regular assert but is testinterface aware.
Use it like:
```cpp
    int test_module_func(ITesting *t) {
        bool res = exeute_a_test();
        // Assert it's true
        TR_ASSERT(t, res == true);
        return kTR_Pass;
    }
```    

### Case dependencies
Sometimes it is quite convienient to control order of test execution or ability to allow tests to depend on other tests.
For instance reading data from a database depends on both the connection to DB having been established and the data written.
Same for encoding/decoding. The decoder normally depends on having some encoding data written.

This is configured in runtime during execution of the module/library main function.
Like:
```cpp
int test_module(ITesting *t) {
    t->CaseDepends("decoding", "encoding");
    // Read depends on both write and connect
    t->CaseDepends("read", "write,connect");
    // Write depends only on connect
    t->CaseDepends("write", "connect");
    return kTR_Pass;
}
int test_module_endcoding(ITesting *t) {
    // do encoding tests here
    return kTR_Pass;
}
int test_module_decoding(ITesting *t) {
    // do decoding tests here
    return kTR_Pass;
}
```

During execution the test-runner will ensure that the encoding test is being executed before the decoding test.

### Advanced functionality
It is possible to register a pre/post callback hook for a test. You can/should set them in your library main. You can reset them (set to null) in your test exit.
For instance assume you have a memory allocation tracking library and you want to make sure you are not leaking memory.
```cpp
    int test_module(ITesting *t) {
        // Setup callback's
        t->SetPreCaseCallback(MyPreCase);
        t->SetPostCaseCallback(MyPostCase);
        // Enable tracking of memory allocations
        MemoryTracker_Enable();
        return kTR_Pass;
    }

    int test_module_exit(ITesting *t) {
        // Reset case callbacks
        t->SetPreCaseCallback(nullptr);
        t->SetPostCaseCallback(nullptr);
        // Disable tracking of memory allocations
        MemoryTracker_Disable();
        return kTR_Pass;
    }
    
    // This will be called before any test case in this library
    static void MyPreCase(ITesting *t) {
        // Reset statistics for every case before it's run
        MemoryTracker_Reset();
    }
    
    // This will be called after any test cast in this library
    static void MyPostCase(ITesting *t) {
        // Dump results
        MemoryTracker_Dump();
        // TODO: Verify results...        
    }
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

    // test_exit is a special function, always called last
	int test_exit(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_exit, got called: error is: %p", t->Error);		
		return kTR_Pass;
	}

    // this is a global/top-level function, they are called next after main
	int test_toplevel(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_toplevel, got called");
		return kTR_Pass;
	}

    // library (shared) casde 'sleep' are called afterwards
	int test_shared_sleep(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "sleeping for 100ms");
		t->Info(__LINE__, __FILE__, "this is an info message");
		t->Warning(__LINE__, __FILE__, "this is a warning message");
		usleep(1000*100);	// sleep for 100ms - this will test
		return kTR_Pass;
	}

	int test_shared_assert(ITesting *t) {
		int result = some_function();
		TR_ASSERT(t, result == 1);
		return kTR_Pass;		// kTR_Pass will be ignored if assert fails!
	}


	int test_shared_a_error(ITesting *t) {
		t->Error(__LINE__, __FILE__, "this is just an error");
		return kTR_Fail;
	}

	int test_shared_b_fatal(ITesting *t) {
		t->Fatal(__LINE__, __FILE__,"this is a fatal error (stop all further cases for library)");
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
TestRunner v1.1-Dev - macOS - C/C++ Unit Test Runner
Usage: trun [options] input
Options: 
  -v  Verbose, increase for more!
  -l  List all available tests
  -d  Dump configuration before starting
  -S  Include success pass in summary when done (default: off)
  -D  Linux Only - disable RTLD_DEEPBIND
  -g  Skip library globals (default: off)
  -G  Skip global main (default: off)
  -s  Silent, surpress messages from test cases (default: off)
  -r  Discard return from test case (default: off)
  -c  Continue on library failure (default: off)
  -C  Continue on total failure (default: off)
  -x  Don't execute tests (default: off)
  -R  <name> Use reporting library (default: console)
  -m <list> List of modules to test (default: '-' (all))
  -t <list> List of test cases to test (default: '-' (all))

Input should be a directory or list of dylib's to be tested, default is current directory ('.')
Module and test case list can use wild cards, like: -m encode -t json*
</pre>

### Examples
Be silent (`-s`) and continue even if a library fails `(-c)` or a global case fails (`-C`), test only cases in library `shared`.

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

Duration......: 0.013 sec
Tests Executed: 16
Tests Failed..: 5
Failed:
  [Tma]: _test_pure_main
  [Tma]: _test_shared_a_error
  [Tma]: _test_shared_b_assert, /src/testrunner/src/exshared/exshared.cpp:39, 1 == 2
  [tMa]: _test_shared_b_fatal, /src/testrunner/src/exshared/exshared.cpp:32, this is a fatal error (stop all further cases for library)
  [tmA]: _test_shared_c_abort, /src/testrunner/src/exshared/exshared.cpp:44, this is an abort error (stop any further testing)
</pre>

Be very verbose (`-vv`), dump configuration (`-d`), skip global execution (`-g`) won't skip main.
`bin/trun -vvdg -m mod .`

Same as previous but for just one specific library
`bin/trun -vvdg -m mod lib/libexshared.dylib`

# Listing of tests
To lists all available tests without executing them run:
`bin/trun -lx`
This will provide a list of modules and test cases in all libraries.
Any execution, filtering and so forth are considered in the listing hence you can run it like
`bin/trun -lxgG -m mod lib/libexshared.dylib`
This will give an indication of what tests the testrunner will execute.
```
- Globals:
    ::exit (_test_exit)
    ::main (_test_main)
* Module: mod
   m mod (_test_mod)
   e exit (_test_mod_exit)
  *  mod::create (_test_mod_create)
  *  mod::dispose (_test_mod_dispose)
- Module: pure
     pure::create (_test_pure_create)
     pure::dispose (_test_pure_dispose)
     pure::main (_test_pure_main)
 
 ... further output omitted...
```
Each library is prefixed with
* `-` won't execute
* `*` will execute

The test cases are prefixed with
* `*` will execute
* Nothing

The execution flag for a test case may have `m` or `e` in front this indicates 
if the test case is a `test_main` or `test_exit` function.

# Reporting
The classic console output reporting has been extended to support reporting modules.  To list all available reporting modules run:
`trun -R list`
This will dump the list of reporting library and then exit the program (without running any tests).

Currently (v1.1-DEV) the output would look like:
```
Reporting modules:
  console
  json
```

The default reporting library is `console` thus keeping with previous way of working.

To specify another reporting library simply do: `trun -R json`. In order to make the report the only output
you can combine it with the silent (`-s`) switch, which will now suppress all output.

## JSON Format Example
The JSON format is currently (at best) experimental

Example after running: `bin/trun -sSR json lib/libtrun_utests.dylib` (on macOS).<br> 
JSON Format (some results omitted):
```json
{
   "Summary": {
      "DurationSec": 1.150000,
      "TestsExecuted": 33,
      "TestsFailed": 4
   },
   "Failures": [
      {
         "Status": "TestFail",
         "Symbol": "_test_pure_main",
         "AssertValid": false
      },
      {
         "Status": "TestFail",
         "Symbol": "_test_shared_b_assert",
         "AssertValid": true,
         "Assert": {
            "File": "/Users/gnilk/src/github.com/testrunner/src/exshared/exshared.cpp",
            "Line": 39,
            "Message": "1 == 2"
         }
      }
   ],
   "Passes": [
      {
         "Status": "Pass",
         "Symbol": "_test_main"
      }
   ]
}
```

<b>Note:</b> Passes are only reported IF you include it in the summary (`-S`).

# Version history
## v1.3-Dev
- Test module and case selection can now negate (use: '!'), like `trun -t !case` run all but not case.. 
## v1.2-DEV
- Added ability to configure dependencies between cases within a library
- Added wild cards for modules and test case specification on cmd line
- *WIP:* Supported for embedded systems 
## v1.1
- Internal refactoring and clean-up
- Added ability to list test cases `-l` and not execute `-x`
- Added reporting (-R <library>) default is the console.
- Added suppression of stdout in silent mode (-s)
## v1.0
- Test summary, default only failure (-S also lists success)
## v0.9
- macOS M1 (arm64) support
- changed macOS to use Linux DL loader (through nm)
## v0.8
- Special library exit function 'test_module_exit' for post-test-library teardown

## v0.7
- Special function 'test_exit' for post-test system teardown
- Fixing small issues on Linux

## v0.6
- Test cases are now executed in their own threads and will be terminated on errors
- Linux, fixed issue with premature exit while scanning for dependencies
## v0.5
- Linux support, tested on ubuntu 19.10 - you must have binutils installed (in the path) - depends on 'nm'

## v0.4
- Windows Support (64/32 bit), only tested on Windows 10

## v0.3
- Added 'TR_ASSERT' macro for easier parameter checking

## v0.2
- Added '-t' option to run specific tests in a library

## v0.1
First released version
