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

## Guiding principle — simplify; prefer separate files over #ifdefs

The driving force for this rewrite is **simplification**: better defaults and options
that actually make sense, with as few compile-time conditionals as possible.

- **Prefer per-target implementation files** (`trun` app / `trunlib` / `trunembedded`)
  over `#ifdef`-laden shared files. Each target should read as straight, top-down code
  for *its* use case — not one file you mentally re-compile in your head per macro.
- **Conditionals are still allowed where they genuinely pull their weight** — small,
  local, well-justified. The goal is fewer macros, not zero at any cost.
- **But do not trade #ifdefs for copy-paste.** No ~80% duplicated files. Genuinely
  shared logic stays factored into a common core; only what actually *differs* per
  target gets its own concrete implementation.

In short: split on the axis of "what differs between targets", keep the rest shared,
and let the absence of `#ifdef`s be the readability win.

---

## Analysis (2026-06-29) — current state of the execution layer

Folded in from `open-bugs.md` during the dead-code sweep, because the embedded
cleanup overlaps this rewrite and should be driven from here, not piecemeal.

### Build / define matrix  (updated — step 2, 2026-06-30)

`TRUN_HAVE_FORK` is set in `cmake/TrunCommonOptions.cmake` (APPLE + UNIX). After step 2,
**threading is unconditional** (every target is threaded) — `TRUN_HAVE_THREADS`,
`TRUN_EMBEDDED`, and `TRUN_SINGLE_THREAD` are gone. Targets now differ only by FORK +
EXCEPTIONS + the per-target source/CMake wiring.

| Target          | threaded | FORK | EXCEPTIONS | common_options | C++ |
|-----------------|:--------:|:----:|:----------:|:--------------:|:---:|
| `trun`          | ✓        | ✓    | ✓          | ✓              | 20  |
| `trun_utests`   | ✓        | ✓    | ✓          | ✓              | 20  |
| `trunlib`       | ✓        | —    | ✓          | ✗ (links fmt+cpptrace) | 20 |
| `trunembedded`  | ✓        | —    | ✓          | ✗ (links trunlib)      | 20 |
| `trunmcu` (#3)  | —        | —    | — (`-fno-exceptions -fno-rtti`) | ✗ (no libs) | 20 |

`trunlib` is the desktop-embedded engine: threaded (isolation + mid-body termination),
no fork. It now links fmt + cpptrace (the shared core's arg-parsing + exception unwind)
and is C++20. The genuinely lean, no-thread/no-exception/no-heap path is the step-3 MCU
engine (`trunmcu`, Phase A implemented) — a separate `src/testrunner/mcu/` implementation
selected by CMake wiring, **not** the old `TRUN_EMBEDDED_MCU` define (which was removed).

### Current reality: embedded == the full engine with threads/fork #ifdef'd out

`trunlib`/`trunembedded` compile the *entire* desktop core (`TestRunner`,
`TestModule`, `Config`, `ResultSummary`, `responseproxy`, …) with `TRUN_HAVE_THREADS`
and `TRUN_HAVE_FORK` simply undefined. It works, but it's exactly the "core reused
but not designed for embedded" problem this doc opens with. `trunembedded.cpp`
doesn't even use `Config::FromArguments` — it sets `Config` directly via `RunTests()`.

### Target engine roadmap (3 engines)

1. **Desktop `trun`** — lock down the use case:  **[! DONE 2026-06-29, branch
   `rewrite/func-executor-unification`]**
   - Module-forking is the *only* parallelism, optional, default on; `--sequential`
     turns it off. (The thread-per-module executor is already deleted — it was dead.)
   - Test cases **always** run in their own thread (isolation + mid-body termination).
   - Remove the other per-function execution models — collapse to one threaded executor.
   - Done: three func executors → `Sequential` + one `TestFuncExecutorThreaded`
     (`std::thread`); deleted `TestFuncExecutorParallelPThread` (redundant under
     exceptions). `kThreaded`/`kThreadedWithExit` enum kept as the forced-mode signal
     (V1 + `--allow-thread-exit`). Also fixed V2 `Fatal`/`Abort` to force-terminate
     mid-body (`Error` stays soft) via `TerminateThreadIfNeeded(bool alwaysTerminate)`.
     Suite unchanged (102/15 fork==sequential); only `_test_rust_fatal` output moved
     (stops after the first `Fatal`). External headers untouched (frozen contract — see
     CLAUDE.md). Embedded targets still build.
   - **Follow-on [! 2026-06-30, `dev` `cfbd04a`]:** the desktop fork module-executor was
     hardened — bounded-window concurrency (`--max-concurrency`, default ≈ CPU cores)
     instead of fork-all-then-spin, one log+`Kill()` per timeout/crash, incremental IPC
     drain, and a "Modules incomplete: N" summary section with non-zero exit when a module
     can't finish. Suite still 102/15 (fork==sequential).
2. **`trunlib` (embedded-for-desktop)** — purpose-built, no fork, but *uses the
   threaded function executor* (isolation + mid-body termination). For regular
   Linux/macOS/Windows "trun-as-library" use (memory model + speed).
   **[! DONE 2026-06-30, branch `rewrite/embedded-engine-step2`]** trunlib is now
   threaded (+exceptions, links fmt+cpptrace, C++20); removed the `TRUN_HAVE_THREADS`,
   `TRUN_EMBEDDED`, `TRUN_SINGLE_THREAD` macros (threading is unconditional, targets
   differ by impl-swap + CMake wiring, not `#ifdef`); deleted the dead
   `old_Config_FromArguments` + `ParseNumber`. Suite still 102/15; trunembedded runs
   threaded; trv1/trv2 libs pass.
3. **Embedded MCU** — thin, zero-alloc, compile-time buffers.
   **[+ PHASE A IMPLEMENTED 2026-06-30, branch `rewrite/embedded-engine-step3`, feature
   commit `ffd6814` (pushed to `origin`, not merged; doc follow-ups on top). Full design +
   impl notes in `todo/embedded_mcu_step3.md`. Phase B (cross toolchain/board) deferred.]** The
   self-contained engine lives in `src/testrunner/mcu/` (mcu_static / mcu_config /
   mcu_report / mcu_testing / mcu_runner / trunmcu) with demo + CMake in `src/app/trunmcu/`,
   selected by CMake wiring not `#ifdef`s in the desktop core. Delivered per the settled
   design: (a) **host-validated** — builds/runs on macOS/Linux behind the existing facade;
   (b) **`setjmp`/`longjmp`** mid-body abort (V1 forced assert + Fatal/Abort; V2
   cooperative); (c) **fixed-count constants** + **name-by-pointer** into caller-owned
   literals (no copy, no `MAX_NAME_LEN`, no arena) + `constexpr kStaticFootprintBytes`
   (4136 B defaults); (d) dropped deps / JSON+file reporting / heap var-args logging /
   `FromArguments` (pre/post hooks kept); (e) overridable `SetOutputSink` (default stdout,
   `OutputSinkResult` return). V1 vs V2 is compile-time (`TRUN_USE_V1`), like trv1/trv2.
   No heap/threads/exceptions/RTTI (verified: `nm` shows no `operator new`/`malloc`, builds
   `-fno-exceptions -fno-rtti` clean). The two `TRUN_EMBEDDED_MCU` `#ifdef` stubs
   (`responseproxy.cpp`, `reportingbase.cpp`) were **removed** (impl-swap, no new `#ifdef`s).
   `trunmcu` is a **host-validation lib only — NOT installed**; the engine is compiled for
   the target by the embedder. This is where `TRUN_HAVE_EXCEPTIONS` / `TRUN_HAVE_FORK` get
   the same impl-swap treatment — the engine simply doesn't include the fork path. The
   desktop fork path it swaps *out* is now the hardened, windowed one (see engine #1
   follow-on).

### Key design decision to settle BEFORE coding the sweep  ✅ RESOLVED (branch `rewrite/func-executor-unification`)

Resolution: under `TRUN_HAVE_EXCEPTIONS` (every real desktop build) forced termination
is a thrown `TestAbortException` that unwinds identically on a `std::thread`, so the
pthread executor was redundant and was deleted. One `std::thread` executor remains; the
`pthread_exit` fallback survives only as the (currently-unreachable) no-exceptions path
inside `TerminateThreadIfNeeded`. The forced-vs-cooperative distinction is kept as the
`kThreadedWithExit` enum value (set by V1 auto-promotion / `--allow-thread-exit`).
Original analysis kept below for context.

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
atomic push once the replacement passes the suite.

**Status 2026-06-30:** the MCU engine (engine #3) Phase A is now built *alongside* per this
plan (branch `rewrite/embedded-engine-step3`, `ffd6814`) — `trunlib`/`trunembedded` remain
the untouched baseline. No retirement yet; that stays deferred (and trunlib is a shipped
desktop lib, so it is not going away — only the *old MCU stubs* were removed).

