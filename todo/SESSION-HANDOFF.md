# Session handoff — 2026-07-02

Pick-up notes for continuing on a clean slate.

## Repo state
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
  - **NOTE:** `feature/library-consumption` still exists locally + on `origin` — prior convention
    deletes merged branches; offer to delete it.
- `dev` is in sync with `origin/dev` (verify with `git status -sb`). `dev` is far ahead of
  `master`; a `dev → master` release promotion is a separate, still-outstanding step (needs a
  version bump + cross-project validation per the release-flow rule — never promote unprompted).
- Remaining open work — the **deferred delivery tail** (trunlib rename + trunembedded facade
  retirement, `include/testrunner/` header layout, Linux `.deb` **build** validation) — is its
  own active doc: `todo/embedded_delivery_followups.md`. NOT greenlit — capture only.
- Working tree (intentional / not mine, leave alone):
  - `src/app/trun/trun.cpp` — uncommitted CLion working-dir debug comment (left unstaged on
    purpose; NOT part of any commit).
  - `.DS_Store`, `src/testrunner/.DS_Store` — untracked.
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
5. **`todo/deprecated/signal_handling.md`** — DEPRECATED 2026-07-03 (decided against; fork
   already provides crash isolation, and the `--sequential` gap is covered by the debugger).
   Moved out of the active set into `todo/deprecated/`.

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
- Simplify: prefer per-target implementation files over `#ifdef`-laden shared files (memory
  `prefer-impl-files-over-ifdefs`); the MCU engine is the clearest example so far.
- Top-down code ordering; project CMake platform defines (`APPLE`/`LINUX`), not compiler
  builtins.
- TDD where unit-testable; lifetime/threading/UB fixes verified via suite + ASan instead.
