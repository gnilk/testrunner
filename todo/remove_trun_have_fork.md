# Remove `TRUN_HAVE_FORK`

Work package: retire the `TRUN_HAVE_FORK` compile-time flag so all desktop targets
compile from one uniform source, while keeping `trunlib` (the in-process embedded
engine) free of fork / subprocess / IPC / procspawn code.

Notation (per CLAUDE.md): `-` open, `+` in progress, `!` done.

Branch: `refactor/remove-have-fork` (off `dev`).

---

## Goal

`TRUN_HAVE_FORK` originally existed so the *same* source could compile for genuine
no-fork MCU targets. That reason is gone: the MCU engine (`trunmcu`, engine #3) is now a
separate implementation. The flag's only remaining job is trimming fork/IPC code out of
`trunlib`. We want to remove the macro and express the fork/no-fork split the project's
preferred way — implementation-file selection, not `#ifdef` (see the
"prefer impl files over #ifdefs" principle).

## Key insight (why this is subtle)

- **`--sequential` is runtime *selection*; the macro is compile-time *linkage*.** They are
  different axes. `--sequential` picks the sequential module executor over the fork one at
  runtime; the macro decides whether the fork executor (and `SubProcess` / IPC / procspawn)
  is *compiled/linked in at all*.
- **The fork model is re-exec, not `fork()`-in-place.** `SubProcess::Start` runs
  `Process(argv[0])` — it re-execs the `trun` CLI and passes it a `.so` to `dlopen`. That
  premise (a standalone runner exe + a loadable test library) does not exist for the
  embedded engine, where tests are registered in-process via `AddTestCase` into
  `DynLibEmbedded`. So the fork path is structurally N/A embedded — and moot at runtime once
  `trunlib` enforces `kSequential`.
- Because the linker keeps any symbol *referenced from a reachable function* (even behind a
  runtime `if`), removing the macro and doing nothing else would force `trunlib` consumers to
  link the IPC/procspawn sources (undefined-symbol errors otherwise). It is NOT
  "the linker drops the dead code".

### The IPC-drag sites (code that pulls SubProcess/IPC into a target)

Originally three, all `#ifdef TRUN_HAVE_FORK`:

1. `TestModuleExecutorFork::Execute` (`moduleexecutors.cpp`) → `SubProcess` → `Process`/IPC
2. `ResultSummary::SendResultToParentProc` (`resultsummary.cpp`, called unconditionally from
   `PrintSummary` when `isSubProcess`) → `IPCFifoUnix` + encoder
3. `int_tcov_begincov` (`responseproxy.cpp`, wired into the coverage interface table) →
   `IPCFifoUnix` — **the coverage RPC hook**

## Verified findings (2026-07-06)

- **The coverage RPC path is dead / disposable.** `int_tcov_begincov` (trun side) sends a
  `CovIPCCmdMsg` over `ipc_tcov` + `raise(SIGUSR1)`; `CoverageRunner::ConsumeIPC` (tcov side,
  `Coverage.cpp`) decodes it and calls `breakpointManager.CreateCoverageForSymbol`. It works
  end-to-end but is an abandoned experiment (code-driven, instrument-a-symbol-mid-run
  coverage — too slow/inconvenient; coverage is a develop-time activity).
  - **Verification:** commented out the tcov-side RPC consume (`HandleIPCInterrupt` dispatch
    in `Coverage.cpp::Process`), rebuilt `tcov`, ran
    `tcov -v -R base --target ./trun --symbols 'pucko::DateTime::*' -- --sequential -v -m datetime <pucko dylib>`.
    Result: clean build, exit 0, correct per-function `DateTime` coverage (25 tests), zero
    RPC-related fallout. The `--symbols` static-breakpoint path is untouched by removing the RPC.
  - Consequence: site #3 can be **deleted**, not isolated. `responseproxy.cpp` is its only
    IPC user, so removing it drops `responseproxy` as a drag site entirely → **3 sites → 2**.
- `coveragerpcbrige.{h,cpp}` — a second, redundant `CovIPCCmdMsg` sender — is already dead
  (commented out of the build in `src/app/trun/CMakeLists.txt`). Delete the files.
- **`QueryInterface` / `ITestingCoverage` stay for now.** The `QueryInterface` extension
  mechanism (V2-extension-without-a-version-bump) is experimental and unused outside its own
  tests, and in principle could be dropped from `ITesting` with zero impact on real projects —
  but that is out of scope here. Keep the coverage interface; only the RPC plumbing goes.

## Plan

### ! Step 0 — cleanup / setup (done 2026-07-06)
- ! Man-page rewrite landed on `dev` (`158a1c3`), independent of this work.
- ! Fresh branch created; verification branch + fork-seam WIP stash discarded (captured here).

### ! Step 1 — remove the dead coverage RPC (done 2026-07-06)
- ! `responseproxy.cpp`: gutted `int_tcov_begincov` to a no-op (kept as the
      `ITestingCoverage.BeginCoverage` hook target — interface + `QueryInterface` retained).
      Dropped all IPC includes (`ipc/*`, `unix/IPCFifoUnix.h`, `CoverageIPCMessages.h`) and the
      now-unused `<thread>`/`<signal.h>` → responseproxy no longer drags IPC into any target.
- ! `Coverage.cpp`/`.h` (tcov): removed `CreateIPCServer`/`ipcServer`, `HandleIPCInterrupt`,
      `ConsumeIPC`, `ReadIPCMessage`, `sig_IPC_INTERRUPT`, and the signal dispatch in `Process()`
      (now only breakpoint hits). `PrepareTrunExecution` no longer passes `--tcov-ipc-name`; still
      passes `--sequential`/`--coverage`. `sig_DYNLIB_LOADED` (SIGUSR1) dylib-load sync kept.
- ! Deleted `coveragerpcbrige.{h,cpp}` and `CoverageIPCMessages.{h,cpp}` (git rm); removed the
      latter from `sharedsrcfiles` (CMakeShared.cmake) and the stale commented refs in
      `src/app/trun/CMakeLists.txt`. Removed `tcov.cpp`'s 3 unused IPC includes.
- ! Removed the dead `coverageIPCName` Config field + `--tcov-ipc-name` parsing (config.{h,cpp}).
- ! Verified: `trun`, `trun_utests`, `trunlib`, `tcov` all build clean; internal suite
      `-m '!abortall,!exception,-'` = 116 executed / 13 intentional fails (unchanged baseline).
      Pucko `DateTime` tcov end-to-end smoke test PASSED (see Step 3).

### ! Step 2 — remove `TRUN_HAVE_FORK`  → **DONE, Option B** (2026-07-06)
Two IPC-drag sites remain (fork executor, `SendResultToParentProc`).

**Decision (gnilk): Option B — simple removal, accept inert linkage.** The "keep `trunlib`
lean" premise behind Option A was a pre-MCU artifact: `trunlib` and the MCU world used to share
an implementation, so fork/IPC had to be trimmable. The MCU engine (`trunmcu`) now has its own
implementation, so the desktop-embedded `trunlib` no longer needs to be byte-for-byte lean — we
lean on the linker + inertness instead. And `SendResultToParentProc` is only ever called when
`isSubProcess` is true, which can only be set by a forked child — impossible for `trunlib` once
`Initialize` pins `kSequential`. So the code is present but unreachable.

Note (linker reality): `--gc-sections`/dead-strip will NOT drop the fork/IPC code, because it is
referenced from always-reachable functions (`PrintSummary`, `TestModuleExecutorFactory::Create`)
behind runtime `if`s. So `trunlib`'s CMake must add the fork/IPC/procspawn sources or its
consumers get undefined-symbol errors. The code links, stays inert, and never executes.

(Rejected — Option A, the `fork_ipc.h` impl-swap seam keeping `trunlib` at zero fork/IPC: extra
per-target TUs + CMake wiring, no longer justified now that lean-`trunlib` isn't a goal.)

Done (Option B, all sites):
- ! Removed `TRUN_HAVE_FORK` from `cmake/TrunCommonOptions.cmake` (all 3 platform branches).
- ! Made the fork-only `Config` fields unconditional (`ipcName`, `moduleExecTimeoutSec`,
      `moduleExecConcurrency`) in `config.h`; unconditional `--module-timeout` / `--max-concurrency`
      / `--ipc-name` parsing (`config.cpp`), help text (`trun.cpp`), and `Config::Dump`.
- ! Default `moduleExecuteType` = `kParallel` (unconditional, CLI default); `trun::Initialize`
      (`trunembedded.cpp`) now pins `kSequential` — **load-bearing** guard against the embedded
      engine selecting the fork/re-exec path.
- ! `moduleexecutors.{h,cpp}`: unguarded `TestModuleExecutorFork`; `Create()` now constructs the
      fork executor lazily inside the `kParallel` case (a sequential-only run never constructs it).
- ! `resultsummary.cpp`: unguarded the IPC includes + `SendResultToParentProc` body.
- ! `src/app/trun/CMakeLists.txt`: `trunlib` now links `ipcsrcfiles` + `processsrcfiles` +
      `unix/IPCFifoUnix.cpp` + `unix/process_unix.cpp` (platform-guarded) so the inert fork path
      resolves; updated the stale `subprocess.cpp` + `CMakeShared.cmake` process-spawn comments.

### ! Step 3 — verify (done 2026-07-06)
- ! `trun`, `trun_utests`, `trunlib`, `tcov`, `trunembedded`, and the `trunmcu` demos all build
      clean; `grep -rn TRUN_HAVE_FORK src cmake` == 0 hits.
- ! Internal suite unchanged at the baseline: **116 executed / 13 intentional fails** — verified on
      the fork (default), `--sequential`, and `--max-concurrency 2 --module-timeout 60` paths.
- ! `trunlib` consumer links (Option B): `trunembedded` links against the added fork/IPC/procspawn
      sources and **runs in-process** (5 tests, 1 intentional fail, no fork) — the `kSequential`
      pin verified at runtime.
- ! Pucko `DateTime` tcov end-to-end smoke test PASSED (2026-07-06):
      `tcov -v -R base --target ./trun --symbols 'pucko::DateTime::*' -- --sequential -v -m datetime`
      `.../PuckoNew/cmake-build-debug/lib/libpucko_utests.dylib` → full per-function `DateTime`
      coverage report (exercised fns 80-100%, unexercised 0%), unchanged from before the RPC
      removal. Confirms the `--symbols` static-breakpoint path is intact.

## Related / deferred (not in this work package)
- `trunembedded.cpp` → `trunlib.cpp` rename + `trunembedded` retirement.
- Dropping `QueryInterface` / `ITestingConfig` / `ITestingCoverage` from `ITesting`.
- General `tcov` revival / experimental-code sweep.
