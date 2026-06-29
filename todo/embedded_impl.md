## New implementation for embedded applications - TRUN only

The embedded API is thin. But behind the scenes most of the core concepts are reused. These core concepts are not designed
for embedded use - even if they work. 
A new 'engine' is required. Using zero allocation principles, static buffers etc (compile time changed).
There are actually two types of embedded engines
1) embedded for MCU like projects
2) embedded as compiled with the binary

While '1' is pretty clear - this is a thin engine with compile time allocations and everything for 
proper embedded use.

However, '2' is a bit different. This is intended for regular Linux/macOS/Windows use - where you
compile the testrunner with your executeable ('trun-as-library'). The main reasons for this would be:
1) Differences in memory handling between Linux/macOS/Windows and dynamic libraries
2) Execution speed

Both use cases should be supported - but not necessarily by the same engine..

---

## Analysis (2026-06-29) — current state of the execution layer

Folded in from `open-bugs.md` during the dead-code sweep, because the embedded
cleanup overlaps this rewrite and should be driven from here, not piecemeal.

### Build / define matrix

`TRUN_HAVE_FORK` is set in `cmake/TrunCommonOptions.cmake` (APPLE + UNIX), **not**
per-target. The embedded targets deliberately don't link `trun_common_options`,
so they get neither FORK nor THREADS.

| Target          | THREADS | FORK | EMBEDDED | SINGLE_THREAD | common_options |
|-----------------|:-------:|:----:|:--------:|:-------------:|:--------------:|
| `trun`          | ✓       | ✓    | —        | —             | ✓              |
| `trun_utests`   | ✓       | ✓    | —        | —             | ✓              |
| `trunlib`       | —       | —    | ✓        | ✓             | ✗              |
| `trunembedded`  | —       | —    | ✓        | ✓             | ✗ (links trunlib) |

Consequences that matter: **no build ever has `THREADS && !FORK`**, and **none has
`FORK && EMBEDDED`**. That single fact makes several things dead (below).

### Current reality: embedded == the full engine with threads/fork #ifdef'd out

`trunlib`/`trunembedded` compile the *entire* desktop core (`TestRunner`,
`TestModule`, `Config`, `ResultSummary`, `responseproxy`, …) with `TRUN_HAVE_THREADS`
and `TRUN_HAVE_FORK` simply undefined. It works, but it's exactly the "core reused
but not designed for embedded" problem this doc opens with. `trunembedded.cpp`
doesn't even use `Config::FromArguments` — it sets `Config` directly via `RunTests()`.

### Target engine roadmap (3 engines)

1. **Desktop `trun`** — lock down the use case:
   - Module-forking is the *only* parallelism, optional, default on; `--sequential`
     turns it off. (The thread-per-module executor is already deleted — it was dead.)
   - Test cases **always** run in their own thread (isolation + mid-body termination).
   - Remove the other per-function execution models — collapse to one threaded executor.
2. **`trunlib` (embedded-for-desktop)** — purpose-built, no fork, but *uses the
   threaded function executor* (isolation + mid-body termination). For regular
   Linux/macOS/Windows "trun-as-library" use (memory model + speed).
3. **Embedded MCU** — thin, zero-alloc, compile-time buffers. The `TRUN_EMBEDDED_MCU`
   stubs (`responseproxy.cpp:383`, `reportingbase.cpp:16`) are the only forward-looking
   hooks today; the macro is defined by no target yet.

### Key design decision to settle BEFORE coding the sweep

"Always one threaded executor" + "mid-body termination" are coupled. Today there are
two thread executors *because* of termination:
- `TestFuncExecutorParallel` (`std::thread`, `kThreaded`) — the current default
  (`config.h`: `testExecutionType = kThreaded`). Cannot be force-killed.
- `TestFuncExecutorParallelPThread` (`kThreadedWithExit`) — can terminate mid-body
  (`pthread_exit`), backing `TestResponseProxy::TerminateThreadIfNeeded()`. On desktop
  with `TRUN_HAVE_EXCEPTIONS` the abort instead throws `TestAbortException`.

So the "one true executor" is really: **run-in-thread + terminate-via-exception when
`TRUN_HAVE_EXCEPTIONS`, else `pthread_exit`.** Unifying these two into one is the real
work of step 1, and it touches the `abortall`/`exception` test modules (fragile — see
CLAUDE.md). Design it here first.

### Embedded-specific dead code (clean during the rewrite, not before)

These are dead today but live in files the rewrite will likely replace, so don't churn
them piecemeal:
- `old_Config_FromArguments` + the `TRUN_EMBEDDED` branch of `Config::FromArguments`
  (`config.cpp`) — only call site is commented out; returns `kError` under EMBEDDED;
  embedded never calls it (uses `RunTests()`).
- `#if defined(TRUN_HAVE_FORK) && defined(TRUN_EMBEDDED)` blocks (`config.cpp:37`
  `ParseNumber`, and ~`:395`) — impossible define combo per the matrix above.

### Sequencing

Keep the current embedded engine as the working baseline (it's the only producer of
`trunlib`/`trunembedded`). Build the new engine(s) alongside; retire the old in one
atomic push once the replacement passes the suite. This is a feature-sized branch —
not greenlit for the dead-code-cleanup session.

