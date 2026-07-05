# Session handoff — 2026-07-05

Pick-up notes for continuing on a clean slate.

## Repo state
- **This session (2026-07-05):** the hardware blocker below is resolved — this session ran natively
  on Windows 11 (Visual Studio Community 2026 / MSVC 19.51.36248.0, CMake 4.3.1 + Ninja bundled
  with VS). On branch `feature/windows-phase0` (off `dev`, not yet merged): completed **Phase 0**
  (CMake unblock) of the Windows V4 plan — all bugs in `todo/windows_support.md` §5 fixed, plus
  several more found only by actually compiling under MSVC (fork-only `subprocess.cpp` unconditionally
  pulling in UNIX-only `process.h`; fmt needing `/utf-8`; `<Windows.h>` `min`/`max` macro collisions
  needing project-wide `NOMINMAX`; a pre-existing missing include dir on `trun_utests`; a
  `std::erase_if` shim guarded on raw `__cplusplus` instead of `_MSVC_LANG`; POSIX-only
  `SIGUSR1`/`PATH_MAX`/`getcwd` in `trun.cpp`). Full details + file list in
  `todo/windows_support.md` Phase 0 section. Verified: clean `cmake-build-win` from scratch builds
  all 162 ninja targets with zero errors; `trun.exe -h`/`-lx` work correctly on a real DLL. Went on
  to finish **Phase 1** (MSVC V2 version-symbol detection — `__declspec(selectany)` +
  `/export:...,DATA` linker pragma, since MSVC rejects `selectany` combined with `dllexport`
  directly; `trv2_utest.dll`/`trun_utests.dll` now correctly report 2.0.0) and **Phase 2**
  (sequential self-test green — root cause of the earlier crash/garbled-output was two separate
  issues: V1 fallback forcing `TerminateThread` instead of cooperative asserts, and the project's
  pre-existing static-CRT (`/MTd`) setting giving `trun.exe`/loaded test DLLs independently-buffered
  stdout streams; switched to the DLL CRT. Sequential run now completes cleanly, exit 0, 15/15
  documented self-fails match exactly). **Core milestone (Phase 0–2) is DONE.** Branch
  `feature/windows-phase0` not merged to `dev` pending maintainer review. Phase 3 (fork/IPC parity)
  and Phase 4 (packaging) remain later/not greenlit.
- **Prior session (2026-07-04):** committed the analyzed, phased **Windows V4 plan**
  (`todo/windows_support.md`, commit **`1351b88`**, pushed to `origin/dev`). Decision settled:
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
- `dev` HEAD is **`1351b88`**, in sync with `origin/dev` (the Windows-plan commit above). Earlier,
  since the 07-02 handoff `dev` also gained: version **bumped to 4.0.0** (`a6a9a82`; version string
  `4.0.0-dev` off-tag, `4.0.0` from a release tag), CWD debug-print removed (`606522e`), README
  updated (`18cf569`), SESSION-HANDOFF refreshed + `signal_handling` deprecated (`405e229`).
- `dev` is far ahead of `master`; a `dev → master` release promotion is still outstanding. The
  version-bump prerequisite is now **done on `dev` (4.0.0)**; cross-project validation is still
  required and the maintainer never promotes unprompted (release-flow rule).
- Remaining open work — the **deferred delivery tail** (trunlib rename + trunembedded facade
  retirement, `include/testrunner/` header layout, Linux `.deb` **build** validation) — is its
  own active doc: `todo/embedded_delivery_followups.md`. NOT greenlit — capture only.
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

## Open work — suggested order
0. **Windows V4 support** — `todo/windows_support.md`. A committed 4.0 release-story deliverable.
   The hardware blocker is resolved and the **core milestone (Phase 0 + 1 + 2) is DONE** on branch
   `feature/windows-phase0` (not yet merged to `dev`) — `trun.exe` builds under MSVC, correctly
   detects V2 test DLLs, and its own sequential self-test suite runs clean (15/15 documented
   self-fails, no crash). Next: merge/review the branch, then (not greenlit yet) **Phase 3**
   (fork/IPC parity — `process_win32.{h,cpp}`, `IPCPipeWin`, `TRUN_HAVE_FORK` for Windows) and
   **Phase 4** (packaging).
1. **Deferred delivery tail** — `todo/embedded_delivery_followups.md` (extracted when
   `library_consumption.md` was archived). Three items, none greenlit: (a) rename `trunlib` +
   retire the old two-in-one `trunembedded` facade (coupled — one atomic push; maintainer chose
   to keep `trunlib` for now); (b) `include/testrunner/` header layout (couple with the rename);
   (c) run the Linux `.deb` **build** (`ninja package`) — the config/export/CPACK split is
   authored + verified on macOS, but the `.deb` generator itself is Linux-only and unrun.
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
