# Session handoff — 2026-06-30

Pick-up notes for continuing the engine rewrite on a clean slate.

## Repo state
- Engine-rewrite **steps 1 and 2 are done**. Step 1 merged to `dev`; step 2 is on
  branch **`rewrite/embedded-engine-step2`** (merge to `dev` when happy, per the prior
  merge pattern). **Nothing pushed to origin yet.**
- Working tree (intentional / not mine, leave alone):
  - `src/app/trun/trun.cpp` — uncommitted CLion working-dir debug comment (left unstaged
    on purpose; not part of any rewrite commit).
  - `.DS_Store`, `src/testrunner/.DS_Store` — untracked.
- Build dir: `cmake-build-debug` (ninja). Artifact: `lib/libtrun_utests.dylib`.
- **Build gotcha hit this session:** this build dir was stale on fmt v10 (the v12 pin
  from commit `3c897f3`, 2026-05-12, had never been reconfigured here). After
  `cmake ..`, gnklog can link-fail with undefined `fmt::v10::vprint/vformat`. Fix:
  `rm -rf _deps/gnklog-build/CMakeFiles/gnklog.dir && ninja gnklog` (recompiles gnklog
  against the now-current fmt v12). Not a code issue.

## Done this session — engine rewrite step 2 (threaded trunlib + macro removal)
Branch `rewrite/embedded-engine-step2`. Plan: `todo/embedded_impl.md` (roadmap step 2,
build/define matrix updated). Made `trunlib` use the threaded executor (per-case
isolation + mid-body termination, no fork) and **removed three compile macros** —
`TRUN_HAVE_THREADS`, `TRUN_EMBEDDED`, `TRUN_SINGLE_THREAD` — replacing them with
implementation selection per the `prefer-impl-files-over-ifdefs` principle:
- Threading is now **unconditional** (every target is threaded). Some `THREADS` guards
  were really FORK guards and were re-attributed (`resultsummary.cpp` IPC includes;
  `moduleexecutors.cpp` `<thread>`). Fixed the `config.cpp` CTOR coupling bug
  (exec-type was keyed on FORK; now always `kThreaded`, module-type keys on FORK).
- Deleted the dead `old_Config_FromArguments` + `ParseNumber` from `config.cpp`;
  `FromArguments`/`ArgParser` now compile unconditionally (added `#include <fmt/format.h>`).
- CMake: `trun`/`trun_utests` drop the dead `TRUN_HAVE_THREADS` define; `trunlib` +
  `trunembedded` drop `SINGLE_THREAD`/`EMBEDDED`, **gain `TRUN_HAVE_EXCEPTIONS`, link
  `fmt` + `cpptrace::cpptrace`, and move to C++20** (ArgParser needs it). `TRUN_SINGLE_THREAD`
  still lives in the frozen `testinterface_v1.h` as a user knob — we just stop defining it.
- **Consequence:** `trunlib` (and embedders) now carry a cpptrace + fmt dependency. The
  lean no-thread/no-exception path is step 3 (MCU).
Verified: all six targets build (`trun trun_utests trunlib trunembedded trv1_utest
trv2_utest`); suite **102/15 fork==sequential**; `trunembedded` runs threaded;
`exception`/`abortall` modules unchanged; V1 (1.0.0) + V2 (2.0.0) libs pass.

## Done this session — engine rewrite step 1 (executor unification)
Branch `rewrite/func-executor-unification`. Plan: `todo/embedded_impl.md`
(roadmap step 1 marked done). Collapsed the three function executors to two
(`Sequential` + one `TestFuncExecutorThreaded` on `std::thread`); deleted
`TestFuncExecutorParallelPThread` (redundant — forced termination is a thrown
`TestAbortException` that unwinds fine on a `std::thread`; no target builds
threads-without-exceptions). `kThreaded`/`kThreadedWithExit` enum kept as the
forced-mode signal (V1 auto-promote + `--allow-thread-exit`). Also fixed V2
`Fatal`/`Abort` to force-terminate mid-body (`Error` stays soft):
`TerminateThreadIfNeeded(bool alwaysTerminate)` — Abort/Fatal pass `true`,
Error/Assert pass `false`. Files: `funcexecutors.{h,cpp}`, `responseproxy.{h,cpp}`.
**No external header touched** (frozen contract — now documented in CLAUDE.md +
memory `external-interface-frozen`). Verified: trun/utests/trunlib/trunembedded all
build; canonical suite still **102/15 fork==sequential** (and under
`--allow-thread-exit`); only `_test_rust_fatal` output changed (stops after the first
`Fatal`, attributed to the test not the post-hook).

## How to verify (per CLAUDE.md)
```bash
cd cmake-build-debug && ninja trun trun_utests
# Canonical suite (excludes abortall/exception which break full execution):
./trun -m '!abortall,!exception,-' lib/libtrun_utests.dylib       # fork (default)
./trun --sequential -m '!abortall,!exception,-' lib/libtrun_utests.dylib
```
Expected now: **fork == sequential == 102 executed / 15 failed** (15 are
intentional self-failure tests). Fork count is deterministic.

## Done 2026-06-29 (all merged to `dev` and pushed)
1. **Config arg bugs** (`done/config_arg_bugs.md`) — `1affcd7`, direct to dev.
   `-t` without `-m` no longer clobbers the module filter (deleted duplicate
   block); `-d` (dump) decoupled from `-D` (disable RTLD_DEEPBIND). Tests:
   `test_config.cpp` (module `config`).
2. **Fork executor cluster #1–#4** (`done/fork_executor_fixes.md`) — branch
   `fix/fork-executor-lifetime`, merge `dd8337c`. Local owning
   `vector<unique_ptr<SubProcess>>` + `unique_ptr<Process>` (no static
   accumulation / leaks); output dumped once after finish (measured 6.2M → 84
   lines); `state`/`exitStatus` atomic + initialized, `name`/`tStart`/`proc`
   set before the worker spawns; `Wait()`/dtor always join. Removed the
   `threadDeadCounter` warning and dead commented loop.
3. **IPC robustness #2/#5/#6/#8** (`done/ipc_robustness.md`, now fully closed)
   — branch `fix/ipc-robustness-cleanup`, merge `b6b0a20`. FIFO stale-cleanup
   computes name first (#2); both `dynamic_cast`s null-checked (#5);
   `testResults` is `vector<unique_ptr<IPCTestResults>>` — no producer/consumer
   leaks (#6); `IPCAssertError` deserializer matches `kMsgType_AssertError`
   (#8). Tests: `test_ipcmsg_deserializer_ids`, `test_ipcfifo_nix.cpp`.
4. **Dead-code cleanup + embedded roadmap** — branch
   `cleanup/dead-module-parallel`, merge `6fae78e`. Deleted dead
   `TestModuleExecutorParallel` (thread-per-module: unreachable in every build —
   desktop has FORK so factory maps `kParallel→fork`, embedded lacks THREADS so
   it never compiled); re-guarded the fork-path `<thread>` include on FORK.
   Simplified `--continue_on_assert` (4 `IsPresent`→2). Folded the embedded
   analysis (build/define matrix, 3-engine roadmap, executor-unification design,
   embedded dead-code inventory) into `todo/embedded_impl.md`; updated
   `open-bugs.md`. Verified trun/utests **and** trunlib/trunembedded build;
   suite unchanged.

(Both feature branches were merged then deleted. Previous session's work —
platform macros, IPC framing #1/#3/#4/#9, multi-assert #7, resultsummary
de-dup, global double-count — is in earlier `dev` history / `todo/done/`.)

## Open work — suggested order
Ranked by impact (1 = next). Branch for multi-file/function; small in-place
fixes can go straight to `dev` (see memory `branch-vs-direct-commit`).

`todo/open-bugs.md` is now effectively closed: the two dead-code items and the
`--continue_on_assert` simplification are done (merge `6fae78e`); the only
remaining live entry there is the coverage `SymbolResolver::IsInProject` no-op,
which belongs to the deferred coverage sweep below.

1. **Embedded engine rewrite** (`todo/embedded_impl.md`) — multi-step.
   **Steps 1 (executor unification) and 2 (threaded trunlib + macro removal) are
   DONE** (see above; step 1 merged, step 2 on its branch).
   - **Step 3 — thin zero-alloc MCU engine** (`TRUN_EMBEDDED_MCU`): the genuinely
     no-thread, no-exception, zero-alloc swapped implementation. **NOT greenlit** —
     confirm intent/design before coding. This is where `TRUN_HAVE_EXCEPTIONS` and
     `TRUN_HAVE_FORK` should get the same impl-swap treatment (no new `#ifdef`s).
2. **Coverage/tcov sweep** — deferred; experimental, dead code there is
   intentional (see memory `coverage-tcov-experimental`). Includes
   `SymbolResolver::IsInProject` no-op (the last live item in `open-bugs.md`).
3. **`todo/signal_handling.md`** — planning doc, NOT greenlit; feature, not a
   bug. Do not start without explicit go-ahead.

## Key decisions / gotchas to remember
- **Module dependencies must be declared in `test_main`**, never in the module
  main (`test_<module>`) — declaring in module main would force a
  mid-execution rollback/abort, deliberately rejected. Deps are
  discouraged-but-supported.
- Fork still **re-executes** dependency modules in multiple children (benign
  perf cost); only the duplicate **reporting** was the bug (de-duped earlier).
- Forked mode is for CI/CD speed (large suites), usually `-r json` consumed by
  a web-app. `--sequential` is for debugging through tests (CLion owns
  execution).
- macOS `trun` is built with ASan; macOS has no LSan, so leaks aren't caught
  automatically — lifetime fixes were verified by exercising the now-destroyed
  objects through the ASan-clean fork run, not by a leak report.

## Conventions captured (memory + CLAUDE.md)
- Resolved todo docs get an inline `✅ RESOLVED (branch)` tag **and** are moved
  to `todo/done/` once fully closed.
- Top-down code ordering; project CMake platform defines (`APPLE`/`LINUX`).
- TDD: write the failing test first where the code is unit-testable; lifetime/
  threading/UB fixes are verified via suite + ASan instead.
- Branch vs direct-commit rule (above).
