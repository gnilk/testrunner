# testrunner
[![Build](https://github.com/gnilk/testrunner/actions/workflows/cmake.yml/badge.svg)](https://github.com/gnilk/testrunner/actions/workflows/cmake.yml)

Single header C/C++ Unit Test 'Framework'.

<b>Note:</b> This is V2 of the testrunner - the old V1 is in branch `trun_v1_main`.

Heavy GOLANG inspired unit test framework for C/C++.
Currently works on macOS(arm/x86)/Linux/Windows (x86/x64)/embedded(tested on: ESP32/NRF52/STM32x/SiLabs EFR32M series)

The testing framework comes with two parts.
* A header to be used when defining the test-cases
* A runner to execute the tests

See [Usage](#Usage) for more details on how to write test cases and execute them.

# Important changes between 1.6.x and V2.0

## Execution
Parallel execution of test modules. This brings (depending on project) a massive speed-up (about 10x) for medium-sized
projects. Testing on a project with ~90 modules and around a total of 500 test-cases completes within 6 seconds instead
of 60-70 seconds using V1.6.2 on the same hardware.

The testrunner will determine which modules to execute and then internally spin up
separate test-runners in parallel to execute the tests. This improves performance (_alot_) for larger projects.
However, it does limit the ability to debug through through the testrunner. As such - if you write your code in a type
of TDD fashion you should execute the testrunner with `--sequential` and this will disable the parallel execution.

Testrunner ASSERT macro will not terminate threads. Instead it will return. This assumes that the ASSERT macro is only
called from within the main-body of the testable code (i.e. the body of `test_module_func`). IF you use the ASSERT macro
elsewhere - you should run tests with `--allow-thread-exit`.

## Versions
Version 2.x introduces new features in the test interface, effectively deprecating the old interface. If you don't want
to upgrade your unit tests you need to compile your tests with `-D TRUN_USE_V1` to revert back to the old V1 interface.

Old code is still supported (i.e. if you have a local fork of the `testinterface.h`) this will still work properly.

When you install you will always get the latest version of the `testinterface.h` file. This means if you upgrade from V1.x to V2.x there will
be API breaking changes to your tests. However, the testrunner itself can handle both new and old test-interface usage.

The old testinterface is still available under the name `testinterface_v1.h` and if you compile with `TRUN_USE_V1` it
will be used instead of the new version.

<b>Note:</b> Versioning is not supported on Windows. You must compile your tests with `TRUN_USE_V1`.

# Building
You need CMake and GCC/Clang or Visual Studio (Windows). Tested with Visual Studio 19 and 2022.
The Windows version can be built in a 32 or 64 bit mode. Do note that the 32 bit don't support 64 bit DLL's and vice verse.

1) Clone repository 'git clone https://github.com/gnilk/testrunner.git'
2) Create build directory (mkdir build)
3) Enter build directory (cd build)
4) Run cmake `cmake ..`
5) Run `make` or `msbuild ALL_BUILD.vcxproj`

## Dependencies
On Linux and macOS you need 'nm' installed - and the development headers - this come from binutils. On Linux just do:
```shell
sudo apt install binutils binutils-dev
```

The following libraries are fetched when running cmake:
* cpptrace (https://github.com/jeremy-rifkin/cpptrace.git) - v0.7.1
* fmtlib (https://github.com/fmtlib/fmt) - fetching version 10.1.1
* gnklog (https://github.com/gnilk/gnklog) - new logging library which is interface compatible on embedded

<b>Note:</b>
<b>fmtlib</b> is Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors. (see: https://github.com/fmtlib/fmt for more details).

## Apple macOS
Just run `make; sudo make install`. The binary (trun) will be installed in /usr/local/bin and the testinterface.h in /usr/local/include.
<b>Note:</b> The macOs version depends on 'nm' (from binutils) when scanning a library for test functions.

## Linux
Just run `make -j; sudo make install`. The binary (trun) will be installed in `/usr/bin` and the `testinterface.h` in `/usr/include`
<b>Note:</b> The linux version depends on 'nm' (from binutils) when scanning a library for test functions.

You can also run `make -j package` to generate a `.deb` package. Which you can install with `sudo apt install ./testrunner-<version>-Linux.deb`.

## Windows
<b>Note:</b> V2 should compile (`trunwindows` has been updated and tested on VS 2022) but it has not been tested. Also parallel features are not available on Windows.

Launch a 'Developer Command Prompt' from your Visual Studio installation.
To build release version: `msbuild ALL_BUILD.vcxproj -p:Configuration=Release`.
The default will build 64bit with Visual Studio 2019/2022 and 32bit with Visual Studio 2017.

As Windows don't have a default place to store 3rd party include files you need to copy the `testinterface.h` file somewhere common on your environment.
You want to include this file in your unit tests (note: It's optional).

Also, `testinterface` versioning does not work on Windows (relies on `weak` symbols) - instead you must use the old version; always compile your test-code with `-DTRUN_USE_V1`

## Embedded
<b>Note:</b> Embedded has not yet been verified with V2.

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

The testrunner framework has two parts.
* A header file; `testinterface.h`, to help implement test cases (essentially this is optional)
* A runner; `trun`, to execute the test case

You must compile your test cases and the code they are testing to a dynamic library (.dll, .so, .dylib - depending on OS).
The runner `trun` will look for exported functions matching a certain pattern within your dynamic library. The exported function must match the following pattern:

`test_<module>_<testcase>`
<b>Note:</b> In order to use the testrunner on your project you must compile your project as a dynamic library (shared object). And for Windows you must explicitly mark functions for export.

## Rules:
* Any test cased must be prefixed with 'test_' otherwise the runner will not pick them up
* test_main, reserved for first function called in a shared library (see test execution)
* test_exit, reserved for last function called in a shared library (see test execution)
* test_abc (only one underscore), called module main, called first for a module
* test_module_exit (special test case name), called last in a library run

Do NOT use these reserved names (_main, _exit) for anything else.

## Test Execution
The runner executes tests in the following order:
1. test_main, always executed (can be disabled via -G)
2. library functions
   - library main (test_module)
   - any other library function (test_module_XYZ)
   - library exit (test_module_exit)
3. test_exit, always executed last (also disabled via -G)

The order of library functions can be controlled via the `-m` switch. Default is to test all modules (`-m -`) but it is possible to change the order like: `-m shared,-` this will first test the library `shared` before proceeding with all other modules.

## Pass/Fail distinction
Each test case should return `kTR_Pass` if no error occurred. It is possible to discard the return code (`-r`) and the test runner will deduce the result based on interface calls.

The structure of the pass/fail output is:
`marker, testcase, duration, result code, errrors, assert failures`

*Example:*

`=== PASS:	_test_module, 0.000 sec, 0`

`=== FAIL:	_test_shared_a_error, 0.000 sec, 1, 0, 1`

There are 5 result codes:
- _0_, test pass (`kTestResult_Pass`)
- _1_, test fail (`kTestResult_TestFail`)
- _2_, module fail (`kTestResult_ModuleFail`)
- _3_, all fail (`kTestResult_AllFail`)
- _4_, not executed (`kTestResult_NotExecuted`) - not used

_module fail_ means that a test case aborted any further testing of the current module.

_all fail_ means that a test case aborted all further testing for the current library.

## ITesting interface

The ITesting interface (see: `testinterface.h`) is the only argument supplied to the test case.
```cpp
// V2 of the ITesting interface
typedef struct ITesting ITesting;
struct ITesting {
    // Just info output - doesn't affect test execution
    void (*Debug)(int line, const char *file, const char *format, ...);
    void (*Info)(int line, const char *file, const char *format, ...);
    void (*Warning)(int line, const char *file, const char *format, ...);
    // Errors - affect test execution
    void (*Error)(int line, const char *file, const char *format, ...); // Current test, proceed to next
    void (*Fatal)(int line, const char *file, const char *format, ...); // Current test, stop library and proceed to next
    void (*Abort)(int line, const char *file, const char *format, ...); // Current test, stop execution
    // Asserts
    void (*AssertError)(const int line, const char *file, const char *exp);
    // Hooks - this change leads to compile errors for old unit-tests - is that ok?
    void (*SetPreCaseCallback)(int(*)(ITesting *));         // v2 - must return int - same as test function 'kTR_xxx'
    void (*SetPostCaseCallback)(int(*)(ITesting *));        // v2 - must return int - same as test function 'kTR_xxx'

    // Dependency handling
    void (*CaseDepends)(const char *caseName, const char *dependencyList);
    void (*ModuleDepends)(const char *moduleName, const char *dependencyList);  // v2 - module dependencies

    // This is perhaps a better way, we can extend as we see fit..
    // I think the biggest question is WHAT we need...
    void (*QueryInterface)(uint32_t interface_id, void **outPtr);                 // V2 - Optional, query an interface from the runner...
};
```

## Assert Macro
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

The assert macro `TR_ASSERT` will call back into the test runner application. Which will log/save the assert information and then
deal with it. IF the test runner is started with `--allow-thread-exit` the test runner will terminate the thread executing
the test case once it has dealt with the assert information. This will FORCE an exit of the test case at the point of the assert.
However, this termination is not 100% safe in all case. Thus by default the `TR_ASSERT` macro will issue a failure return.

This behavior makes the `TR_ASSERT` macro usable ONLY within the main body of the test case function and it is not recommended
to use it from any sub-functions called by the test case.

Using `TR_ASSERT` is equivialent of doing:
   ```c++
    if (!condition) { testInterface->Assert(__LINE__, __FILE__, "condition not met"); return kTR_Failure; }
   ```

## Case dependencies
Sometimes it is quite convenient to control order of test execution or ability to allow tests to depend on other tests.
For instance reading data from a database depends on both the connection to DB having been established and the data written.
Same for encoding/decoding. The decoder normally depends on having some encoding data written.

This is configured in runtime during execution of the module main function.
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

## Module Dependencies
Similar to case-dependencies, you can control/override module execution order by letting them depend on each other.
This allows for overriding module execution order and other thins. While it also allows for separation when configuring
modules with side-effects, for example integration testing, where one module might initialize a full system.

Like:
```c++
int test_mymodule(ITesting *t) {
    t->ModuleDepends("dbwrite", "dbconfigure, dbconnect");
    t->ModuleDepends("dbconnect", "dbconfigure);
    t->ModuleDepends("dbread", "dbwrite");
    return kTR_Pass;
}
```
The above would ensure that `dbconfigure` is ran first. Also `dbconnect` will be executed before any of `dbread` or `dbwrite`.
At the same time - there is no point with read-testing unless we have written something first. Therefore
we say that `dbwrite` should execute before `dbread` (as `dbread` depends on `dbwrite`).

## Advanced functionality
It is possible to register a pre/post callback hook for a test. You can/should set them in your library main. You can reset them (set to null) in your test exit (but it's
not required as hooks are associated with the module upon setting them).

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
        // Reset case callbacks, optional
        t->SetPreCaseCallback(nullptr);
        t->SetPostCaseCallback(nullptr);
        // Disable tracking of memory allocations
        MemoryTracker_Disable();
        return kTR_Pass;
    }
    
    // This will be called before any test case in this library
    // New from V2.0.0 - you must return a kTR_xyz code from here!
    static int MyPreCase(ITesting *t) {
        // Reset statistics for every case before it's run
        MemoryTracker_Reset();
        return kTR_Pass;
    }
    
    // This will be called after any test cast in this library
    // New from V2.0.0 - you must return a kTR_xyz code from here!
    static int MyPostCase(ITesting *t) {
        // Dump results
        MemoryTracker_Dump();
        return kTR_Pass;        
    }
```    

## Using QueryInterface

_NOTE:_ QueryInterface is considered experimental and unstable. The API might change.

From version 2.0 of the TestRunner the ITesting interface provides a new function 'QueryInterface' this is a generic extension point interface.
At the present there is only one extension present; `ITestingConfig`. Which allows a library under test to check how it is being executed.

While it is allowed to access `QueryInterface` from any place, the recommended place is `test_main` (the global library main point).
The `ITestingConfig` extension can be used to verify if the test-runner is invoked in a supported manner. A project might not actually support parallel testing.
And can basically abort the test if the test-runner is invoked in such a manner.
```c++
int test_main(ITesting *t) {
    ITestingConfig *tr_config = nullptr;
    t->QueryInterface(ITestingConfig_IFace_ID, (void **)&tr_config);
    TR_ASSERT(t, tr_config != nullptr);
    TRUN_ConfigItem cfg_moduleExecutionType = {};
    tr_config->get("moduleExecutionType", cfg_moduleExecutionType);
    
    // Convoluted for now...
    if (cfg_moduleExecutionType.isValid) {
        if (cfg_moduleExecutionType.value_type == kTRCfgType_Num) {
            if (cfg_moduleExecutionType.value.num == 0) {
                // Don't support parallel module testing for this project
                return kTR_FailAll;
            }
        } 
    }
}
```

The above checks if the `testrunner` (`trun`) was invoked without `--sequential` (i.e. parallel mode - which is the default mode) and rejects the testing all together if so.
Three can be many reasons why you would like to reject testing. In this specific case there are memory-arenas which will run amok.

## Advanced functionality
Specifying `--sequential` turns off parallel execution of modules. If you debug your code through the test-runner it is necessary to switch off parallel execution
as the main trun instance just acts as a coordinator for executing other instances running the specific tests and thus won't be able to trap your debugger properly.

# Your Code
In your dynamic library export the test cases (following the pattern) you would like to expose. Compile your library and execute the runner on it.
See `src/exshared/exshared.cpp` for details.

## Example Code
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

    // module 'shared' main is called after 'test_main'
	int test_shared(ITesting *t) {
        return kTR_Pass;
    }
    
    // module (shared) cases are called afterwards
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
    
    // module 'shared' exit is called last after all other test functions in `shared`
	int test_shared_exit(ITesting *t) {
        return kTR_Pass;
    }

    
    // Another module, has main but no exit...
	int test_mod(ITesting *t) {
		t->Debug(__LINE__, __FILE__, "test_mod, got called");
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

# Trun Usage

<b>Note:</b> TestRunner default input is the current directory. It will search recursively for any testable functions.

```
TestRunner v2.1.0 - macOS - C/C++ Unit Test Runner
Usage: trun [options] input
Options: 
  -v  Verbose, increase for more!
  -l  List all available tests
  -d  Dump configuration before starting
  -S  Include success pass in summary when done (default: off)
  -D  Linux Only - disable RTLD_DEEPBIND
  -G  Skip global main (default: off)
  -s  Silent, surpress messages from test cases (default: off)
  -r  Discard return from test case (default: off)
  -c  Continue on module failure - kTR_FailModule - (default: off)
  -C  Continue on total failure - kTR_FailAll -  (default: off)
  -x  Don't execute tests (default: off)
  -R  <name> Use reporting library (default: console)
  -O  <file> Report final result to this file, use '-' for stdout (default: -)
  -m  <list> List of modules to test (default: '-' (all))
  -t  <list> List of test cases to test (default: '-' (all))
  --sequential
      Disable any parallel execution of modules
  --continue_on_assert
      Continue test execution on assert errors (default: off)
  --module-timeout <sec>
      Set timeout (in seconds) for forked execution, 0 - infinity (default: 30)
  --allow-thread-exit
      Test cases execution thread will self-terminate on assert/error/fatal

Input should be a directory or list of dylib's to be tested, default is current directory ('.')
Module and test case list can use wild cards, like: -m encode -t json*
```

## Examples
Be silent (`-s`) and continue even if a library fails `(-c)` or a global case fails (`-C`), test only cases in library `shared`.

`bin/trun -scC -m shared .`

Output:
```
build$ bin/trun -scC -m shared

=== RUN  	_test_main
=== PASS:	_test_main, 0.000 sec, 0

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
```

Be very verbose (`-vv`), dump configuration (`-d`), skip global execution (`-g`) won't skip main.
`bin/trun -vvdg -m mod .`

Same as previous but for just one specific library
`bin/trun -vvdg -m mod lib/libexshared.dylib`

### Listing of tests
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

Currently (v2) the output would look like:
```
Reporting modules:
  console
  json
  jsonext
```

The default reporting library is `console` thus keeping with previous way of working.

To specify another reporting library simply do: `trun -R json`. In order to make the report the only output
you can combine it with the silent (`-s`) switch, which will now suppress all output.

## JSON Format Example
JSON output of the test-results. This makes it easy to import test-results in a build-server or similar.

Example output:
```json
{
  "Summary": {
    "DurationSec": 0.250000,
    "TestsExecuted": 4,
    "TestsFailed": 2
  },
  "Failures": [
    {
      "Status": "TestFail",
      "Symbol": "test_rip_complex_hops",
      "DurationSec": 0.000000,
      "ExecutionState": "main",
      "IsAssertValid": true,
      "Assert": {
        "File": "/home/user/src/project/tests/test_rip.cpp",
        "Line": 113,
        "Message": "rip.HopsForRouteTo(5) == 3"
      }
    },
    {
      "Status": "TestFail",
      "Symbol": "test_rip_complex_routeto",
      "DurationSec": 0.000000,
      "ExecutionState": "main",
      "IsAssertValid": true,
      "Assert": {
        "File": "/home/user/src/project/tests/test_rip.cpp",
        "Line": 102,
        "Message": "rip.RouteTo(4) == 2"
      }
    }
  ],
  "Passes": [
    {
      "Status": "Pass",
      "Symbol": "test_main",
      "DurationSec": 0.000000,
      "IsAssertValid": false
    },
    {
      "Status": "Pass",
      "Symbol": "test_rip",
      "DurationSec": 0.000000,
      "IsAssertValid": false
    }
  ]
}
```

<b>Note:</b> Passes are only reported IF you include it in the summary (`-S`).

# Version history
## v2.1.2
- Fixed a bug when using V1 causing asserts to continue even if they should exit
- When dumping config also write out continue on assert setting
## v2.1.1
- Fixed embedded version (exceptions, version handling, pre-c++ 17, etc..)
## v2.1
- Support for exceptions, i.e. won't crash
- Support for continue on assert error (--continue_on_assert)
- Added new 'TR_REQUIRES' which will always break a test-case 
## v2.0
- Two external dependencies
   - `gnklog` has replaced the old debug logging library - this is interface compatible with embedded
   - `fmtlib` is being used, as this is a requirement for `gnklog`
   - Both libraries are fetched and added to the local source directory by the build script (cmake)
- Pre/Post now returns test-result (kTR_xyz), this will cause any previous unit-tests to break compile.
- Extensions are now supported through function `QueryInterface` in ITesting
- Modules are executed in parallel as default (`--sequential` to disable)
- You can specify `--module-timeout` (in seconds) to kill long-running/hanging modules (default is 30sec)
- Threads don't terminate on ASSERT/ERROR/FATAL, specify `--allow-thread-exit` to enable
- More stringent, no global test functions except `test_main` and `test_exit` (module main are still `test_<module>`)
## v1.6.3
- Dangling reference could lead to seg-fault when finished
- Compile issues on macos
## v1.6.0
- Code clean up and refactoring
- Dependency handling rewritten (also fixing circular dependencies)
- FIX: Crash on non-existing dependency (nullptr)
## v1.5.1
- BUG: JSON reporting was not escaping strings properly leading to JSON was wrongly produced
## v1.5
- Possible to specify report file -O <file>
## v1.4
- Test module and case selection can now negate (use: '!'), like `trun -t -,!case` run all but not case..
- Embedded version is now verified on ESP32 (espressif-idf) and NRF52 (with Zephyr)
- On Linux you can now build .deb package files
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
