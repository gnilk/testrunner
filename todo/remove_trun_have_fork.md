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

### - Step 1 — remove the dead coverage RPC (verified safe; do first)
- - `responseproxy.cpp`: delete `int_tcov_begincov`'s IPC body. Decide: drop the coverage
      interface's `BeginCoverage` hook, or keep it as a no-op. Keep `QueryInterface` +
      `ITestingCoverage` themselves.
- - `Coverage.cpp` (tcov): remove `CreateIPCServer`/`ipcServer`, `HandleIPCInterrupt`,
      `ConsumeIPC`, `ReadIPCMessage`, and the `sig_IPC_INTERRUPT` dispatch in `Process()`.
      Keep `SIGUSR1`/`sig_DYNLIB_LOADED` handling (dylib-load detection shares that signal).
- - Delete `coveragerpcbrige.{h,cpp}` (already out of the build).
- - `CovIPCCmdMsg` / `CoverageIPCMessages.{h,cpp}`: remove if nothing references them after
      the above (check both trun and tcov).
- - Rebuild `tcov`; re-run the Pucko `DateTime` coverage command; confirm unchanged.

### - Step 2 — remove `TRUN_HAVE_FORK`
Two IPC-drag sites remain (fork executor, `SendResultToParentProc`). **Open decision:**

- **Option A — impl-swap seam (lean `trunlib`).** A `fork_ipc.h` seam with a real impl
  (`fork_ipc.cpp`, compiled by fork targets: `trun`, `trun_utests`, `tcov`) and a stub
  (`fork_ipc_stub.cpp`, compiled by `trunlib`). The fork executor + `SendResultToParentProc`
  live in the real impl; the module-executor factory reaches the fork executor only via
  `CreateForkModuleExecutor()` (returns `nullptr` in the stub → falls back to sequential).
  `trunlib` links zero fork/IPC/procspawn — byte-for-byte as lean as today. Cost: a couple of
  per-target TUs + CMake wiring. (This was the WIP that was sketched then discarded; recreate
  if chosen.)
- **Option B — simple removal (accept inert linkage).** Delete the macro, always-compile the
  fork/IPC paths, add the IPC/procspawn/subprocess sources to `trunlib` so it links, enforce
  `kSequential` in `Initialize`. Smallest diff, but `trunlib` (and its consumers) carry the
  never-executed fork/IPC code. The dead code is harmless functionally (guarded by
  `isSubProcess`/never-selected `kParallel`), just present.

Regardless of A/B:
- - Make the `TRUN_HAVE_FORK`-only `Config` fields unconditional (`ipcName`,
      `moduleExecTimeoutSec`, `moduleExecConcurrency`) and the `--module-timeout` /
      `--max-concurrency` parsing + help text.
- - Unify the default `moduleExecuteType` to `kParallel` (CLI default) and have
      `trun::Initialize` (in `trunembedded.cpp`) enforce `kSequential`. This becomes
      **load-bearing** once the macro is gone — it is the guard against an embedded process
      selecting the fork (re-exec) path.
- - `Create()` fork-executor handle: drop the function-local `static TestModuleExecutorFork`
      (cosmetic; it forces construction even when only sequential is used).

### - Step 3 — verify
- - Build `trun` and `trunlib` (and demos) clean, no `TRUN_HAVE_FORK` anywhere.
- - Internal suite: `./trun -m '!abortall,!exception,-' lib/libtrun_utests.dylib` (expect the
      known intentional-fail count, unchanged).
- - Confirm `trunlib`'s consumer links (Option A: no IPC symbols pulled; Option B: links with
      the added sources).
- - Optional: re-run the Pucko `DateTime` tcov coverage as an end-to-end smoke test.

## Related / deferred (not in this work package)
- `trunembedded.cpp` → `trunlib.cpp` rename + `trunembedded` retirement.
- Dropping `QueryInterface` / `ITestingConfig` / `ITestingCoverage` from `ITesting`.
- General `tcov` revival / experimental-code sweep.
