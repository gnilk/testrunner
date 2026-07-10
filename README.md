# testrunner
[![Build](https://github.com/gnilk/testrunner/actions/workflows/cmake.yml/badge.svg)](https://github.com/gnilk/testrunner/actions/workflows/cmake.yml)

Single header C/C++ Unit Test and Coverage 'Framework'

<b>Note:</b> This is V4 of the testrunner - the old V1 is in branch `trun_v1_main`.

<b>The coverage tool 'tcov' works with any executable</b>
You do not need to use the unit-test framework for the coverage tool to work. This should work even if you use CTest/GTest/Catch2/etc.

Heavy GOLANG inspired unit test framework for C/C++.
Currently works on macOS (arm/x86), Linux, and Windows (x86/x64). Embedded MCU targets are supported via
the `trun::mcu` engine — the framework has run on ESP32/NRF52/STM32/SiLabs EFR32; on-device validation of
the new V4 zero-allocation MCU engine (PlatformIO / Zephyr / possibly Arduino) is in progress.

The testing framework comes with two parts.
* A header to be used when defining the test-cases
* A runner to execute the tests
* A coverage tool to generate code coverage reports
* A library to be used in embedded systems or for in-process testing (optional)
 
See [Usage](#Usage) for more details on how to write test cases and execute them.

# Important changes between V4.x and V3.0

Used AI (Claude) to hunt for bugs and supervise some long required larger rewrites.
Lot's of bugs fixed. The old 'trunembedded' is split into 'trunlib' and the 'trun::mcu' engine — the
MCU engine for embedded projects and 'trunlib' for static in-process linkage on regular machines.
The MCU execution engine was completely rewritten (by Claude). It is now a zero-allocation,
minimal C/C++ library overhead (vsnprintf still used). 

Lots of validation was done to ensure a reliable delivery of the same results regardless of version and invocation.
Also a lot of effort was made into packaging and proper installation.

# Important changes between V2.x and V3.0

Experimental test coverage tool included. Will instrument the code being tested and generate a coverage report.
Requires LLDB installed on your system. The coverage tool (`tcov`) only works on macOS/Linux — it has no
Windows backend. (The test *runner* itself is fully supported on Windows again as of V4 — see [Building](#building).)
Note: tests written for V1 of the testinterface works
fine on new versions of the testrunner. However, v2 and v3 of the testinterface are not compatible with v1 for certain features (pre/post case callbacks).

See documentation at the bottom for Test Coverage tool.


# Building
You need CMake (3.16+, 3.28+ recommended) and a C++20 compiler: GCC or Clang on Linux/macOS,
or Visual Studio 2019/2022 on Windows. All third-party dependencies are fetched automatically
by CMake (see [Dependencies](#dependencies)) — nothing to install by hand.

```shell
git clone https://github.com/gnilk/testrunner.git
cd testrunner
cmake -B build                 # configure (add options below, e.g. -DCMAKE_BUILD_TYPE=Release)
cmake --build build -j         # build (or run ninja / make from inside build/)
sudo cmake --install build     # install (see Installing)
```

> The project defaults `CMAKE_BUILD_TYPE` to **Debug**. Pass `-DCMAKE_BUILD_TYPE=Release` for
> an optimized build (a Debug build also installs Debug-built dependencies).

## Build options
Pass options to the configure step, e.g. `cmake -B build -DBUILD_TCOV=OFF -DCMAKE_BUILD_TYPE=Release`.

| Option | Default | Description |
|---|---|---|
| `CMAKE_BUILD_TYPE` | `Debug` | Standard CMake build type. Use `Release` for optimized builds. |
| `CMAKE_INSTALL_PREFIX` | `/usr/local` | Where `cmake --install` puts files. |
| `BUILD_TCOV` | `ON` | Build the `tcov` code-coverage tool (needs `liblldb-dev` on Linux). |
| `BUILD_TRUNMCU` | `ON` | Build the zero-alloc **MCU** engine (`trunmcu`) and its demos. |
| `BUILD_TRUNEMBEDDED` | `ON` | Build the `trunembedded` demo — an example of embedding `trun::lib` in a desktop app (slated to be renamed `trunlib_example`). |
| `BUILD_OTHER` | `ON` | Build extra example executables. |
| `TRUN_BUNDLE_DEPS` | `OFF` | `ON` also installs the bundled fmt/cpptrace/libdwarf so the prefix is self-contained for `find_package`. `OFF` installs only testrunner's own files and relies on system fmt/cpptrace. See [Installing](#installing). |

**MCU engine capacities** — only relevant when you compile the embedded `trun::mcu` engine
(see [Consuming the libraries](#consuming-the-libraries)). They size fixed, no-heap storage:

| Option | Default | Description |
|---|---|---|
| `TRUN_MCU_MAX_TESTFUNCS` | `64` | Max registered `test_*` functions. |
| `TRUN_MCU_MAX_MODULES` | `16` | Max distinct test modules. |
| `TRUN_MCU_MSG_BUF_LEN` | `128` | Size of the single message/report scratch buffer. |
| `TRUN_MCU_SINK_MAX_RETRY` | `8` | Max consecutive output-sink retries before giving up. |

**Interface version** (`TRUN_USE_V1`) — the one universal knob: `testinterface.h` is always the
current (V2) interface and reverts to `testinterface_v1.h` when `TRUN_USE_V1` is defined. Define it
(e.g. `add_compile_definitions(TRUN_USE_V1)`) to build your test code — and the embedded `trun::mcu`
engine sources — against V1. There is no separate MCU-specific V1 option; `trun::mcu`'s sources compile
in your target, so they honour the same flag. (V2 is the default on every platform including Windows —
MSVC detects it via a version symbol, so V1 is no longer forced there.)

## What gets built
| Target | Kind | Purpose |
|---|---|---|
| `trun` | executable | The test-runner CLI (loads `.so`/`.dylib`, runs exported `test_*`). |
| `tcov` | executable | LLDB-based line-coverage tool. |
| `trunlib` (`trun::lib`) | static lib | Desktop **in-process** engine — threaded, no fork; links fmt + cpptrace + gnklog. |
| `trunmcu` (`trun::mcu`) | source lib | Zero-alloc **MCU** engine, compiled for the target (host-validated here). |
| `trun_utests` | shared lib | testrunner's own unit tests (it tests itself). |

## Dependencies
On Linux and macOS the runner uses `nm` (from **binutils**) to scan a library for `test_*`
symbols. On Linux:
```shell
sudo apt install binutils
```

The following are fetched automatically by CMake (FetchContent) — no manual install:
* **fmt** (https://github.com/fmtlib/fmt) — 12.1.0
* **cpptrace** (https://github.com/jeremy-rifkin/cpptrace) — v0.7.1 (pulls in libdwarf)
* **gnklog** (https://github.com/gnilk/gnklog) — logging library, interface-compatible on embedded

To build the coverage tool (`tcov`) on Linux you also need `liblldb-dev`. On macOS install LLDB
via Homebrew (see [Platform notes](#platform-notes)).

The **MCU** engine (`trunmcu`) is self-contained: it does **not** use fmt/cpptrace/gnklog.

<b>Note:</b>
<b>fmt</b> is Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors. (see: https://github.com/fmtlib/fmt for more details).

## Installing
`cmake --install build` (add `--prefix <dir>` to override `CMAKE_INSTALL_PREFIX`) installs two
logical component sets:
- **runtime** — the `trun` and `tcov` CLI tools + the man page.
- **dev** — `libtrunlib.a`, the public headers (`trunlib.h` — plus the deprecated
  `trunembedded.h` shim, removed in v5.0.0 — `testinterface.h`,
  `testinterface_v1.h`), and a CMake package config so downstream projects can
  `find_package(testrunner)` (see [Consuming the libraries](#consuming-the-libraries)).

A default install (`TRUN_BUNDLE_DEPS=OFF`) lays down **only** testrunner's own files, e.g. under
`/usr/local`:
```text
bin/trun  bin/tcov
include/trunlib.h  include/trunembedded.h  include/testinterface.h  include/testinterface_v1.h
lib/libtrunlib.a
lib/cmake/testrunner/testrunnerConfig.cmake   (+ ConfigVersion / Targets)
share/man/man1/trun.1.gz
```
It does **not** install fmt/cpptrace — a downstream `find_package(testrunner)` then expects those
from the system. Configure with `-DTRUN_BUNDLE_DEPS=ON` to also install the bundled
fmt/cpptrace/libdwarf and make the prefix self-contained. You can install one set only with
`cmake --install build --component runtime` (or `dev`).

### Debian package (Linux)
From `build/`, `cpack` (or `make package` / `ninja package`) produces two `.deb`s:
- **testrunner** — the runtime CLI tools.
- **testrunner-dev** — the library, headers and CMake config.

Install with `sudo apt install ./testrunner-<version>-*.deb`.

## Platform notes
### macOS
Install Xcode, then LLDB via Homebrew (needed for `tcov`):
```shell
brew update
brew install lldb
```
Then `cmake -B build && cmake --build build -j && sudo cmake --install build` (default prefix
`/usr/local`). macOS depends on `nm` (binutils) when scanning a library for test functions.

### Linux
`cmake -B build && cmake --build build -j && sudo cmake --install build`. Install `liblldb-dev`
for the coverage tool. The `.deb` packages install under `/usr`. Linux also depends on `nm`.

### Windows
Windows is a **first-class V2 platform** (Visual Studio 2019/2022, MSVC, C++20). Build it with the **same
CMake flow** as Linux/macOS — no special solution or `msbuild` invocation:
```shell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
MSVC detects the V2 interface via a version symbol, so **`-DTRUN_USE_V1` is not required** — V2 is the
default. **Parallel/fork module execution works** on Windows (`process_win32` + named-pipe IPC). Build an
installer with `cpack -G NSIS` from `build/` (produces `testrunner-<version>-win64.exe`).

The coverage tool **`tcov` is not available on Windows** (`BUILD_TCOV` is forced off — it needs an
LLDB/debug-engine backend that isn't implemented there).

## Consuming the libraries
Instead of running the external `trun` CLI you can link a testrunner library into your own
project and run tests **in-process** (~100k tests/sec on a modern machine). There are two
libraries with different natures and different consumption paths. Both expose the same small
API — register test cases and run them — see `app/trunembedded` and `app/trunmcu` for examples.

### Desktop / in-process — `trun::lib` via find_package
`trunlib` is the desktop in-process engine (threaded, no fork; links fmt + cpptrace + gnklog). Use it to
run tests *inside* your application — handy when memory layout or speed matters, or on platforms
without the external runner. Install testrunner (above), then in your project:
```cmake
find_package(testrunner 4.0 REQUIRED)
target_link_libraries(my_tests PRIVATE trun::lib)
```
Include `<trunlib.h>` and use the API (`trun::Initialize`, `trun::AddTestCase`,
`trun::RunTests`). (`<trunembedded.h>` still works as a deprecated shim — it forwards to
`<trunlib.h>` and is removed in v5.0.0.) `trunlib` links **fmt, cpptrace and gnklog**, so those must be
resolvable at your final link — from the system or your own FetchContent. `-DTRUN_BUNDLE_DEPS=ON` installs
bundled **fmt/cpptrace** into the prefix for a self-contained `find_package`; **gnklog** must be supplied
by the consumer for now (a gnklog `find_package` is on the way).

### Embedded / MCU — `trun::mcu` via FetchContent
The MCU engine is a separate, zero-alloc implementation (no heap / threads / exceptions / RTTI,
no fmt/cpptrace). It is **compiled for your target** from source, so you pull it in with
FetchContent (or a vendored `add_subdirectory`) rather than installing a prebuilt lib:
```cmake
include(FetchContent)
set(TRUN_MCU_MAX_TESTFUNCS 128)      # optional: size the fixed storage (see Build options)
FetchContent_Declare(trunmcu
    GIT_REPOSITORY https://github.com/gnilk/testrunner
    GIT_TAG        <tag>
    SOURCE_SUBDIR  src/app/trunmcu)  # only this subdir — no fmt/cpptrace, no desktop core
FetchContent_MakeAvailable(trunmcu)
target_link_libraries(my_tests PRIVATE trun::mcu)
```
Include `<trunmcu.h>` and use the same small API. To build against the V1 interface, define the
universal `TRUN_USE_V1` (e.g. `add_compile_definitions(TRUN_USE_V1)`) — it applies to both the
engine sources and your test code.

### Embedded with PlatformIO
<b>Note:</b> On-device validation of the V4 zero-alloc MCU engine (`trun::mcu`) is in progress —
PlatformIO is the primary target (a clean Zephyr project, and possibly Arduino, to follow). The
`library.json` manifest points PlatformIO at the MCU engine sources.
Clone the repository into your `lib_extra_dirs`, then add it as a dependency in your `platformio.ini`.
```text
lib_extra_dirs =
    ${sysenv.HOMEPATH}/MyLibraries/libraries

lib_deps =
    testrunner
```
You must then, in source, add your test cases and kick it off manually, like this:
```c++
// Example (Arduino-style setup() shown; adapt to your framework's entry point)
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

### Limitations of the in-process engines
Compared to the external `trun` CLI:
- No fork/subprocess isolation, and no parallel execution of test *modules*.
- The **MCU** engine (`trun::mcu`) additionally runs everything on the main thread — no threading,
  no exceptions, no heap. (`trun::lib` *is* threaded: it isolates each case in its own thread.)
- Internal logging is stripped down (the MCU engine has no verbose var-args logging).
- Assumes `stdout` is mapped to a console/serial port (embedded systems).
- Console reporting only (no JSON reporting).

# Unit-testing Usage

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
* test_module_exit (special test case name), called last for a module

Do NOT use these reserved names (_main, _exit) for anything else.

## Test Execution
The runner executes tests in the following order:
1. test_main, always executed (can be disabled via -G)
2. module functions
   - module main (test_module)
   - any other module function (test_module_XYZ)
   - module exit (test_module_exit)
3. test_exit, always executed last (also disabled via -G)

The order of module functions can be controlled via the `-m` switch. Default is to test all modules (`-m -`) but it is possible to change the order like: `-m shared,-` this will first test the module `shared` before proceeding with all other modules.

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

_all fail_ means that a test case aborted all further testing for the current dynamic library.

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
    if (!condition) { testInterface->AssertError(__LINE__, __FILE__, "condition not met"); return kTR_Fail; }
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

Note: Module dependencies must be handled in 'test_main'.

Like:
```c++
int test_main(ITesting *t) {
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
It is possible to register a pre/post callback hook for a test. You can/should set them in your module main. You can reset them (set to null) in your test exit (but it's
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
At present the stable extension is `ITestingConfig`, which allows a library under test to check how it is being executed. (There is also an experimental `ITestingCoverage`, currently inert.)

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
TestRunner v4.0.0-dev - macOS - C/C++ Unit Test Runner
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
      Disable parallel execution (use this if you debug through trun, default: off)
  --continue-on-assert
      Continue test execution on assert errors (default: off)
  --module-timeout <sec>
      Set timeout (in seconds) for forked execution, 0 - infinity (default: 30)
  --max-concurrency <n>
      Max module subprocesses running at once (0 = auto, ~CPU cores)
  --allow-thread-exit
      Test cases execution thread will self-terminate on assert/error

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
Each module is prefixed with
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

# Test Coverage
The test coverage tool (`tcov`) is a tool to generate test coverage reports. 
Coverage is calculated by running `trun` through LLDB and trap breakpoints for specific symbols under test.
Thus `tcov` requires you to have LLDB installed. Both the binaries and the dev-packages (in order to build it).

Assume you are building a DateTime handling library and you run `trun` as the unit-test framework. You can
now generate coverage by means of running `trun` through `tcov`.
Example:
```shell
./tcov -R base --symbols mynamespace::DateTime -- -m datetime myapp_utests.dylib
```

This will run the basic reporting and monitor coverage for any symbol in the class `DateTime` within the namespace `mynamespace`.
Anything after `--` is passed to `trun` as regular arguments.

Basic usage:
```shell
Coverage tool v4.0.0 - Linux - Calculating code coverage through LLDB
Usage: ./tcov [options] -- <target cmd line>
Options:
  -h, --help              Print this help
  -v, --verbose           Verbose output
  -t, --target            Target executable to run (default: trun)
  -R, --Report            Comma separated list of report engines (base, lcov, diff)
  -s, --symbols           Comma separated list of symbols to track for coverage
Linux
  --lldb-server <path>    Set the full path to the lldb-server binary

Examples:
Run locally (same directory) compiled 'trun' generate coverage for 'MyClass' pass '-m myclass ./libunittests.so' to trun
  ./tcov --target ./trun --symbols MyClass -- -m myclass ./libunittests.so

Run same as above but use both diff and lcov
  ./tcov -R diff,lcov --target ./trun --symbols MyClass -- -m myclass ./libunittests.so

```
While `tcov` was designed to be used with `trun` it can be used with any executable.
By specifying `--target` you can run any executable as the baseline for your unit-testing.
Any arguments after `--` will be passed to the target executable.

## Reporting
There are currently three different reporting modes available.
- base, basic grouping of functions by steps of 25% coverage 
- lcov, LCOV compatible output - you can use these files with VSCode or any other editor supporting LCOV files
- diff, generate a diff between the baseline and the current coverage (the baseline is overwritten)

### Diff reporting
Diff reporting is really useful when you write unit-tests and want to see progress and exactly which lines are being hit and not.
The first time invoked `tcov` will generate a baseline coverage report - everything will be tagged as 'new'.
After this the baseline will be used as the reference for the current coverage report in order
to track Added, Removed and Modified lines.

Each time you run `tcov` a new baseline file is created. Thus, if you run it twice in succession, the output
will say `no changes detected`.  If you update a unit-test to cover an else case which was not in the original, the diff will detect this and mark the line as covered the next time your run `tcov`.

## Symbols
You can/should specify a list of symbols to track coverage for. A symbol can be a namespace, class or function. Symbol lookup supports
wildcards (mynamespace::*, myclass::F*, etc...).


## Performance
As `tcov` is essentially stepping through your program, with a breakpoint on each statement for every matching symbol, it can take quite a while
to finish. As an example, tracking 30 unit-tests for a class with 1000 lines of code takes roughly 3 seconds on my M1 Pro.

Coverage is more of a dev-tool than a `generate coverage for this project` tool.
It is very helpful when you are writing unit-tests and want to see exactly which lines are being hit and not.

# Version history
## v4.0.0
- AI Cleanup release
- New MCU variant with dedicated zero allocation engine (API compatible with V1/2/3)
- Installation handling
  - CMake compatible ('find_package', 'fetch_content' for MCU variant)
  - Release has two variants 'regular' and '-dev' (full library version)
- Lot's of bugs fixed
  - Better installation on Linux
  - Forking improved
    - Conservative starting of sub-processes, observe max concurrency settings
    - IPC was spamming the console by re-printing all output
    - IPC framing of messages
    - Double counting results
    - Global 'test_main'/'test_exit' counted one (was per fork)
  - Argument parsing was overwriting modules
  - Teardown during Assert
  - V1 using setjmp/longjmp instead of just killing the execution thread
  - Threads for single test-function are now always enabled
## v3.0.4
- Embedded (or in-process) version of the library
## v3.0.3
- Exception handling
- Thread termination on assert/error/fatal
- gnklog has been upgraded and improved (speed and alike)
## v3.0.2
- C++17 compile for embedded
- New command line parser for trun
- Deadlock fixes in logger
## v3.0.1
- Fixed macOS run after install issue, whereby it could not find lldb
## v3.0.0
- Coverage tool added
- Various issues fixed in the 'testinterface.h' to better support C as well as C++
## v2.1.3
- Fixed bugs when compiling unit tests as C code instead of C++ code (testinterface.h was threw errors)
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
- Threads don't terminate on ASSERT/ERROR by default; specify `--allow-thread-exit` to enable (FATAL/ABORT always stop the test regardless)
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
