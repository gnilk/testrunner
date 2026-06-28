## Feature: Proper signal handling (crash isolation for the no-fork path)

### Background / motivation

Crash isolation in `trun` today comes **only** from the per-module `fork`
(`TestModuleExecutorFork`). A test that raises a fatal signal — `SIGSEGV`,
`SIGABRT` (failed libc `assert`, `std::abort`, double-free), `SIGBUS`, `SIGFPE`,
`SIGILL` — kills only that module's child process; the parent detects the
abnormal exit and carries on. There is currently **no signal handler installed
anywhere** in the runner.

The per-test `std::thread` (`TestFuncExecutorParallel`, the `kThreaded` default)
does **not** provide crash isolation — a thread shares the address space, so a
fatal signal is delivered process-wide. The thread exists for a different
reason: *mid-body exit* of a test case (V1 `AssertError` returns `void`, so the
only way out is to unwind/kill the running thread — see
`responseproxy.cpp:TerminateThreadIfNeeded` and `kThreadedWithExit`). Keep these
two mechanisms conceptually separate: threads = intentional abort, signals =
unintentional crash.

The gap is the **`--sequential` / no-fork path**, which is the *debugging*
use-case: it keeps everything in one process so a debugger (CLion, lldb, gdb)
retains ownership of `trun` and can trap/step — which is impossible across a
fork. In that mode a single crashing test takes down the whole runner and you
lose every result gathered so far.

Goal: install fatal-signal handling so that, in the no-fork path, a crashing
test is reported (which test, which signal, ideally a backtrace) instead of
silently nuking the process — and, as a second step, optionally recover and
continue with the next test.

### Important reality check

A signal handler is **best-effort**, never as clean as fork. When a fatal
signal fires the process may already be in a corrupt state (e.g. the crash hit
mid-`malloc` while the allocator lock was held). `fork` remains the gold
standard for isolation; signal handling is a quality-of-life improvement for the
single-process debugging workflow, not a replacement.

POSIX only for now (Windows support was dropped — see `windows_support.md`).
Gate everything behind `#ifndef WIN32`. A future Windows path would use SEH /
`AddVectoredExceptionHandler`.

---

### Phase 1 — Crash *reporting* (low risk, do this first)

Catch the signal, report it usefully, exit cleanly. No attempt to resume.

- Install handlers for `SIGSEGV`, `SIGABRT`, `SIGBUS`, `SIGFPE`, `SIGILL` via
  `sigaction` with `SA_SIGINFO`. Do **not** touch `SIGTRAP`/`SIGINT` (leave to
  debugger / user).
- Install an alternate signal stack (`sigaltstack` + `SA_ONSTACK`) so a stack
  overflow (`SIGSEGV` with exhausted stack) can still be handled.
- In the handler, identify the currently-running test. Store a pointer to the
  active `TestFunc` / symbol name in a `thread_local`, set just before invoking
  the case (see wiring below). Handler reads only `volatile sig_atomic_t` /
  pre-stored pointers — async-signal-safe access only.
- Emit a crash report. Full symbolized traces are **not** async-signal-safe.
  Use cpptrace's two-step safe API: collect raw frames in the handler
  (`cpptrace::safe_object_frame` / the documented signal-safe path), resolve &
  symbolize *after* leaving the handler. cpptrace v0.7.1 is already linked.
- Write only with async-signal-safe calls inside the handler (`write(2)` to
  stderr), not `printf`/`fmt`/iostreams.
- Flush results gathered so far and exit non-zero with a clear message:
  `*** CRASH: test '<symbol>' caused <signal name> ***`.

### Phase 2 — Crash *recovery* (higher risk, opt-in)

Abandon the crashing test, mark it failed, continue with the next one.

- Set a `sigsetjmp` checkpoint immediately before invoking the test case; the
  handler does `siglongjmp` back to it. The jump buffer must be `thread_local`
  (the test runs in the per-test worker thread).
- On the non-zero return from `sigsetjmp`, synthesize a failure result for the
  current test (new crash result type, see below) and return control to the
  executor, which proceeds to the next case. NB: you cannot `throw` from a
  signal handler — recovery must be `siglongjmp`; only once back in normal
  context may you build the result (or throw, if convenient there).
- Re-arm / reset handler state correctly between tests (`SA_NODEFER`
  considerations; re-establish the checkpoint per case).
- Document and accept the residual hazards (see Risks). Consider marking results
  produced *after* a recovered crash as "process state may be unreliable", or at
  minimum log a warning, since the recovered process is potentially tainted.

---

### Wiring (where the code goes)

- New component: `src/testrunner/signalhandler.{h,cpp}` (POSIX impl under
  `src/testrunner/unix/`). Owns install/uninstall, the alt stack, the
  `thread_local` checkpoint + active-test pointer, and report formatting.
- Checkpoint set/restore wraps the actual test invocation in
  `TestFuncExecutorSequential::Execute` (`funcexecutors.cpp`, around the
  `InvokeTestCase(proxy)` call at ~line 193) — same spot as the existing
  `TRUN_HAVE_EXCEPTIONS` try/catch scaffolding, so the two failure paths sit
  side by side.
- Handler installation at startup in `src/app/trun/trun.cpp` (or lazily when the
  no-fork execution path is selected).
- Result type: extend `TestResult` with a crash state (e.g.
  `kTestResult_Crashed` and/or a `kFailState::Crashed`) carrying the signal
  number/name; populate it in `testfunc.cpp:CreateTestResult` analogous to the
  exception path. Wire through `reportconsole` and the JSON reporters.
- Config: add `Config::Instance().catchTestCrash` plus a CLI flag
  (`--catch-crash` / opt-out `--no-catch-crash`). See defaults below.

### Config / defaults — decide

- Default ON for the no-fork (`--sequential`) path, opt-out via flag — this is
  the path that benefits.
- Auto-disable (or default off) when `isCoverageRunning` is set: `tcov` drives
  `trun` under LLDB with its own breakpoint/signal handling; our handlers would
  fight it.
- Debugger interaction: when running under CLion/lldb/gdb the debugger traps the
  signal first regardless. That's desired while debugging — document that the
  user can let the debugger handle it, or configure the debugger to pass the
  signal on to let our handler run. Having handlers installed does not break
  debugging.
- In the fork path our handlers are largely redundant (the child crash is
  already caught by the parent). Optionally install them in the child too, so a
  crash in one test of a module no longer discards the remaining tests' results
  in that module child — nice-to-have, not required.

### Risks / open questions

- `malloc`/lock state: a `siglongjmp` out of a crash that happened inside the
  allocator can leave the lock held → next allocation deadlocks. Main argument
  for keeping Phase 2 opt-in.
- Tainted global/system state after recovery: a crashed test may have corrupted
  shared or OS-level state; subsequent tests run in a poisoned process. fork
  stays the gold standard.
- Stack overflow: requires the alt signal stack (covered in Phase 1) or the
  handler itself can't run.
- Reentrancy: a second fatal signal during handling/recovery — block fatal
  signals during the handler or handle re-entry defensively.
- Signal vs. thread: disposition is process-wide but a handler may run on any
  thread; ensure the checkpoint/active-test pointers are the worker thread's
  `thread_local`s.
- C test frames: recovery unwinds back to our C++ frame via `siglongjmp`, which
  is fine; no C++ destructors run in the abandoned test (acceptable for a crash).

### Task list

- [Phase 1] Add `signalhandler.{h,cpp}` with `sigaction`-based install + alt stack
- [Phase 1] `thread_local` active-test pointer, set before `InvokeTestCase`
- [Phase 1] Async-signal-safe crash report (signal name + symbol) via `write(2)`
- [Phase 1] cpptrace signal-safe backtrace (collect in handler, resolve after)
- [Phase 1] Flush partial results + clean non-zero exit
- [Phase 2] `sigsetjmp`/`siglongjmp` checkpoint around the test invocation
- [Phase 2] Synthesize crash result and continue to next test
- [Phase 2] Re-arm handler state per test; handle reentrancy
- Add `kTestResult_Crashed` / `kFailState::Crashed` to `TestResult` + reporters
- Add `Config::catchTestCrash` + `--catch-crash` / `--no-catch-crash`
- Suppress handlers when `isCoverageRunning` (tcov/LLDB owns signals)
- Document debugger pass-through behaviour in the help / README
- (Optional) install handlers in fork children to preserve same-module results
