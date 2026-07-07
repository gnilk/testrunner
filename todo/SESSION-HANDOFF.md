# Session handoff — 2026-07-06

Pick-up notes for continuing on a clean slate.

---

## ⭐ CURRENT WORK — none active (TRUN_HAVE_FORK removal + trunlib rename batch just LANDED on `dev`)

> This session landed a **refactor batch on `dev`** — all merged, branches deleted, **CI green on
> Linux + Windows** (run `28822807846`). **No work is in progress** — pick from "Open work" below.
> `dev` HEAD is **`892203c`**, in sync with `origin/dev`; suite **fork == seq == 116 / 13** (unchanged).
> The reporting/robustness batch (`b2d5ad9`) + testrunner-core audit (`0a7989d`) are background now.
>
> Landed this session, in order:
> 1. **Removed `TRUN_HAVE_FORK`** (merge `0f4499c`, Option B). The fork/sequential split is now
>    **runtime** (`Config::moduleExecuteType`), not compile-time. `trunlib` links the fork/IPC/
>    procspawn transport **inert** and `trun::Initialize` **pins `kSequential`** so the embedded
>    engine never forks. Also deleted the dead code-driven coverage RPC (`int_tcov_begincov` body,
>    tcov IPC server, `coveragerpcbrige.*`, `CoverageIPCMessages.*`, `--tcov-ipc-name`);
>    `ITestingCoverage`/`QueryInterface` kept, `BeginCoverage` now a **no-op** (see open-bugs).
>    Doc archived to `todo/done/remove_trun_have_fork.md`.
> 2. **Renamed `trunembedded.{h,cpp}` → `trunlib.{h,cpp}`** (merge `6ae2232`) — matches `trun::lib`.
>    `trunembedded.h` stays a deprecated `#pragma message` shim → `trunlib.h`, removed in **v5.0.0**;
>    both installed. Partial progress on the deferred rename tail — the `trunlib` *target* rename +
>    the `trunembedded` *demo app* are still deferred (`embedded_delivery_followups.md`).
> 3. **Rewrote PlatformIO `library.json` for V4** (merge `1aa0c88`) — now points at the MCU engine
>    (`src/testrunner/mcu`); dropped the removed `TRUN_EMBEDDED_MCU`/`TRUN_SINGLE_THREAD` macros +
>    `c++17`. **NOT `pio`-verified** (no toolchain here) — smoke-test on the embedded box before
>    release; the one risk is consumer exposure of the `ext_testinterface` `-I` (see commit `b0b3ba1`).
> 4. **Linux + Windows CI build fixes** (`b00902c`, `892203c`). Adding the shared fork/IPC code to
>    `trunlib`'s compile exposed two latent gaps: (a) `trunlib` resolves `logger.h` to the *stripped*
>    embedded logger, which doesn't pull transitive `<string>/<vector>/<list>` the way gnklog's does →
>    libstdc++ failed; fixed with explicit IWYU includes. (b) `trunlib` didn't link
>    `trun_common_options`, so no `NOMINMAX` → MSVC `std::max` broke on the win32 fork transport's
>    `<Windows.h>`; fixed by linking it (`BUILD_INTERFACE`-guarded, export stays clean).
>    **Lesson: macOS/libc++ hides missing includes + platform-define gaps; only the Linux/Windows CI
>    catches them — always watch CI (`gh run watch`) after changing which files a target compiles.**

### Resume from any machine (clean slate on `dev`)
```bash
git fetch origin
git checkout dev && git pull --ff-only                                    # dev @ 892203c
cd cmake-build-debug && ninja trun trun_utests
./trun            -m '!abortall,!exception,-' lib/libtrun_utests.dylib     # fork (default)
./trun --sequential -m '!abortall,!exception,-' lib/libtrun_utests.dylib   # seq == fork
# Expected: fork == sequential == 116 executed / 13 failed (13 = intentional self-fails).
```
A `dev → master` release promotion is still separate and outstanding (needs the version story +
cross-project validation — never unprompted). `run_test_suite.sh` wraps the canonical exclude-list
invocation; always exclude `abortall`/`exception` (they abort/crash the process by design — CLAUDE.md).

### NEW — CI now builds `dev` on Linux + Windows (commit `0e46dee`, verified live)
`.github/workflows/cmake.yml` now triggers on `dev` pushes and PRs into `master`/`dev` (was `master` +
`v*` tags only). Both platform jobs (Linux + Windows: configure → build → smoke-test the loader on a
real `.so`/`.dll`) run every `dev` push/PR; the `.deb`/NSIS **package + upload** steps are gated to
`master`-push and `v*` tags only, so **`dev`/PR builds publish nothing** (no artifacts, no release —
the release job stays tag-only). Side effect: `master` PRs also stop packaging (build+smoke only) —
those artifacts were never consumed. Proven on run `28784026516` (merged `dev`): Linux ✅, Windows ✅,
release skipped, 0 artifacts. **Docs-only pushes/PRs are skipped** (`paths-ignore` for `**.md`,
`todo/**`, `LICENSE`, `notes.txt`; `eacb4fb`) — a mixed docs+code push still builds, and tag pushes
are never skipped (a new tag has no diff base). **Verified:** the docs-only push `f870487` created
**0** CI runs (absent from `gh run list`, `runs?head_sha=…` → `total_count 0`), while the earlier
docs-only push `7fdc7aa` (before the filter) and the non-docs `eacb4fb` each created a run.

### What LANDED in the reporting/robustness merge `b2d5ad9` (9 commits)
All 7 planned items from `todo/open-bugs.md` resolved — each a focused commit with a regression test
**proven to fail on the pre-fix code** where unit-testable. Per-item `✅ RESOLVED` detail is in
`open-bugs.md`.
1. **JSON escaping** (`fdde5e1`) — Symbol/File/Library/Module/Case now escaped; `EscapeString`
   rewritten (UTF-8 survives, control chars → `\uXXXX`). Test `jsonreport_escapestring`.
2. **compose buffer** (`f7b3509`) — reporting `Write*` size exactly via `ComposeString()` (was a shared
   `static char[256]` truncating long lines mid-JSON, non-reentrant). Test `report_longline`.
3. **`CREATE_REPORT_STRING`** (`ac7c7de`) — measure-once sizing (no >1024-byte truncation) +
   `IsMsgSizeOk` `%d`-no-arg vararg UB fixed. Verified e2e (static trampoline, no inline test).
4. **empty `-m`/`-t` filter + `PopIndent`** (`76b780d` + `aca1ddb`) — parse layer keeps the `-`
   match-all default + warns (was: silently ran nothing); `PopIndent` got its missing `return` (was
   `pop_back` on an emptied string → ASan SEGV). Tests `config_emptyfilter`, `report_popindent`.
5. **`ConsumePipes`** (`7b2052d`) — `ssize_t`, guarded `>0`, forwards only bytes read (no OOB on
   `read()==-1`, no 1024-byte padding). Test `module_procoutput`.
6. **global main/exit null-deref** (`4212a4e`) — `!= nullptr` guard (defensive, no test).
7. **[dead/cosmetic]** (`142c916`) — dropped dead `IsModuleMain`, redundant `GetOrAddModule`,
   `fsync`-on-a-FIFO no-op.

**Still open** (tracked in `open-bugs.md`): `ExecuteDependencies` discards its `Execute()` result (a
dependency-accounting *semantics* decision, not mechanical); the Windows `TerminateThread` unwinding
gap (non-default build). **CLAUDE.md self-suite count** is now **`116 executed / 13 fail`** on `dev`
(was 110/13, was 102/15).

### Earlier — testrunner-core audit (merge `0a7989d`, background)
Resolved 10 core items before the reporting batch: the Fatal/Abort result decision
(`TestResult::DeriveResult` + `SetForciblyTerminated`), the unified filter matcher (`caseMatch`,
first-match-wins), IPC hardening (32-bit string length, decoder read-checks, partial-write loop, the
`close(0)`-stdin bug), fork `AllFail` propagation, and `-t`-under-fork. Tests in modules
`resultdecision` / `execorder` / the IPC suite; the design doc `todo/result_model.md` is closed. Full
detail in `todo/open-bugs.md` (section "testrunner core — audit 2026-07-05") and git.

### Evergreen gotchas / cross-cutting notes
- **IPC wire format changed** (32-bit string length, in `0a7989d`) — safe because every IPC endpoint
  is the **same binary**, but an old `trun` can't talk to a new forked child (not a concern within one
  build; matters only if you ever mix binaries).
- JSON reporting tests live in module `jsonreport`; reporting-base tests in module `report`; the
  `nm`/pipe path is covered by `module_procoutput` (Unix-only).
- Leave untracked `.DS_Store` / `src/testrunner/.DS_Store` alone.

---

## Repo state
- **Latest (this session) — CI: Windows release publishing.** `.github/workflows/cmake.yml` now
  builds and publishes **both** platforms. New `build-windows` job (windows-latest): installs NSIS
  via choco, builds Release (tcov auto-off on WIN32, no liblldb), smoke-tests `trun.exe -lx` on a
  real DLL, and `cpack -G NSIS` produces `testrunner-<ver>-win64.exe`. Linux job modernized:
  `actions/checkout` **v2→v5**, and the **vestigial `ctest` step** (the project registers **no**
  CTest tests) replaced with a `trun -lx` smoke test mirroring Windows. Publishing was refactored
  to a **single `release` job** (`needs: [build, build-windows]`, tag-gated, `contents: write`):
  each build job uploads its package as a workflow artifact (`deb-package` / `windows-installer`,
  on **every** build — grabbable from any run page for beta testers), and the release job downloads
  both and does **one** `action-gh-release` upload — removing the create-release race two parallel
  upload steps would hit, and blocking the release if either platform fails.
  **Verified end-to-end** by cutting a throwaway `v0.0.0-ci-test` tag: all 3 jobs green, one Release
  with `testrunner-4.0.0-Linux-runtime.deb` + `-Linux-dev.deb` + `-win64.exe`; then release+tag
  deleted. **Gotcha found:** package version is CMake-driven (`TRUN_VERSION` hardcoded 4.0.0), **not**
  the tag string — a `v4.1.0` tag without bumping `CMakeLists.txt` would still emit `4.0.0` packages.
  Commits `14dcefa` (win job + checkout/ctest), `55feb62` (checkout→v5), `2e44f0f` (single release
  job). **SUPERSEDED (2026-07-06, `0e46dee`):** the workflow now *also* builds `dev` pushes + PRs, and
  the package/upload steps are gated to `master`-push + `v*` tags (so the "on every build" artifact
  note above now means master/tags only; `dev`/PR builds publish nothing) — see the "CI now builds
  `dev`" section at the top. `dev` HEAD is now **`b2d5ad9`** (in sync with `origin/dev`).
- **Windows V4 merge + unix build fix (2026-07-05):** Windows V4 support is **fully merged to `dev`**
  — merge `012994c` brought branch `feature/windows-phase0` (**all four phases**) into `dev`; the
  branch is deleted. A follow-up fix `bb43867` repaired the **unix build** after Phase 3's
  `Process`/`procspawn` split — `dynlib_unix.cpp` had lost its `#include "procspawn.h"` and `tcov`
  wasn't linking `procspawn.cpp`; both were Windows-invisible breaks. macOS build green
  (176/176 ninja targets), self-suite 102 executed / 15 expected self-fails. The plan doc is
  archived to `todo/done/windows_support.md`.
- **Windows V4 implementation (2026-07-05, branch `feature/windows-phase0`, now merged):** the hardware blocker below is resolved — this session ran natively
  on Windows 11 (Visual Studio Community 2026 / MSVC 19.51.36248.0, CMake 4.3.1 + Ninja bundled
  with VS). On branch `feature/windows-phase0` (off `dev`, not yet merged): completed **Phase 0**
  (CMake unblock) of the Windows V4 plan — all bugs in `todo/done/windows_support.md` §5 fixed, plus
  several more found only by actually compiling under MSVC (fork-only `subprocess.cpp` unconditionally
  pulling in UNIX-only `process.h`; fmt needing `/utf-8`; `<Windows.h>` `min`/`max` macro collisions
  needing project-wide `NOMINMAX`; a pre-existing missing include dir on `trun_utests`; a
  `std::erase_if` shim guarded on raw `__cplusplus` instead of `_MSVC_LANG`; POSIX-only
  `SIGUSR1`/`PATH_MAX`/`getcwd` in `trun.cpp`). Full details + file list in
  `todo/done/windows_support.md` Phase 0 section. Verified: clean `cmake-build-win` from scratch builds
  all 162 ninja targets with zero errors; `trun.exe -h`/`-lx` work correctly on a real DLL. Went on
  to finish **Phase 1** (MSVC V2 version-symbol detection — `__declspec(selectany)` +
  `/export:...,DATA` linker pragma, since MSVC rejects `selectany` combined with `dllexport`
  directly; `trv2_utest.dll`/`trun_utests.dll` now correctly report 2.0.0) and **Phase 2**
  (sequential self-test green — root cause of the earlier crash/garbled-output was two separate
  issues: V1 fallback forcing `TerminateThread` instead of cooperative asserts, and the project's
  pre-existing static-CRT (`/MTd`) setting giving `trun.exe`/loaded test DLLs independently-buffered
  stdout streams; switched to the DLL CRT. Sequential run now completes cleanly, exit 0, 15/15
  documented self-fails match exactly). The **core milestone (Phase 0–2) was DONE** here; **Phase 3
  (fork/IPC parity) and Phase 4 (NSIS packaging) followed and are also DONE** — all four phases are
  now merged to `dev` (see "Latest" above). Phase 3 also fixed two genuine runtime bugs surfaced
  only once fork mode ran on Windows: `Process::AddArgument`'s variadic overload never resized its
  scratch buffer after `vsnprintf` (corrupting every subprocess command line on Windows), and a
  `PeekNamedPipe`/`ReadFile` visibility race in the post-exit stdout drain. Per-phase details in
  `todo/done/windows_support.md`.
- **Prior session (2026-07-04):** committed the analyzed, phased **Windows V4 plan**
  (`todo/done/windows_support.md`, commit **`1351b88`**, pushed to `origin/dev`). Decision settled:
  **Windows (first-class V2, not a new interface version) is a committed 4.0 release-story
  deliverable** — the MCU/Desktop-library split *plus* Windows are jointly what justify the major
  bump over the prior bug-fix-only 3.x line. It is **not** demand-triggered/low-priority; it is
  gated only on hardware — the maintainer has an older laptop (at work) to update, or a
  `windows-latest` GitHub Actions MSVC runner is the cheaper path for the Phase 0–2 verification
  loop. Core milestone = Phase 0 (CMake unblock) → 1 (MSVC V2 detection) → 2 (sequential self-test
  green); Phase 3 fork/IPC parity + Phase 4 packaging deferred; `tcov` out of scope. See memory
  `four-oh-release-story`.
- **Prior session (2026-07-03):** reviewed `signal_handling.md` after the executor refactor and
  **deprecated** it — fork already isolates crashes on both platforms and the `--sequential`
  gap is covered by the debugger, so it's valid-but-low-ROI. Moved to
  `todo/deprecated/signal_handling.md` with a `[!] DEPRECATED` header + revival note; updated the
  inbound reference below. Committed `405e229`, pushed to `origin/dev`.
- Engine-rewrite **steps 1, 2, and step-3 Phase A** are all merged to `dev` (step-3 via `--no-ff`
  merge **`197f090`**, feature commit `ffd6814`; branch deleted). All three roadmap engines are
  in `dev`.
- **Library consumption + trunembedded split — DONE and merged** (2026-07-02). Landed via
  `--no-ff` merge **`9f494fe`** (branch `feature/library-consumption`, 4 commits
  `628c24d`..`cd65820`, pushed to `origin/dev`). Delivered: `trun::mcu` FetchContent source
  target (`SOURCE_SUBDIR src/app/trunmcu`, `TRUN_MCU_*` capacity options, V1 via the universal
  `TRUN_USE_V1`); `trun::lib` installed dev package (`find_package(testrunner)` + config-file
  package); `TRUN_BUNDLE_DEPS` (default OFF, no `/usr/local` pollution); two-component CPACK
  (`testrunner` / `testrunner-dev`); README "Building" rewritten. Verified on macOS end-to-end
  (both consumption paths build/link/run; desktop suite 102/15). Design doc archived to
  `todo/done/library_consumption.md`.
  - `feature/library-consumption` has since been **deleted** (local + `origin`), per the
    merged-branch convention.
- `dev` HEAD is **`2e44f0f`** (Windows Phases 0–4 merged via `012994c`, then the unix-build fix
  `bb43867`, then the CI/release-publishing work — see the CI bullet at the top of Repo state),
  in sync with `origin/dev`. Earlier,
  since the 07-02 handoff `dev` also gained: version **bumped to 4.0.0** (`a6a9a82`; version string
  `4.0.0-dev` off-tag, `4.0.0` from a release tag), CWD debug-print removed (`606522e`), README
  updated (`18cf569`), SESSION-HANDOFF refreshed + `signal_handling` deprecated (`405e229`).
- `dev` is far ahead of `master`; a `dev → master` release promotion is still outstanding. The
  version-bump prerequisite is now **done on `dev` (4.0.0)**; cross-project validation is still
  required and the maintainer never promotes unprompted (release-flow rule). Once promoted, the
  first `v4.0.0` tag will auto-publish a Release with the two `.deb`s + the `win64.exe` (CI proven,
  see the CI bullet) — tag it **`v4.0.0`** to match the CMake-driven package version.
- Remaining open work — the **deferred delivery tail** (rewrite the `trunembedded` demo app as
  `trunlib_example`; `include/testrunner/` header layout) — is its own active doc:
  `todo/embedded_delivery_followups.md`. NOT greenlit — capture only.
  **Note:** the **trunlib rename is now DONE** — header/engine-source `trunembedded.{h,cpp}` →
  `trunlib.{h,cpp}` (+ deprecated shim) AND the *target* rename is satisfied by the `trun::lib`
  alias + `EXPORT_NAME lib` (internal `trunlib` name kept by choice). Only the demo-app →
  `trunlib_example` rewrite + the header layout remain. The **Linux `.deb` build validation is
  DONE** — the generator ran in CI and the
  maintainer installed the produced `.deb` on Linux (works fine); it's dropped from the list above.
- Working tree clean except untracked `.DS_Store`, `src/testrunner/.DS_Store` (leave alone).
  The old uncommitted `trun.cpp` CLion debug comment is gone (the CWD debug print was removed in
  `606522e`).
- Build dir: `cmake-build-debug` (ninja).
- **Sandbox build caveat (this environment only):** `cmake-build-debug/_deps/fmt-src` was
  never fully fetched here and there is no network, so the **desktop** targets
  (`trun`/`trun_utests`/`trunlib`) can't build/link in this sandbox (fmt/gnklog). The MCU
  engine is self-contained (no fmt/cpptrace) and builds fine. To build the MCU targets
  through CMake here I configured with `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`; **drop that
  flag when reconfiguring with network**: `cmake -U FETCHCONTENT_FULLY_DISCONNECTED ..`.
  (Historic gotcha still applies elsewhere: a stale fmt v10 vs the v12 pin can make gnklog
  link-fail with undefined `fmt::v10::vprint/vformat`; fix is
  `rm -rf _deps/gnklog-build/CMakeFiles/gnklog.dir && ninja gnklog`.)

## Prior session — engine rewrite step 3 Phase A (MCU engine)  [MERGED 197f090]
Merged to `dev` via `197f090` (feature commit `ffd6814`; branch deleted). Design + impl notes:
`todo/done/embedded_mcu_step3.md` (roadmap: `todo/done/embedded_impl.md` engine #3). A brand-new,
**self-contained zero-alloc engine** under `src/testrunner/mcu/` (7 files) + demo/CMake in
`src/app/trunmcu/`, selected by CMake wiring — NOT `#ifdef`s in the desktop core. No heap,
no threads, no exceptions, no RTTI, no fmt/cpptrace/STL-containers/`std::string`.
- **Files:** `mcu_static.h` (StaticVector/StrView), `mcu_config.h` (constants),
  `mcu_report.{h,cpp}` (console + output sink), `mcu_testing.{h,cpp}` (ITesting vtable +
  setjmp/longjmp), `mcu_runner.{h,cpp}` (registry + run loop + `kStaticFootprintBytes`),
  `trunmcu.{h,cpp}` (public facade). Demo: `src/app/trunmcu/trunmcu_demo.cpp` + CMake.
- **Design (settled with maintainer):** fixed-count capacities
  (`TRUN_MCU_MAX_TESTFUNCS`=64/`_MAX_MODULES`=16/`_MSG_BUF_LEN`=128/`_SINK_MAX_RETRY`=8)
  backing in-house containers; **names stored by pointer** into caller-owned literals (no
  copy, no `MAX_NAME_LEN`, no arena) — footprint is `trun::mcu::kStaticFootprintBytes`
  (**4136 B** with defaults), static_assert-able against a RAM budget. Mid-body abort via
  **setjmp/longjmp** (V1 bare-void assert + Fatal/Abort force-stop; V2 assert/Error
  cooperate). Console output drains through an overridable `SetOutputSink` returning
  `OutputSinkResult{kOk,kRetry,kErrorContinue,kErrorAbort}` (default stdout; bounded
  retries). Dropped: deps (CaseDepends/ModuleDepends no-op), JSON/file reporting, heap
  var-args logging, `Config::FromArguments`. **Kept:** pre/post-case hooks. `RunTests`
  now returns `RunResult` (was `void` on trunembedded).
- **V1 vs V2 is compile-time** (`TRUN_USE_V1`), like trv1/trv2 — the ITesting struct layout
  differs so the engine is recompiled per version; trampolines branch on a small, local
  `#ifdef` (justified by the frozen header's own versioning). `trunmcu` static lib = V2
  (**host-validation only — NOT installed**; the engine is compiled for the target by the
  embedder). `trunmcu_demo` = V2; `trunmcu_demo_v1` recompiles the engine with `TRUN_USE_V1`.
- **Also removed** the two now-dead `TRUN_EMBEDDED_MCU` `#ifdef` stubs
  (`responseproxy.cpp`, `reporting/reportingbase.cpp`) — impl-swap, not `#ifdef`. Both
  edited files compile clean (their `.o`s built before the unrelated sandbox fmt failure).
- **Verified (host):** V1 + V2 demos build and run **identically** — a failing assert stops
  its case mid-body (V2 cooperative return / V1 longjmp), `Fatal` stops the module's
  remaining cases but module exit + sibling cases still run. Built `-fno-exceptions
  -fno-rtti -Wall -Wextra` with zero warnings; `nm` shows no `operator new`/`malloc` in the
  engine objects. Footprint 4136 B.

## How to verify
```bash
# MCU engine (self-contained; builds even without the desktop deps):
cd cmake-build-debug
cmake -DFETCHCONTENT_FULLY_DISCONNECTED=ON ..     # only needed if desktop deps missing
ninja trunmcu trunmcu_demo trunmcu_demo_v1
./trunmcu_demo        # V2 - expect: 9 executed / 2 failed, footprint 4136 bytes
./trunmcu_demo_v1     # V1 - identical output (validates the forced-longjmp assert path)

# Desktop canonical suite (needs fmt/cpptrace fetched - NOT runnable in the dev sandbox):
ninja trun trun_utests
./trun -m '!abortall,!exception,-' lib/libtrun_utests.dylib          # fork (default)
./trun --sequential -m '!abortall,!exception,-' lib/libtrun_utests.dylib
# Expected: fork == sequential == 102 executed / 15 failed (15 are intentional self-fails).
```
CI (`.github/workflows/cmake.yml`) only runs on `master` pushes + `v*` tags, so `dev` pushes do NOT
trigger it. To exercise the release pipeline before a real tag, push a throwaway tag (fires on any
branch) and inspect/clean up:
```bash
git tag v0.0.0-ci-test && git push origin v0.0.0-ci-test
gh run watch <id> --exit-status                       # 3 jobs: build, build-windows, release
gh release view v0.0.0-ci-test --json assets --jq '.assets[].name'   # 2 .deb + 1 win64.exe
gh release delete v0.0.0-ci-test --cleanup-tag --yes  # tears down release + remote tag
```

## Open work — suggested order
0. ~~**Windows V4 support**~~ — ✅ **DONE & merged** (all four phases, merge `012994c`; doc archived
   to `todo/done/windows_support.md`). `trun.exe` builds under MSVC, detects V2 DLLs, and runs its
   self-test suite clean in **both** sequential and fork mode (`process_win32` + `IPCPipeWin`,
   `TRUN_HAVE_FORK` live for Windows). Explicitly **out of scope** (recorded in the archived doc):
   `tcov` on Windows (needs an SEH / `AddVectoredExceptionHandler` / DbgHelp debug backend) and
   retiring the legacy `trunwindows/` VS solution.
1. **Deferred delivery tail** — `todo/embedded_delivery_followups.md` (extracted when
   `library_consumption.md` was archived). ~~(a) trunlib rename~~ — **DONE**: header/engine-source
   `trunembedded.{h,cpp}` → `trunlib.{h,cpp}` (+ deprecated shim) AND the *target* rename via the
   `trun::lib` alias + `EXPORT_NAME lib` (internal `trunlib` name kept by choice). **Remaining:**
   rewrite the `trunembedded` *demo app* as `trunlib_example` (deferred, not scheduled — doc input);
   (b) `include/testrunner/` header layout. ~~(c) Linux `.deb` build validation~~ — **DONE**
   (2026-07-07): the generator ran in CI and the maintainer installed the produced `.deb` on Linux
   (works fine).
2. **Post-merge verification (step-3)** — the merge is done (`197f090`); the desktop 102/15 suite
   was re-run green during the consumption work, so this is effectively covered. Small follow-ups
   still noted in `todo/done/embedded_mcu_step3.md`: glob/negation (`!mod`) in the filter matcher;
   whether `RunTests` returning `RunResult` (vs the old `void`) should also flow into the
   trunembedded facade.
3. **Step-3 Phase B** — cross toolchain + real board (e.g. `arm-none-eabi-gcc`,
   `-ffreestanding`, no-libc considerations: the host phase leans on `<cstdio>`/`vsnprintf`;
   freestanding must swap those for the sink + a tiny formatter). NOT greenlit — its own plan.
4. **Coverage/tcov sweep** — deferred; experimental, dead code there is intentional (memory
   `coverage-tcov-experimental`). Includes the `SymbolResolver::IsInProject` no-op.

(`signal_handling` is no longer open work — **deprecated** 2026-07-03, moved to
`todo/deprecated/signal_handling.md`; see Repo state above.)

## Key decisions / gotchas to remember
- **External interface headers are frozen** (`ext_testinterface/testinterface.h` V2 +
  `testinterface_v1.h` V1). Don't edit them. V1's threaded assert has no `return`, so a
  failing V1 assert can only stop mid-body via forced termination — thread-kill on desktop,
  **`longjmp` on MCU**. (CLAUDE.md + memory `external-interface-frozen`.)
- **Module dependencies must be declared in `test_main`**, never in module main — declaring
  in module main forces a mid-execution rollback/abort (deliberately rejected). Deps are
  discouraged-but-supported on desktop; **dropped entirely on MCU**.
- Forked mode is for CI/CD speed (large suites), usually `-r json` consumed by a web-app;
  `--sequential` is for debugging through tests (CLion owns execution).
- macOS `trun` is built with ASan; macOS has no LSan, so leaks aren't caught automatically —
  lifetime fixes are verified by exercising the objects under the ASan-clean run.
- Branch vs direct-commit rule: multi-file/function work gets its own branch; small in-place
  fixes go straight to `dev` (memory `branch-vs-direct-commit`).

## Conventions captured (memory + CLAUDE.md)
- Resolved todo docs get an inline `✅ RESOLVED (branch)` tag and move to `todo/done/` once
  fully closed. TODO markers: `-` open, `+` in progress, `!` done.
- **Decided-against** docs get a `[!] DEPRECATED <date>` blockquote header (rationale + revival
  note) and move to `todo/deprecated/` — new bucket added 2026-07-03 for `signal_handling.md`,
  distinct from `todo/done/` (= completed/archived).
- Simplify: prefer per-target implementation files over `#ifdef`-laden shared files (memory
  `prefer-impl-files-over-ifdefs`); the MCU engine is the clearest example so far.
- Top-down code ordering; project CMake platform defines (`APPLE`/`LINUX`), not compiler
  builtins.
- TDD where unit-testable; lifetime/threading/UB fixes verified via suite + ASan instead.
