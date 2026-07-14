## tcov architectural cleanup — experimental → beta

> **✅ RESOLVED (branch `tcov_beta`, merged to `dev` 2026-07-14, merge `28a9e4f`).** All 6 phases
> (0–5) and all 5 beta gates done; CI green on Linux + Windows (the merge added a Linux `tcov_utests`
> gate). Filed to `todo/done/`. **One residual, deliberately out of scope:** the `IsInProject` no-op
> filter (§2 last bullet) — now its own task, `todo/tcov_isinproject_filter.md`. Not shipped yet: the
> *experimental → beta* doc label + `dev → master` still follow the normal release gate (see §8).

> **Status: assessment + roadmap (2026-07-08).** This is a planning document, not a change.
> Each phase below is separately approvable; no code is touched until its phase is approved.
> Markers follow the project convention: `-` open, `+` in progress, `!` done, `[!]` deprecated.
>
> Cross-references: `SymbolResolver::IsInProject` and the removed coverage RPC are already
> noted in `todo/open-bugs.md`; the RPC removal itself is `todo/done/remove_trun_have_fork.md`.
> This document is the consolidated tcov-specific view and supersedes those scattered notes for
> the road-to-beta.

### Background

`tcov` measures line coverage on an **un-instrumented** binary: it runs the target under LLDB,
sets a breakpoint on every source line inside the functions named in `--symbols`, disables each
breakpoint on first hit, and turns hit counts into per-line coverage (`Breakpoint.cpp`
`ComputeCoverage`). No `gcov`/`--coverage` recompile is needed — that is tcov's reason to exist.

It was conceived as a companion to `trun`: it launches `trun --coverage`, waits for `trun` to
`dlopen` all its test libraries (signalled by `SIGUSR1`), and only then resolves symbols and
places breakpoints. It has since been found to work against any executable — but the
trun-specific sync and signal handling are still wired in unconditionally, which is one of the
things this cleanup addresses.

The tool is marked **experimental**. The implementation has accreted redundancy and dead code —
most notably two live symbol-resolution paths (one unfinished, one now reached only as a name
filter) and leftovers from a removed `trun`→`tcov` IPC experiment.

---

## 0. Test scaffolding — TDD prerequisite (`tcov_utests`)

tcov has **zero** unit tests today (built exploratively). Before touching the symbol/breakpoint
layer we stand up a `trun`-style unit-test target and add tests **TDD-first** — red before a
refactor, green after — so "behavior-preserving" becomes executable and intent is documented.

> **Phase 0 DONE (2026-07-09, branch `tcov_beta-phase0`).** `tcovcore` extracted, `tcov_utests`
> stood up (8 cases, all green via `./trun lib/libtcov_utests.dylib`), `tcov` verified behavior-
> preserving (builds + real coverage run `trun::split 22/22` unchanged). D1 + D5 tests landed;
> **D3/D4 deferred to Phase 2** (see the last bullet). Note: git can't create a branch
> `tcov_beta/phase0` while `tcov_beta` exists as a ref, so the phase branch is `tcov_beta-phase0`
> (hyphen); apply the same hyphen convention to later phases.

- `!` **Decided + DONE:** extract the coverage engine (`tcovsrcfiles`) into a **`tcovcore` static
  library** (`src/coverage/CMakeLists.txt`); `tcov` and the new `tcov_utests` shared lib both link it
  (LLDB + fmt/gnklog + `trun_common_options` are PUBLIC usage requirements of `tcovcore`). No
  duplicated LLDB wiring — mirrors the `trunlib` static-lib pattern. `tcovcore` is `POSITION_INDEPENDENT_CODE`
  (static archive linked into the `tcov_utests` shared lib). The shared utilities (`strutil`/`glob`/
  `timer`/…) compile into `tcovcore` once; `tcov` is now a thin front-end (`tcov.cpp` + the still-inert
  IPC/process objects removed in Phase 1).
- `!` **DONE:** New `src/coverage/tests/` with `trun`-convention tests, built as `libtcov_utests.{so,dylib}`
  (gated behind `BUILD_TCOV`, linking `tcovcore`): global `test_main`/`test_exit` skeleton, then one
  `test_<module>.cpp` per unit (each with its `test_<module>` / `test_<module>_exit` main/exit) —
  `symbolresolver`, `breakpoint`. Run via `./trun lib/libtcov_utests.dylib`.
- `!` The first real tests encode §2's decisions. **Phase 0:** `SymbolInfo.endLoadAddress` default (D1,
  `test_symbolresolver`); overloads stay distinct via the full-name map key (D5, `test_breakpoint`).
  **Phase 2 (landed with the recomposition they guard):** `Function` composes `SymbolInfo` with
  `GetDisplayName()==info.name` (D3/D4, `test_breakpoint_func_composesinfo`) and a default-constructed
  `Function` no longer crashes (`test_breakpoint_func_displaynamedefault`) — these could not be a clean
  red-before because the old `GetDisplayName()` dereferenced a default `SBSymbol` (a crash, not an
  assert). The two new address fields
  `SymbolInfo::startLoadAddress`/`endLoadAddress` (renamed from `addr`/`endAddr` in Phase 0 to match
  `Function`, once they proved self-contained — only `ResolveForTarget` writes them) are
  behavior-neutral (`endLoadAddress` is read by nothing until Phase 2). These need only the tcov *types*.
- Boundary: `ResolveForTarget` + breakpoint install need a live `SBTarget`, so they stay covered by
  the integration before/after coverage diff, not `tcov_utests`.

---

## 1. Architectural verdict

**The core design is sound. The implementation is unfinished and carries redundant / dead
code.** This is a reasonable base to harden toward beta, not something to redesign.

**Smart — keep:**

- `!` **Breakpoint-based coverage on an un-instrumented binary** via LLDB. Legitimate,
  well-understood technique; no recompile required.
- `!` **Synchronous LLDB** (`SetAsync(false)`, `Coverage.cpp:111`) — no event loop; the run
  loop just polls `process.GetState()` (`Coverage.cpp:313` `WaitState`, `:362` `Process`).
  Simple and correct for this use.
- `!` **`SIGUSR1` dlopen-sync for trun** (`Coverage.h:51`, `Coverage.cpp:159-196`
  `RunInitialLLDBPhase`, plus `trun.cpp` `--coverage`) — a pragmatic, low-coupling sync point
  that correctly replaced the earlier over-engineered IPC channel
  (`todo/done/remove_trun_have_fork.md`).
- `!` **Report engines behind `ReportBase` + factory map** (`Coverage.cpp:423-435`) — extensible
  (`base` / `lcov` / `diff`).
- `!` **Clean layering** — `src/shared` (common) / `src/coverage` (engine) / `src/app/tcov`
  (thin entry; `main` is ~20 lines, `tcov.cpp:249`).

**Unfinished / not smart — fix on the road to beta:** two live symbol-resolution paths (§2, the
central issue); substring-match trun detection with the author's own `FIXME` (§5); dead IPC
remnants (§3); assorted dead / inert code (§4); a stale build/doc dependency (§7).

---

## 2. Symbol resolution — separate STATIC data from DYNAMIC state  (decision: made)

> **Phase 2 DONE (2026-07-09, branch `tcov_beta-phase2`).** Single authoritative resolver +
> dynamic-only manager landed; `SymbolTypeChecker.{h,cpp}` and the Path B class code
> (`CheckSymbolType`/`CreateCoverageForClass`/`EnumerateMembers`, the classify dispatch, and the
> `FindFunctions` re-resolution) are gone. `Function` now composes `SymbolInfo` with **no duplicate mirror
> fields** (only the genuinely-dynamic `startLine` remains alongside it — see D3/D4); `CreateCoverageForSymbol`/
> `CreateCoverageForFunction` take `const SymbolInfo&`; `Begin()` passes the whole struct. D3/D4
> tests landed (`test_breakpoint_func_composesinfo`, `_displaynamedefault`) → **tcov_utests 12/12**.
> **Validated behavior-preserving** against two integration targets:
> - **single-module** symbol (`test_strutil_split`) → LCOV **byte-identical** before/after (proves the
>   resolver+breakpoint rewrite is faithful);
> - **multi-module** symbol (`trun::split`, compiled into *both* `trun` and the test dylib) → **line-level
>   coverage is identical** (diff report = "No changes", `LF/LH 22/22` unchanged, covered/instrumented
>   line sets identical, base % still 75%). The only delta is the **raw breakpoint/hit counts** (`bp 264→132`,
>   `hits 198→99`, LCOV `DA` per-line execution counts ~halved): the old code tallied the same function
>   once per Path-A info × per Path-B `FindFunctions` context (a cross product), so a function present in N
>   loaded modules was over-counted. The new path instruments each module image once. This is an accuracy
>   **fix** (no line changes covered↔uncovered), not a regression — but it is why "byte-identical" holds
>   only for single-module symbols. Real user targets (symbol lives only in the lib under test, not in
>   `trun`) are single-module, so unaffected.
>
> **Not done here (adjacent, left open):** the `IsInProject` stub (last bullet) — a separate filter, also
> tracked in `todo/open-bugs.md`, not part of the static/dynamic split.

**End-state (not a task list):** cleanly split *static* symbol data from *dynamic* coverage
state. `SymbolResolver::ResolveForTarget` becomes the **single source of truth** for everything
static about a symbol — name, source line, and **start AND end** load address.
`BreakpointManager` owns only *dynamic* state — breakpoints and hit counts. The coverage layer
never re-discovers, re-classifies, or re-looks-up a symbol; it consumes fully-resolved
`SymbolInfo`.

There are two mechanisms today, currently **chained**, not parallel:

- **Path A — `SymbolResolver::ResolveForTarget`** (`SymbolResolver.cpp:68`): walks every
  module's symbol table, keeps `eSymbolTypeCode` symbols (`:87`) with valid line info that match
  the `--symbols` globs (`IsCoverageSymbol`, `:52`), and returns
  `SymbolInfo{name, full, file, line, startLoadAddress}` (`:141-148`), where **`startLoadAddress` is
  already a post-relocation LOAD address** (`:94,146`). The newer path (dated 25.03.26) — **but unfinished**.
- **Path B — `BreakpointManager` + `SymbolTypeChecker`** (`Breakpoint.cpp:36`
  `CreateCoverageForSymbol`, `SymbolTypeChecker.cpp:16` `ClassifySymbol`): takes a *name*,
  classifies it function-vs-class via `FindFunctions`/`FindTypes`, and for functions walks the
  compile-unit line table to place breakpoints (`Breakpoint.cpp:145`
  `CreateBreakpointsFunctionRange`); for classes enumerates members via the type API.

**The problem.** `Begin()` (`Coverage.cpp:61-68`) runs Path A, then passes **only `s.name`**
into Path B (`Coverage.cpp:67`), which **re-resolves address/line from scratch** via a second
`FindFunctions` (`Breakpoint.cpp:69`). So:

- Path A's `file`/`line`/`startLoadAddress` (computed at `SymbolResolver.cpp:144-146`) are **discarded** —
  the resolver is effectively just a name filter today.
- The **class half of Path B is unreachable.** Path A only ever emits `eSymbolTypeCode` symbols
  normalized to concrete function names (`SymbolResolver.cpp:87,125,142`), so `ClassifySymbol`
  always matches `IsFuncType` first → `kSymFunc`. `IsClassType`, `CreateCoverageForClass`
  (`Breakpoint.cpp:229`), and `EnumerateMembers` (`:245`) are never hit for real inputs.
  Class→member fan-out still works, but via the glob/prefix matching in `IsCoverageSymbol`
  (`SymbolResolver.cpp:52`), not via the type API.

```
Begin() today — two chained paths

  SymbolResolver::ResolveForTarget         BreakpointManager::CreateCoverageForSymbol
  (computes name+file+line+addr)  --s.name-->  ClassifySymbol -> FindFunctions AGAIN
          |                                          |
          | file/line/addr DISCARDED                 +--> kSymFunc: CreateCoverageForFunction
          v                                          .
         (x)                                         .... kSymClass path: NEVER REACHED
                                                          (CreateCoverageForClass/EnumerateMembers)

After §2 — one authoritative resolver, dynamic-only manager

  SymbolResolver::ResolveForTarget --> SymbolInfo{name, full, file, line, startLoadAddress, endLoadAddress}
  (STATIC: resolves SBFunction start/end + all filtering)
          |
          v
  BreakpointManager (DYNAMIC only): fill Function from SymbolInfo, resolve CU by address,
  install breakpoints across [startLoadAddress,endLoadAddress) — no FindFunctions, no classify
```

**Completion invariant (the test that the refactor is done):**

1. `CreateCoverageForFunction` does **no** LLDB symbol lookup or classification. It takes a
   `const SymbolResolver::SymbolInfo&` and builds its `Function` from it. It MAY use the
   `SBTarget` to *install* breakpoints across the symbol's address range — instrumentation, not
   discovery.
2. `Function` **composes** `SymbolInfo` (holds it as a member); it is not emitted by the resolver.
3. No remaining callers of `CheckSymbolType`, `ClassifySymbol`, or `CreateCoverageForClass`.

**Decisions (made — full rationale in the plan file):**

- `!` **D1 — resolve start+end in the resolver.** `SymbolInfo` already carries
  `startLoadAddress`/`endLoadAddress` (added + named to match `Function` in Phase 0; renamed there
  from `addr`/`endAddr` once they proved self-contained — only `ResolveForTarget` writes them, nothing
  in `Breakpoint.cpp` touches `SymbolInfo`). **DONE:** `ResolveForTarget` now resolves the owning
  `SBFunction` from the symbol's own `SBAddress` (`addr.GetSymbolContext(eSymbolContextFunction|eSymbolContextCompUnit)`)
  and sets `startLoadAddress`/`endLoadAddress` from its start/end load addresses — reproducing today's
  range, more robustly (no by-name ambiguity).
- `!` **D2 — compile unit by address at install time.** **DONE:** `CreateCoverageForFunction` derives
  the `SBCompileUnit` from `info.startLoadAddress` (`ResolveLoadAddress` →
  `ResolveSymbolContextForAddress(…, eSymbolContextCompUnit)`) — address plumbing, not a name lookup.
  `SymbolInfo` stays a pure data struct (no LLDB handles).
- `!` **D3/D4 — `Function` composes `SymbolInfo`.** **DONE:** added `SymbolInfo info;`; `GetDisplayName()`
  returns `info.name` (dropped the now-dead `SBSymbol` member). **Went past the original "keep the scalar
  mirrors" plan** (that was a minimality hedge) — the pure-duplicate mirrors `name`/`startLoadAddress`/
  `endLoadAddress` are **removed**; every read site now goes through `info.full`/`info.startLoadAddress`/
  `info.endLoadAddress` (`GetOrAddFunction` seeds identity via `info.full` = the with-args map key). The
  only remaining scalar is `startLine`, which is **genuinely dynamic, not a mirror**: it is seeded from
  `info.line` but *lowered* while placing breakpoints, and that lowering was measured to fire constantly
  (56× for `trun::`, 13× for `gnilk::`, e.g. `Execute` 133→50) — it is the §6 inlined-code-in-range
  symptom, not a `SymbolInfo` field. Result: `Function` = `SymbolInfo info` (static) + `startLine`/`nHits`/
  `breakpoints` (dynamic) — a clean static/dynamic split. Behavior-neutral: validation re-run byte-identical.
- `!` **D5 — preserve report identity.** **DONE:** the `CompileUnit::functions` map and `Function::name`
  are keyed from `info.full` (with-args); `info.full` is sourced from `sym.GetDisplayName()` (the demangled
  display name), and `info.name` from `NormalizeName(display)`, so `ReportDiff` (uses `name`) and
  `ReportLCOV`/`ReportConsole` (use `GetDisplayName()`) output stays byte-identical.
- `!` **D6 — move the inlined-function guard into the resolver.** **DONE:** the filespec-mismatch check
  moved from `Breakpoint.cpp` into `ResolveForTarget` (compares the resolved function's start line-entry
  filespec against its compile-unit filespec) — static filtering now belongs to the resolver.
- `!` **Delete** `SymbolTypeChecker.{h,cpp}`, and from `BreakpointManager`: `CheckSymbolType`,
  `CreateCoverageForClass`, `EnumerateMembers`, the `kSymClass`/`kSymFunc` dispatch, the
  `SymbolTypeChecker` include, and the `FindFunctions` re-resolution loop. **DONE:** all removed; both
  `CreateCoverageForSymbol` and `CreateCoverageForFunction` now take `const SymbolInfo&`; `Begin()` passes
  the whole `SymbolInfo`.
- `!` Fix the mislabeled logger — `ResolveForTarget` logged as `"SymbolTypeChecker"`; **DONE:** renamed to
  `"SymbolResolver"`.
- `!` *Adjacent (not core to this split):* the `IsInProject` stub (`SymbolResolver.cpp:45-54`,
  `return true;` + dead `/your/project/root/` body) — implement the project-root filter or delete
  it. **Promoted out of this plan (2026-07-14) to its own task: `todo/tcov_isinproject_filter.md`**
  (still open there — the `!` here means "handed off", not "fixed").

> **TDD-guarded:** the D1/D3/D4/D5 behaviours above are pinned by `tcov_utests` tests written *before*
> this refactor (§0) — red pre-refactor, green after.

---

## 3. Dead IPC remnants  (decision: remove)

> **Phase 1 DONE (2026-07-09, branch `tcov_beta-phase1`).** §3 + §4 cleanup, zero behavior change:
> tcov_utests still 10/10 green, and a real coverage run (`trun::split 22/22`) is byte-identical to
> Phase 0. All targets build; `tcov` is now just `tcov.cpp` + `tcovcore`.

The code-driven `trun`→`tcov` coverage RPC was removed from trun
(`todo/done/remove_trun_have_fork.md`); its tcov-side leftovers are inert and should go.

- `!` `Config::ipc_name` — removed from `Config.h`.
- `!` Commented `--tcov-ipc-name` target arg (`Coverage.cpp`) and help line (`tcov.cpp`) — removed.
- `!` Stale `Process()` header comment describing an `IPC_INTERRUPT` path — rewritten (no IPC ref).
- `!` `${ipcsrcfiles}` / `${processsrcfiles}` **and `${unixsrcfiles}`** dropped from the tcov target
  (`app/tcov/CMakeLists.txt`): grep confirmed neither `tcov.cpp` nor the coverage engine references
  Process / dynlib / dirscanner / IPC, and the three groups are coupled (`dynlib_unix`→`Process`,
  `IPCFifoUnix`→ipc), so all three came out together. tcov is now `tcov.cpp` + `tcovcore`; builds
  clean (§7 re-confirmed).

---

## 4. Other dead / inert code (cleanup checklist)

- `!` `CoverageRunner::EnableSelfDebugging` — removed (`Coverage.{h,cpp}`), along with its
  now-orphaned `SBCommandInterpreter` / `SBCommandReturnObject` / `SBStringList` includes.
- `!` `ReportBase::GetShortDisplayName` — removed (`ReportBase.h`, no callers).
- `!` Vestigial `SIGUSR2` handling in `SuppressSignals` — removed (only `SIGUSR1` is awaited). The
  broader "make `SuppressSignals` trun-only" landed in §5/Phase 3 (now gated behind `IsTrunTarget()`).
- `!` Dead `LLDB_DEBUGSERVER_PATH` env branch — **deleted** (not wired). Wiring it to honor a
  user-set env would change Linux behavior — out of scope for a zero-behavior-change phase;
  reintroduce it correctly in §5/Phase 3 if the feature is wanted. (tcov still `setenv`s the
  *detected* lldb-server path for the child — that stays.)
- `!` `Config::cache_dir` + `--cache-dir` flag + `ResolveCacheDir()` — **removed together** (field,
  flag parse, resolver impl + its `<filesystem>`/`TOOL_NAME`, logging). It located no file; wiring
  it into report paths would be a feature, deferred.
- `!` `Config::diffClean` + its dead snapshot-reset branch (`ReportDiff.cpp`) — **removed** (field +
  branch). Binding a CLI flag would be a feature, deferred.
- `!` Hardcoded dev path in DEBUG builds (`Coverage.cpp` `PrepareTrunExecution`) — removed. `target`
  defaults to `"trun"`, so the `target.empty()` block was already unreachable → zero behavior change.

---

## 5. trun coupling & generic-target robustness  (auto-detection retained; mechanism chosen)

Automatic trun detection stays; the mechanism is hardened.

> **Phase 3 DONE (2026-07-10, branch `tcov_beta-phase3`).** Chose **option (c)**: basename
> auto-detect **plus** explicit `--trun` / `--no-trun` overrides (a `Config::TrunDetect` tristate;
> `--trun` forces trun, `--no-trun` forces generic). `SuppressSignals` is now gated behind
> `IsTrunTarget()` (the last ungated trun-only action; the arg-injection + 2nd `WaitState` were
> already gated). New `test_config` TDD module pins the detection (`tcov_utests 16/16`). Validated:
> trun path unchanged (`trun::split` 75%/bp:132, same as Phase 2), `--trun` identical, and
> `--no-trun` on the trun binary **completes without hanging** (generic path — symbols resolved at
> `main`, `bp:77`, no `SIGUSR1` trapping). **Also fixed here:** the target-arg copy
> (`tcov.cpp` `CopyAllAfter`) was itself gated behind `IsTrunTarget()`, so a *generic* target got
> no `--`-args. That guard was introduced by `1758dcb` "NEW: Ability to run coverage on non-trun
> execs" (the immediate child of `3d361be`) purely to dodge `CopyAllAfter`'s `-1` "no `--` present"
> return, which the old unconditional code treated as fatal. Now the copy runs for **every** target
> and a missing `--` is fatal only for trun (which needs its library arg); a generic target may run
> with no extra args. Verified: `--no-trun` now forwards `--sequential -m … lib.dylib` to the target
> (visible in the `Target:` log), trun path unchanged.

- `!` **`IsTrunTarget()` was a substring match** (matched any path containing `"trun"` —
  `trunembedded`, any `.../testrunner/...` dir → false-positive `--coverage`/`--sequential`
  injection then a `SIGUSR1` hang). **DONE:** now `basename(target) == "trun"` (auto), overridable
  by `--trun` / `--no-trun`.
- `!` **Signal trapping was unconditional.** `SuppressSignals` ran for every target → a non-trun
  target using `SIGUSR1` got it trapped/mishandled. **DONE:** `SuppressSignals` is gated to
  `IsTrunTarget()`. (The `PrepareTrunExecution` arg-injection and the 2nd `WaitState` were already
  behind `IsTrunTarget()`, so accurate detection alone also removed the false-positive hang path.)
- **Mechanism chosen: (c)** — basename auto-detect + explicit `--trun`/`--no-trun` override. (a)
  alone left a false-negative on a renamed trun binary; (b) symbol-probe was heavier and not needed.
- `!` **Document the generic-target contract:** **DONE** — comment at the non-trun early-return in
  `RunInitialLLDBPhase`: no late-`dlopen` sync outside trun, so a generic target's post-`main`-loaded
  code is not instrumented; symbols to be covered must be resolvable at the `main` breakpoint.

---

## 6. Coverage-accuracy TODOs (carry forward)

> **Phase 4 DONE (2026-07-10, branch `tcov_beta-phase4`).** Per-item decisions made and
> validated against the trun coverage path:
> - **Cross-file inline leakage → FIXED, default-on.** `CreateBreakpointsFunctionRange` now skips
>   line entries whose file ≠ the compile unit's file. Measured on `trun::split` (in a 175-line
>   `strutil.cpp`): the phantom `DA:305` — libc++ `<string>` code inlined into split — is gone
>   (`LF/LH 22→21`), and a header line coincidentally numbered 59 that inflated `DA:59 4→3` is gone
>   too; `bp 132→130, hits 99→97` = exactly the two phantom cross-file breakpoints, both previously
>   hit. **No real covered line lost** — only phantom counts de-inflate. This is the per-line analog
>   of the resolver's per-function D6 guard.
> - **`startLine`-lowering → REMOVED (the Phase-2 loose end closed).** The lowering only ever fired
>   on leaked line entries: cross-file (now filtered) and same-file neighbours (e.g. a `}` at line 83
>   mapping into `match(char*)`'s range, so it reported `FN:83` for a function declared at 88). It ran
>   *after* breakpoint creation, so it affected only the reported `FN:` line, never placement.
>   Removed it and the now-pure-mirror `Function::startLine` field; reports read `info.line`. Result:
>   every `FN:` now matches the true source declaration (`ltrim 28, rtrim 33, trim 38, split 59,
>   match 88/92` — was `…, match 83/92`). This reverses Phase-2's "keep startLine, it's genuinely
>   dynamic" note: the dynamism *was* the §6 bug. New guard `test_breakpoint_func_startlinefrominfo`
>   (tcov_utests **17/17**).
> - **Prologue lines (bare `}`) → DEFERRED as a documented beta limitation.** The candidate filters
>   (drop column-0 line entries; drop the breakpoint at the exact function start address) are too
>   aggressive — they risk dropping real executable lines — so they are left out by default rather
>   than shipped or flag-gated. Documented in the `tcov.cpp` TODO block and here.
> - **Multi-statement `if (X && Y)` → left PARTIAL, documented.** `ComputeCoverage`'s duplicate-line
>   removal already resolves a line that is both covered and uncovered (counts it covered); true
>   column-level branch distinction is out of scope for beta.
>
> Boundary: the filter is breakpoint-install logic (needs a live `SBTarget`), so it is validated by
> the integration coverage diff, not `tcov_utests` (per §0). The type-level `startLine`→`info.line`
> fold is guarded by the new unit test above.

These were accuracy items for beta, not dead code. The in-source markers in the `tcov.cpp` TODO
block and the (formerly commented) filters in `CreateBreakpointsFunctionRange` are updated in place.

- `!` **Inline members** — cross-file inlined code leaking into a function's range is filtered by
  file (default-on); phantom `DA:` lines and lowered `FN:` starts are gone. See the banner above.
- `!` **Prologue lines** (bare `}`) reported untested — DEFERRED, documented beta limitation (the
  aggressive column-0 / start-address filters risk dropping real lines).
- `!` **Multi-statement** `if (X && Y)` — PARTIAL (same-line covered/uncovered dedup in
  `ComputeCoverage`); column-level branch coverage documented as out of scope.

---

## 7. Build / docs hygiene

> **Phase 5 DONE (2026-07-10, branch `tcov_beta-phase5`).** Docs-only. Verified before editing:
> no `bfd`/`libbfd`/`demangle.h`/binutils headers anywhere in the source or CMake (only a comment
> in `dynlib_unix.cpp` noting `nm` is spawned); `trun` genuinely spawns `nm` (`Process proc("nm")`,
> `dynlib_unix.cpp:161`) so the `binutils` *package* stays a real need; CI installs only
> `liblldb-dev` (not binutils-dev), confirming the build never uses the `-dev` headers.

- `!` **Drop `binutils-dev` (headers/libbfd) from the docs — keep `binutils`.** **DONE.** tcov reads
  DWARF internally via LLDB and never links libbfd; `binutils` is still required, but by **trun**,
  which spawns the `nm` binary to scan libraries (`src/shared/unix/dynlib_unix.cpp:161`).
  - `README.md` — dropped `binutils-dev` and the "binutils headers" wording; now `sudo apt install binutils`.
  - `CLAUDE.md` — "`liblldb-dev` and `binutils-dev`" → `liblldb-dev` (with a note that `binutils`/`nm`
    is still needed by the runner, `-dev` headers are not).
  - CI (`.github/workflows/cmake.yml`) installs only `liblldb-dev`, confirming `-dev` is unused.
- `!` Re-confirmed (Phase 1): the tcov target compiles + links with `${ipcsrcfiles}` /
  `${processsrcfiles}` / `${unixsrcfiles}` all removed — `tcov` is now `tcov.cpp` + `tcovcore`.

---

## 8. Definition of "beta"

tcov leaves *experimental* when:

1. `!` A **single, finished symbol-resolution path** (§2, Phase 2) — `SymbolResolver` authoritative for
   all *static* data (incl. `endLoadAddress`), `BreakpointManager` dynamic-only, `SymbolTypeChecker` +
   Path B class code deleted.
2. `!` **Dead code removed** (§3 IPC remnants, §4 inert code) — done in Phase 1.
3. `!` **Robust trun auto-detection + signal-trap gating** (§5, Phase 3) — basename detect +
   `--trun`/`--no-trun` override; `SuppressSignals` gated to trun. No hang on a false positive, no
   mistrapped signals on a generic target.
4. `!` **Accuracy TODOs (§6) either fixed or explicitly deferred** with the known limitations
   documented for users — done in Phase 4. Cross-file inline leakage fixed (default-on file
   filter) and the buggy `startLine`-lowering removed; prologue `}` and column-level
   multi-statement explicitly deferred as documented limitations (`tcov.cpp` TODO + §6 banner).
5. `!` **tcov has unit tests** — a `tcov_utests` target (§0, 17 cases) with regression coverage of the
   symbol/breakpoint types; the behavior-preserving refactor (§2) was guarded by it plus the
   single-/multi-module integration coverage diffs, and §6's `startLine`→`info.line` fold by
   `test_breakpoint_func_startlinefrominfo`.

> **All five gates met (2026-07-10, after Phase 4).** Every phase (0–5) is on `tcov_beta`.
>
> **Merged down to `dev` (2026-07-14, merge commit `28a9e4f`; `tcov_beta` deleted local +
> origin).** Done as an integration/CI step — to get the Linux + Windows validation the macOS
> maintainer can't run locally — **not** the release-gate promotion. The merge also added a CI
> gate (`.github/workflows/cmake.yml`) that runs `tcov_utests` on Linux (pure logic, no
> `lldb-server` needed). CI on the `dev` push is **green**: both platform builds pass and the
> coverage-engine tests report **17/17** on Linux.
>
> Still outstanding before beta actually ships: a live `tcov` coverage **run** on Linux (CI has
> no `lldb-server`, so tcov's *runtime* on Linux is still unvalidated — only build + engine-logic
> are), the *experimental → beta* label flip in user-facing docs, and `dev → master` — which
> follow the normal release gate (version bump + cross-project validation) and are not taken
> unprompted.

---

## Suggested phasing

Each phase is separately approvable. Phase 0 (test scaffolding) precedes the Phase 2 refactor;
Phases 1 and 5 are safe to do first and independently.

| Phase | Scope | Risk |
|---|---|---|
| 0 | §0 test scaffolding: extract `tcovcore`, `tcov_utests` target + TDD tests for §2 | build refactor + new tests |
| 1 | §3 IPC remnants + §4 dead / inert code | zero behavior change |
| 2 | §2 static/dynamic split (SymbolInfo single source incl. `endLoadAddress`; delete SymbolTypeChecker + Path B class code) | behavior-preserving refactor |
| 3 | §5 trun detection + signal-trap gating | behavior change |
| 4 | §6 accuracy (prologue / inline / multi-statement) | behavior change |
| 5 | §7 build / docs hygiene | docs / build only |

## Branching & merge flow

All beta work lives on a dedicated line off `dev`; each phase is its own short-lived branch that
merges back to the integration branch, and only the finished beta merges down to `dev`.

> **Flow completed (2026-07-14).** Phases 0–5 merged into `tcov_beta`, then `tcov_beta` merged
> down to `dev` (merge commit `28a9e4f`) and the branch was deleted (local + origin). See the §8
> banner for the merge's CI status and what remains before the beta label ships.

- **`tcov_beta`** — long-lived integration branch, cut from `dev`. Holds this plan and every merged phase.
- **`tcov_beta/phase0`, `tcov_beta/phase1`, …** — one branch per phase, cut **from `tcov_beta`**.
- After a phase is done (its tests / validation green), merge the phase branch **back into `tcov_beta`**.
- When the whole beta looks good, merge `tcov_beta` **down into `dev`** — then `dev → master` follows
  the normal release gate (version bump + cross-project validation).

```
dev
 └── tcov_beta                    (integration branch for all phases)
      ├── tcov_beta/phase0  ──►  merge back into tcov_beta
      ├── tcov_beta/phase1  ──►  merge back into tcov_beta
      └── …
 ◄── merge tcov_beta into dev     (when the beta is good)
```

## Files the cleanup will touch (reference)

- `src/coverage/Coverage.{h,cpp}` — resolver call site now passes whole `SymbolInfo` (§2), signal
  trapping / trun gating (§5), dead helpers (§4), DEBUG dev path (§4), IPC comment (§3)
- `src/coverage/SymbolResolver.{h,cpp}` — authoritative static resolver: populate `startLoadAddress`/`endLoadAddress`, resolve
  `SBFunction` start/end, move the inline guard, logger rename (§2)
- `src/coverage/SymbolTypeChecker.{h,cpp}` — **delete** (§2)
- `src/coverage/Breakpoint.{h,cpp}` — `SymbolInfo`-based signatures, `Function` composes
  `SymbolInfo`, CU-by-address, delete class path + classifier include (§2), accuracy filters (§6)
- `src/coverage/Config.{h,cpp}` — `IsTrunTarget` (§5); `ipc_name` (§3); `cache_dir`/`ResolveCacheDir` (§4); `diffClean` (§4)
- `src/coverage/reporting/ReportBase.h` — dead helper (§4); `reporting/ReportDiff.cpp` — `diffClean` (§4),
  verify function-name output unchanged (§2); `reporting/ReportLCOV.cpp` — verify `startLine` output unchanged (§2)
- `src/app/tcov/tcov.cpp` — help text (§3), `LLDB_DEBUGSERVER_PATH` branch (§4), `--cache-dir` (§4), signal-gating TODO (§5)
- `src/app/tcov/CMakeLists.txt` — IPC/process shared-source slim-down (§3); drop `SymbolTypeChecker` sources (§2); engine sources move to `tcovcore` (§0)
- `src/coverage/CMakeLists.txt` — **new**: `tcovcore` static lib + LLDB wiring (§0)
- `src/coverage/tests/` — **new**: `test_main.cpp`, `test_symbolresolver.cpp`, `test_breakpoint.cpp`, `CMakeLists.txt` → `tcov_utests` (§0)
- `CMakeLists.txt` — add the `tcov_utests` subdir under the `BUILD_TCOV` block (§0)
- `README.md`, `CLAUDE.md` — `binutils-dev` dependency (§7)

## Validation recipe (for each future phase)

After each phase: build `tcov`, then run it against **both** a trun library and a non-trun
target, and diff the coverage report before/after to confirm no regression in the numbers.

```bash
cd cmake-build-debug && ninja tcov

# trun path
./tcov -t ./trun -s <syms> -- -m <module> lib/libtrun_utests.so

# generic-target path
./tcov -t ./otherexe -s <syms>
```

(The document itself is this session's only deliverable; the recipe above is for the
implementation phases, not this session.)
