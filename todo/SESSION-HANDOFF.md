# Session handoff — 2026-06-29

Pick-up notes for continuing the bug-fix work on a clean slate.

## Repo state
- On branch **`dev`**, **in sync with `origin/dev`** (pushed through `b6b0a20`).
- Working tree (intentional / not mine, leave alone):
  - `src/app/trun/trun.cpp` — uncommitted CLion working-dir debug comment.
  - `.DS_Store`, `src/testrunner/.DS_Store` — untracked.
- Build dir: `cmake-build-debug` (ninja). Artifact: `lib/libtrun_utests.dylib`.

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

(Both feature branches were merged then deleted. Previous session's work —
platform macros, IPC framing #1/#3/#4/#9, multi-assert #7, resultsummary
de-dup, global double-count — is in earlier `dev` history / `todo/done/`.)

## Open work — suggested order
Ranked by impact (1 = next). Branch for multi-file/function; small in-place
fixes can go straight to `dev` (see memory `branch-vs-direct-commit`).

1. **`todo/open-bugs.md`** — dead code (`old_Config_FromArguments`, legacy
   `TestModuleExecutorParallel`), `--continue_on_assert` triple-check. Small.
2. **Coverage/tcov sweep** — deferred; experimental, dead code there is
   intentional (see memory `coverage-tcov-experimental`). Includes
   `SymbolResolver::IsInProject` no-op noted in `open-bugs.md`.
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
