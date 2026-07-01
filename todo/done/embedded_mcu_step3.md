# Step 3 — thin zero-alloc MCU engine (design)  [ARCHIVED — Phase A done + merged]

> **ARCHIVED 2026-07-01.** Phase A is implemented and **merged to `dev`** (merge `197f090`,
> feature commit `ffd6814`; branch `rewrite/embedded-engine-step3` deleted). This is the Phase A
> design + implementation record. The still-open follow-ons that used to live here — **easy
> consumption (FetchContent)** and the **trunembedded split** — were extracted to
> `todo/mcu_consumption.md`. **Phase B** (cross toolchain/board) stays deferred and gets its own
> doc when greenlit (see "Deferred to Phase B" below).

Design settled 2026-06-30, Phase A implemented 2026-06-30. Companion to
`todo/done/embedded_impl.md` (roadmap engine #3). Guiding principle: simplify; prefer separate
per-target files over `#ifdef`s (see `embedded_impl.md` "Guiding principle" + memory
`prefer-impl-files-over-ifdefs`).

## TODO  [ -:open  +:in progress  !:done ]
```
! Phase A: host-validated MCU engine (this doc)  [branch rewrite/embedded-engine-step3]
  ! in-house fixed containers (StaticVector<T,N>, StrView)   src/testrunner/mcu/mcu_static.h
  ! registration table + module grouping (test_<module>_<case> split)   mcu_runner.{h,cpp}
  ! zero-alloc ITesting impl + setjmp/longjmp abort   mcu_testing.{h,cpp}
  ! minimal console reporter via output sink   mcu_report.{h,cpp}
  ! public facade (Initialize/AddTestCase/RunTests/SetVerbose + SetOutputSink)   trunmcu.{h,cpp}
  ! CMake: trunmcu (static, V2) + trunmcu_demo (V2) + trunmcu_demo_v1 (V1), -fno-exceptions -fno-rtti
  ! remove the two TRUN_EMBEDDED_MCU #ifdef stubs (responseproxy.cpp, reportingbase.cpp)
  ! validate: V1+V2 demos build+run identically; assert/Fatal stop mid-body; heap-free
    (nm: no operator new/malloc); footprint 4136 bytes; -fno-exceptions -fno-rtti clean
  - desktop suite 102/15: NOT runnable in the dev sandbox (fmt not fetchable here);
    run it where the FetchContent deps exist. The edited shared files compile clean.
! Merge Phase A to dev  [merge 197f090, branch deleted]
> Easy consumption (FetchContent) + trunembedded split — extracted to todo/mcu_consumption.md
- Phase B: cross toolchain + real board (deferred — gets its own doc when greenlit)
```

### Implementation notes (2026-06-30, branch rewrite/embedded-engine-step3)

- Engine lives in `src/testrunner/mcu/` (7 files) + demo `src/app/trunmcu/`. Self-contained:
  no fmt/cpptrace/gnklog, no STL containers, no std::string, no heap, no threads, no
  exceptions/RTTI. Verified with `nm` (no `operator new`/`malloc` in engine objects) and
  built with `-fno-exceptions -fno-rtti -Wall -Wextra` (clean).
- **V1 vs V2** is compile-time (`TRUN_USE_V1`), like trv1/trv2 — the ITesting struct layout
  differs, so the engine is recompiled per version. The trampolines branch on a small,
  local `#ifdef TRUN_USE_V1` (AssertError arg order + void/int return; ModuleDepends/
  QueryInterface only exist in V2) — justified by the frozen header's own versioning, not
  macro-soup in a shared core. The `trunmcu` static lib is a **host-validation build only**
  (V2): the MCU engine is compiled *for the target* by the embedder with their cross-
  toolchain — it is **not installed** (unlike `trunlib`). `trunmcu_demo_v1` recompiles the
  engine with `TRUN_USE_V1`.
- **Abort observed working both ways**: V2 assert returns kTRLeave → macro `return kTR_Fail`
  (cooperative); V1 assert is a bare void call → `longjmp`. Fatal/Abort `longjmp` for both.
  In the demo, the line after a failing assert / after Fatal never runs, the next case in a
  Fatal'd module is skipped, but module exit (teardown) still runs, and a sibling case after
  an assert still runs — exactly as intended.
- Footprint exposed as `trun::mcu::kStaticFootprintBytes` (4136 bytes with defaults: 64
  testfuncs, 16 modules, 128-byte msg buf). static_assert it against a RAM budget if wanted.
- Dependencies dropped (CaseDepends/ModuleDepends are no-ops); pre/post-case hooks ARE
  supported (stored per module, invoked around each case inside the setjmp guard).
- Filtering (`moduleFilter`/`caseFilter`) is in-place, zero-alloc: `-`/`*`/empty = all, else
  comma-separated exact tokens. Glob/negation (`!mod`) is a possible follow-up.
- Sandbox note: the dev build dir's fmt source was never fully fetched, so I configured the
  MCU targets with `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` to build through the project's
  CMake. Drop that flag (`cmake -U FETCHCONTENT_FULLY_DISCONNECTED ..`) when reconfiguring
  with network so the desktop deps fetch normally.

## Settled decisions

1. **Scope — host-validated, architecture-first.** Build the separate zero-alloc /
   no-thread / no-exception engine so it compiles and runs on macOS/Linux behind the
   existing `trunembedded.h` facade, validated against a sample target + the desktop
   suite. No cross-toolchain / MCU CI yet — that is Phase B. The win of Phase A is
   proving the discipline (no heap, no threads, no exceptions, no RTTI) on a toolchain we
   can actually run the suite on.

2. **Abort model — `setjmp`/`longjmp`.** No threads to kill, no exceptions to throw.
   Per case the engine does `setjmp` before the call; forced-stop callbacks `longjmp`
   back. This is required by the frozen V1 header (`TR_ASSERT` → bare `void AssertError`,
   no `return`), and it also cleanly serves V2 `Fatal`/`Abort`. Forced-stop set:
   - V1 `AssertError`  → `longjmp` (V1 is forced, mirroring desktop `kThreadedWithExit`)
   - `Fatal` (V1+V2)   → `longjmp`
   - `Abort` (V1+V2)   → `longjmp` + signal abort-all
   - V2 `AssertError`  → returns `kTR_Fail`, cooperative (no `longjmp`)
   - `Error` (soft)    → record + continue (no `longjmp`)
   V1-vs-V2 detection reuses the existing `testlibversion` / weak-symbol path if it links
   freestanding; otherwise a compile-time `TRUN_USE_V1`. (Confirm during impl.)

3. **Allocation — fixed-count compile-time constants + name-by-pointer.** No heap, no
   allocator (not even a bump arena — an arena reintroduces a runtime alloc path and an
   order-dependent failure mode, which is the worst trait for a *test* tool). The only
   variable-length data is names, and names are already caller-owned string literals
   (`TRUN_ADD_TEST(tc)` → `AddTestCase(#tc, tc)`; explicit calls pass literals). So the
   engine **stores `const char*` / `{ptr,len}` into the caller's literal and never copies
   a name** — this removes `MAX_NAME_LEN` entirely and is *more* zero-alloc than an arena.
   Contract: a registered name pointer must outlive the run (true for every current
   caller).

   | Constant | Backs | Default |
   |---|---|---|
   | `TRUN_MCU_MAX_TESTFUNCS` | flat `{name_ptr, fn, moduleIdx, state}` table | 64 |
   | `TRUN_MCU_MAX_MODULES`   | derived `{name view, case range, main, exit}` table | 16 |
   | `TRUN_MCU_MSG_BUF_LEN`   | one scratch buffer for assert/log formatting | 128 |

   All storage is fixed-count → footprint is a compile-time constant. Expose it for the
   "single RAM number" ergonomic, with a deterministic, link/compile-time failure:
   ```cpp
   namespace trun::mcu { constexpr size_t kStaticFootprintBytes = /* tables + buf + jmp_buf */; }
   // optional, if the embedder wants the single-knob feel:
   static_assert(trun::mcu::kStaticFootprintBytes <= TRUN_MCU_MEM_BUDGET, "trun MCU footprint exceeds budget");
   ```
   Overflow fails locally and reproducibly ("symbol X exceeds MAX_TESTFUNCS"), never as an
   order-dependent runtime surprise.

4. **Feature cut — drop all four:**
   - **Dependencies** — no dep graph. `CaseDepends`/`ModuleDepends` become no-ops
     (optionally log "unsupported"). Flat module → case iteration only.
   - **JSON / file reporting** — no `reportjson*`, no `FILE*`/`fopen`. Console only.
   - **Var-args logging (heap path)** — drop the `alloca`/`std::string` var-args path.
     Keep a *bounded* `vsnprintf` into the fixed `TRUN_MCU_MSG_BUF_LEN` scratch buffer for
     assert/error messages (no heap, no `alloca`). This replaces the existing
     `TRUN_EMBEDDED_MCU` stub that neutralized messages to empty.
   - **`Config::FromArguments` / `ArgParser`** — no `argv`. Configure via the
     `RunTests(moduleFilter, caseFilter)` API + compile-time defaults only.

5. **Output — default stdout, overridable sink that reports back.** Reporting drains
   through a callback so we don't hard-wire stdout (target points it at UART/serial), and
   the sink returns a small result enum so the transport can tell the engine whether to
   continue:
   ```cpp
   enum class OutputSinkResult {
       kOk,            // wrote the whole chunk; continue
       kRetry,         // wrote nothing (all-or-nothing); engine re-issues the same chunk
       kErrorContinue, // write failed; drop this output but keep running tests
       kErrorAbort,    // fatal transport error; stop the run
   };
   typedef OutputSinkResult (*OutputSinkFunc)(const char *data, size_t length);
   void trun::SetOutputSink(OutputSinkFunc fn);   // nullptr restores the default stdout sink
   ```
   Contract / handling:
   - **`kRetry` is all-or-nothing** — the sink consumed the whole `{data,len}` or none of
     it; `kRetry` = "took nothing, ask again". No byte counts / partial-write offsets.
   - **Retries are bounded** by `TRUN_MCU_SINK_MAX_RETRY` (default 8); on exhaustion the
     engine downgrades to `kErrorContinue` (an unbounded `kRetry` spin would hang an MCU —
     the worst failure for a test tool).
   - `kOk` → next · `kRetry` → re-issue (capped) · `kErrorContinue` → drop chunk, keep
     testing · `kErrorAbort` → stop emitting + abort the run.
   - **`RunTests` returns a status** (not `void`) so the embedder learns the run was cut
     short by `kErrorAbort`.
   Default sink: `fwrite(data, 1, length, stdout)` on host → `kOk` (or `kErrorContinue` on
   short write). One call per line (maps to `uart_write(buf, len)`). Names proposed
   `SetOutputSink` / `OutputSinkResult` / `OutputSinkFunc` (alts: `SinkResult`,
   `SetOutputWriter`, `SetWriteCallback`).

## Architecture & file layout

New self-contained engine under `src/testrunner/mcu/`. It **does not compile**
`testrunner.cpp`, `testmodule.cpp`, `config.cpp`, `responseproxy.cpp`, `funcexecutors.cpp`,
`resultsummary.cpp`, or `reporting/*` — those are the desktop core. It reuses **only**:
- the frozen `ext_testinterface/testinterface.h` (V2) + `testinterface_v1.h` (V1) contract
  and the `kTR_*` result codes (do not edit — frozen, see memory `external-interface-frozen`);
- `shared/glob.h`'s **`Glob::Match(const char*, const char*)`** C-string overload for
  filter matching (non-allocating — confirm the `.cpp` path stays heap-free for the C-string
  overload; if not, inline a tiny matcher).

Proposed files:
| File | Replaces / role |
|---|---|
| `mcu/static_containers.h` | `StaticVector<T,N>`, `StrView{const char*,size_t}` (header-only, no heap) |
| `mcu/mcu_config.h` | the `TRUN_MCU_*` constants + defaults + `kStaticFootprintBytes` |
| `mcu/mcu_registry.{h,cpp}` | registration table + `test_<module>_<case>` grouping (replaces `DynLibEmbedded`) |
| `mcu/mcu_testing.{h,cpp}` | zero-alloc `ITesting` impl + `setjmp`/`longjmp` abort (replaces `TestResponseProxy`) |
| `mcu/mcu_report.{h,cpp}` | minimal console reporter + output sink (replaces `reporting/*`) |
| `mcu/mcu_runner.{h,cpp}` | the small orchestration: main → module → case(setjmp) → exit |
| `mcu/trunmcu.{h,cpp}` | public facade `Initialize/AddTestCase/RunTests/SetVerbose/SetOutputSink` |

The public facade keeps the **same names/signatures** as today's `trunembedded.h`
(`Initialize`, `AddTestCase`, `RunTests`, `SetVerbose`) so existing embedders are
source-compatible; `SetOutputSink` is additive. Whether `trunmcu.h` *is* the installed
`trunembedded.h` (one facade, swapped impl) or a sibling header is a wiring call to make at
impl time — prefer one facade if it stays clean.

## Execution sequence (the orchestration core)

```
test_main (global)                         # if present
  for each module (in registration order):
    test_<module>          (module main)   # if present
    for each case in module:
      setjmp(caseEnv)                       # forced-stop lands here
        run case → record pass/fail/abort
    test_<module>_exit     (module exit)    # if present
test_exit (global)                         # if present
```
Symbol classification by splitting on `_`: `test_main`, `test_exit`, `test_<module>`
(module main), `test_<module>_exit` (module exit), else `test_<module>_<case>`. Filtering:
match `moduleFilter`/`caseFilter` in place against the registration table (no vector
building); `"-"` = all; comma-separated names; `Glob::Match` for patterns.

## CMake / target wiring

- New `trunmcu` STATIC lib (mcu/ sources + frozen headers only) and `trunmcu_demo` host exe
  (parallels `trunembedded`, but on the MCU engine and with V1+V2 sample cases).
- **Does not** link `fmt` or `cpptrace`; **does not** compile the desktop core.
- Build Phase A with **`-fno-exceptions -fno-rtti`** (and ideally `-fno-threadsafe-statics`)
  to *prove* the no-exception/no-RTTI discipline on the host toolchain.
- `TRUN_EMBEDDED_MCU` may still be defined for the target, but its job is target/source
  *selection*, not `#ifdef`-ing the shared core. **Remove** the two existing
  `TRUN_EMBEDDED_MCU` `#ifdef` stubs (`responseproxy.cpp:393`, `reportingbase.cpp:16`) —
  the MCU engine simply doesn't compile those files (impl-swap, not `#ifdef`; honors the
  guiding principle).

## Validation (Phase A)

- `trunmcu_demo`: a sample lib with V1 and V2 cases exercising pass, soft `Error`,
  `AssertError` (V1 forced-stop vs V2 cooperative), `Fatal`, `Abort` → output matches
  expected pass/fail/stop-points.
- Print / `static_assert` `kStaticFootprintBytes`.
- Build cleanly under `-fno-exceptions -fno-rtti` (no hidden exception/RTTI/heap pull-in;
  spot-check the link map for `malloc`/`operator new`).
- Desktop suite unchanged: fork == sequential == **102 executed / 15 failed**, exit 0.
- Per memory `coverage-tcov-experimental` / `branch-vs-direct-commit`: this is a
  feature-sized branch (`rewrite/embedded-engine-step3`), built alongside the existing
  embedded engine, which stays the baseline until the new one passes.

## Follow-on work (extracted — no longer tracked here)

The two open follow-ons that used to live in this doc — **easy consumption (FetchContent)**
and the **trunembedded split** (trunmcu = embedded vs trunlib = desktop-embed) — moved to
`todo/mcu_consumption.md` when this doc was archived. They are not Phase A work; they are how
the delivered engines get shipped as first-class, easily-consumable libraries.

## Deferred to Phase B

- Cross toolchain file (e.g. `arm-none-eabi-gcc`), `-ffreestanding`, concrete board/RTOS
  startup, no-libc considerations (the host phase may lean on `<cstdio>`/`vsnprintf`; the
  freestanding phase must swap those for the sink + a tiny formatter).
- Decide whether `StrView` / `std::string_view` and `<cstring>` usages are freestanding-safe
  or need in-house equivalents.
