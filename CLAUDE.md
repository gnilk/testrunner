# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this project is

`testrunner` is a Go-inspired C/C++ unit test and code coverage framework. It produces three main artifacts:
- **`trun`** — the test runner CLI that dynamically loads shared libraries and executes exported `test_*` functions
- **`tcov`** — a coverage tool that runs `trun` under LLDB and instruments breakpoints to measure line coverage
- **`trunlib`** — a static library for in-process / embedded use (no fork, no threads)

It is written in C++20. Runs on Linux and macOS (the v1.0 branch runs on Windows).

## Build commands

```bash
# Configure (from repo root)
mkdir build && cd build && cmake ..

# Or use an existing build dir (e.g. cmake-build-debug)
cd cmake-build-debug

# Build everything
ninja          # or: make -j

# Build only trun
ninja trun

# Build only the unit test shared library
ninja trun_utests

# Install (Linux)
sudo ninja install

# Package as .deb
ninja package
```

The active build directories are `cmake-build-debug` and `cmake-build-release`. The `bld/` dir is an older Make-based artifact.

### Dependencies

Fetched automatically by CMake via FetchContent — no manual install needed:
- `fmtlib` 12.1.0
- `gnklog` (gnilk's logging library, interface-compatible with embedded)
- `cpptrace` v0.7.1

System requirement on Linux for `tcov`: `liblldb-dev` and `binutils-dev`.

## Running the internal tests

The project tests itself using its own framework. After building:

```bash
cd cmake-build-debug

# Run all internal unit tests
./trun lib/libtrun_utests.so

# Run a single module
./trun -m strutil lib/libtrun_utests.so

# Run a specific test case
./trun -m strutil -t split lib/libtrun_utests.so

# Sequential (needed for debugging through a test)
./trun --sequential lib/libtrun_utests.so

# List tests without executing
./trun -lx lib/libtrun_utests.so
```

## Code architecture

### Execution flow for `trun`

1. `src/app/trun/trun.cpp` — entry point; parses args via `Config::FromArguments`, scans input path for `.so`/`.dylib` files using `DirScanner`
2. For each library: `IDynLibrary` (abstract interface in `src/shared/dynlib.h`) loads it via `dlopen`, then `nm` lists exported symbols matching `test_*`
3. `TestRunner` (`src/testrunner/testrunner.h`) groups symbols into `TestModule` objects (one per `test_<module>` prefix)
4. Execution delegates to `TestModuleExecutorFactory::Create()` which returns one of:
   - `TestModuleExecutorSequential` — single-threaded in-process
   - `TestModuleExecutorParallel` — one thread per module (default when `TRUN_HAVE_THREADS`)
   - `TestModuleExecutorFork` — fork a sub-process per module (default on Linux when `TRUN_HAVE_FORK`)
5. Each module executes: `test_main` (global) → module main → test cases → module exit → `test_exit` (global)
6. Results collected into `TestResult`s and printed via the active reporting module

### Key source directories

| Path | Purpose |
|---|---|
| `src/testrunner/` | Core runner: `TestRunner`, `TestModule`, `TestFunc`, `Config`, `ResponseProxy` |
| `src/testrunner/ext_testinterface/` | Public headers installed alongside `trun`: `testinterface.h` (V2), `testinterface_v1.h` |
| `src/testrunner/reporting/` | `reportconsole`, `reportjson`, `reportjsonext` — implement `ResultsReportPinterBase` |
| `src/testrunner/embedded/` | `dynlib_embedded`, stripped logger — only compiled into `trunlib` |
| `src/testrunner/mcu/` | Self-contained zero-alloc MCU engine (engine #3): `StaticVector`/`StrView`, registry, `setjmp`/`longjmp` `ITesting`, console+sink reporter, `trunmcu` facade — compiled into `trunmcu`/demos only, never the desktop core |
| `src/testrunner/tests/` | Internal unit tests (compiled into `trun_utests.so`) |
| `src/coverage/` | `CoverageRunner`, `BreakpointManager`, `SymbolResolver` — LLDB-based coverage logic |
| `src/shared/` | Cross-cutting utilities: `strutil`, `timer`, `glob`, `dirscanner`, `testlibversion` |
| `src/shared/ipc/` | Binary IPC protocol (encoder/decoder/serializer) used to communicate between forked sub-processes and the main runner |

### Compile-time feature flags

| Define | Effect |
|---|---|
| `TRUN_HAVE_FORK` | Enables `TestModuleExecutorFork` (per-module subprocess via `fork`); desktop-only. Without it the module loop runs sequentially. |
| `TRUN_HAVE_EXCEPTIONS` | Catches C++ exceptions inside test cases (cpptrace unwind); required for the threaded executor's forced-termination throw. |
| `TRUN_HAVE_EXT_REPORTING` | Enables JSON reporting modules |
| `TRUN_USE_V1` | Forces V1 test interface (needed on Windows; also selects V1 for the MCU engine, e.g. `trunmcu_demo_v1`) |
| `TRUN_MCU_MAX_TESTFUNCS` / `TRUN_MCU_MAX_MODULES` / `TRUN_MCU_MSG_BUF_LEN` / `TRUN_MCU_SINK_MAX_RETRY` | MCU engine (engine #3) compile-time capacities; sane defaults, override per project. See `src/testrunner/mcu/mcu_config.h`. |
| ~~`TRUN_EMBEDDED_MCU`~~ | **Removed** in step 3. The zero-alloc MCU engine is a separate `src/testrunner/mcu/` implementation selected by CMake wiring, not a define (impl-swap, no `#ifdef`). |

**Threading is no longer a compile flag.** Test cases always run in their own thread
(the single `TestFuncExecutorThreaded`); every current target is threaded. The old
`TRUN_HAVE_THREADS`, `TRUN_EMBEDDED`, and `TRUN_SINGLE_THREAD` macros were removed —
targets differ by **swapping implementations** (executor / module-executor factories +
base classes) and per-target CMake wiring, not by `#ifdef`. `trunlib` is the
threaded-but-no-fork desktop-embedded engine (it links fmt + cpptrace and is C++20).
The **MCU engine** (`trunmcu`, engine #3, step 3 Phase A) is the genuinely no-thread /
no-exception / no-heap / zero-alloc path — a separate `src/testrunner/mcu/` implementation
compiled *for the target* by the embedder (a host-validation lib only; **not installed**).
See `todo/embedded_mcu_step3.md`.
`TRUN_SINGLE_THREAD` still exists *only* inside the frozen `testinterface_v1.h` as a
user-side knob for the V1 assert macro — we no longer define it in any target.

### IPC between processes

When `TRUN_HAVE_FORK` is active (Linux default), the main `trun` process forks one child per module. Children communicate results back via FIFO-based IPC (`src/shared/unix/IPCFifoUnix.cpp`) using a binary protocol defined in `src/shared/ipc/`. The coverage tool (`tcov`) uses a separate IPC channel (`ipc_tcov`) to receive breakpoint hit data from the `trun` process it runs inside LLDB.

### ResponseProxy and ITesting

`TestResponseProxy` (`src/testrunner/responseproxy.h`) is the concrete implementation of the `ITesting` C interface that is handed to test cases at runtime. It receives calls to `Error`, `Fatal`, `Abort`, `AssertError`, `SetPreCaseCallback`, `CaseDepends`, `ModuleDepends`, and `QueryInterface`, and routes them back to the runner.

### Versioning of testinterface

The public `testinterface.h` uses `__attribute__((weak))` symbols to detect which version a compiled test library was built against. V1 used value-based callbacks; V2 requires pre/post callbacks to return `kTR_xxx` result codes. Windows does not support `weak` symbols so must always use V1.

**FUNDAMENTAL — the external interface headers are a frozen contract.** Both
`ext_testinterface/testinterface.h` (V2) and `ext_testinterface/testinterface_v1.h` (V1, via
`TRUN_USE_V1`) are deployed in large real projects running on current `trun`. **Do not change
either header** — V1 in particular must keep working. This is load-bearing for the execution
engine: V1's threaded `TR_ASSERT` expands to a bare `AssertError(...)` with **no `return`**
(`AssertError` is `void` in V1), so the only way a failing V1 assert can stop a test mid-body in
a threaded build is for the runner to **force-terminate the thread** inside `AssertError`. That
is why any V1 library is auto-promoted to `kThreadedWithExit` (`testrunner.cpp`) and why forced
thread termination can never be removed. The cooperative-return (V2) vs forced-kill (V1)
distinction is interface-version-driven, not an optional runtime flag.


## Coding Standards

### General
- Always use curly braces for all control flow, even single-line bodies
- C++17 standard throughout
- RAII everywhere, no raw owning pointers

### Formatting
- 4-space indentation, no tabs
- Opening brace on same line as statement (K&R style)
- One blank line between methods

### Naming
- PascalCase for classes and structs: `GraphicsDevice`
- PascalCase for methods and variables: `InitDevice()`
- UPPER_SNAKE_CASE for constants: `MAX_BUFFER_SIZE`
- Never prefix member variables with `m_`: `m_width`
- Convey intent with variables: `bool isSomething`
- camelCase for variables: `isSomething`
- Avoid shadowing for parameters when same as local variables (like CTOR, in that case prefix param with '_')
- Write code top-down, no function should appear before it is used.

### C++ Specifics
- Prefer `nullptr` over `NULL`
- Use `auto` sparingly, only when type is obvious from context
- Mark all single-argument constructors `explicit`
- Prefer range-based for loops

## Tracking of work items
The applications `tcov` has a TODO list on the top section of the file.
The TODO follows a pattern I used since many years, items are marked/prefixed as follows:
- `-`, an open item
- `+`, work is in progress (no notion of how long or state of the progress)
- `!`, work has completed in one way or another (can be deprecation)

Sometimes the notation `[!]` is used, this denotes a deprecation items and normally with an explanation

## Running unit-tests
The `trun` can run it's own unit-tests. However, be aware that some tests may break full execution and should
perhaps only be executed when exactly those scenarios are tested.
I would suggest avoiding the following modules.
* abortall, various execution abort tests
* exception, various exception stressing tests

When running the full test-suite (05.07.2026) with the following parameters:
```shell
   trun -m !abortall,!exception,- lib/libtrun_utests.dylib
```
It will execute 110 tests and fail 13 (fork == sequential; the 13 are intentional self-fails).
(Was 102/15 before the testrunner-core bug-fix merge 0a7989d — the extra tests are the
new regression cases and the Fatal/Abort fix legitimately un-fails a couple of cases.)