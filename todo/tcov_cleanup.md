## tcov architectural cleanup — experimental → beta

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

## 0. Test scaffolding — TDD prerequisite (`tcovutest`)

tcov has **zero** unit tests today (built exploratively). Before touching the symbol/breakpoint
layer we stand up a `trun`-style unit-test target and add tests **TDD-first** — red before a
refactor, green after — so "behavior-preserving" becomes executable and intent is documented.

- `!` **Decided:** extract the coverage engine (`tcovsrcfiles`) into a **`tcovcore` static
  library**; `tcov` and a new `tcovutest` shared lib both link it (LLDB as a PUBLIC usage
  requirement). No duplicated LLDB wiring — mirrors the `trunlib` static-lib pattern.
- `-` New `src/coverage/tests/` with `trun`-convention tests, built as `libtcovutest.{so,dylib}`
  (gated behind `BUILD_TCOV`, linking `tcovcore` + `trun_common_options`): global
  `test_main`/`test_exit` skeleton, then one `test_<module>.cpp` per unit (each with its
  `test_<module>` / `test_<module>_exit` main/exit, even if empty) — `symbolresolver`, `breakpoint`.
  Run via `./trun lib/libtcovutest.dylib`.
- `-` The first real tests encode §2's decisions and are written **before** the Phase 2 refactor:
  `SymbolInfo.endAddr` default (D1); `Function` composes `SymbolInfo` with `GetDisplayName()==info.name`
  (D3/D4); overloads stay distinct via the `info.full` map key (D5). These need only the tcov *types*.
- Boundary: `ResolveForTarget` + breakpoint install need a live `SBTarget`, so they stay covered by
  the integration before/after coverage diff, not `tcovutest`.

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
  `SymbolInfo{name, full, file, line, addr}` (`:141-148`), where **`addr` is already a
  post-relocation LOAD address** (`:94,146`). The newer path (dated 25.03.26) — **but unfinished**.
- **Path B — `BreakpointManager` + `SymbolTypeChecker`** (`Breakpoint.cpp:36`
  `CreateCoverageForSymbol`, `SymbolTypeChecker.cpp:16` `ClassifySymbol`): takes a *name*,
  classifies it function-vs-class via `FindFunctions`/`FindTypes`, and for functions walks the
  compile-unit line table to place breakpoints (`Breakpoint.cpp:145`
  `CreateBreakpointsFunctionRange`); for classes enumerates members via the type API.

**The problem.** `Begin()` (`Coverage.cpp:61-68`) runs Path A, then passes **only `s.name`**
into Path B (`Coverage.cpp:67`), which **re-resolves address/line from scratch** via a second
`FindFunctions` (`Breakpoint.cpp:69`). So:

- Path A's `file`/`line`/`addr` (computed at `SymbolResolver.cpp:144-146`) are **discarded** —
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

  SymbolResolver::ResolveForTarget --> SymbolInfo{name, full, file, line, addr, endAddr}
  (STATIC: resolves SBFunction start/end + all filtering)
          |
          v
  BreakpointManager (DYNAMIC only): fill Function from SymbolInfo, resolve CU by address,
  install breakpoints across [addr,endAddr) — no FindFunctions, no classify
```

**Completion invariant (the test that the refactor is done):**

1. `CreateCoverageForFunction` does **no** LLDB symbol lookup or classification. It takes a
   `const SymbolResolver::SymbolInfo&` and builds its `Function` from it. It MAY use the
   `SBTarget` to *install* breakpoints across the symbol's address range — instrumentation, not
   discovery.
2. `Function` **composes** `SymbolInfo` (holds it as a member); it is not emitted by the resolver.
3. No remaining callers of `CheckSymbolType`, `ClassifySymbol`, or `CreateCoverageForClass`.

**Decisions (made — full rationale in the plan file):**

- `-` **D1 — resolve start+end in the resolver.** Add `endAddr` to `SymbolInfo`; in
  `ResolveForTarget` resolve the owning `SBFunction` from the symbol's own `SBAddress` (the
  commented hint at `SymbolResolver.cpp:137-139`) and set `addr`/`endAddr` from its start/end load
  addresses — reproducing today's range exactly, and more robustly (no by-name ambiguity).
- `-` **D2 — compile unit by address at install time.** The range-walk still needs an
  `SBCompileUnit`; `CreateCoverageForFunction` derives it from `info.addr` (`ResolveLoadAddress`
  → `ResolveSymbolContextForAddress(…, eSymbolContextCompUnit)`) — address plumbing, not a name
  lookup. `SymbolInfo` stays a pure data struct (no LLDB handles).
- `-` **D3/D4 — `Function` composes `SymbolInfo`.** Add `SymbolInfo info;`; `GetDisplayName()`
  returns `info.name` (drops the now-dead `SBSymbol` member). Keep the scalar mirrors
  (`startLine`/`startLoadAddress`/`endLoadAddress`/`name`) filled once from `info`, so the
  debug-dump loop (`Breakpoint.cpp:54-61`) and the reports stay unchanged.
- `-` **D5 — preserve report identity.** Key the `CompileUnit::functions` map and `Function::name`
  from `info.full` (with-args); ensure `info.full` is the demangled display name so
  `ReportDiff.cpp:271` / `ReportLCOV.cpp:71` output stays byte-identical.
- `-` **D6 — move the inlined-function guard into the resolver** (the filespec-mismatch check at
  `Breakpoint.cpp:92-99`) — static filtering belongs in the resolver.
- `-` **Delete** `SymbolTypeChecker.{h,cpp}`, and from `BreakpointManager`: `CheckSymbolType`
  (`Breakpoint.cpp:21`), `CreateCoverageForClass` (`:229`), `EnumerateMembers` (`:245`), the
  `kSymClass`/`kSymFunc` dispatch in `CreateCoverageForSymbol` (`:41-52`), the `SymbolTypeChecker`
  include, and the `FindFunctions` re-resolution loop in `CreateCoverageForFunction`. Both
  `CreateCoverageForSymbol` and `CreateCoverageForFunction` change to take `const SymbolInfo&`;
  `Begin()` (`Coverage.cpp:66-68`) passes the whole `SymbolInfo`.
- `-` Fix the mislabeled logger — `ResolveForTarget` logs as `"SymbolTypeChecker"`
  (`SymbolResolver.cpp:70`); rename to `"SymbolResolver"`.
- `-` *Adjacent (not core to this split):* the `IsInProject` stub (`SymbolResolver.cpp:41-50`,
  `return true;` + dead `/your/project/root/` body) — implement the project-root filter or delete
  it. (Also tracked in `todo/open-bugs.md`.)

> **TDD-guarded:** the D1/D3/D4/D5 behaviours above are pinned by `tcovutest` tests written *before*
> this refactor (§0) — red pre-refactor, green after.

---

## 3. Dead IPC remnants  (decision: remove)

The code-driven `trun`→`tcov` coverage RPC was removed from trun
(`todo/done/remove_trun_have_fork.md`); its tcov-side leftovers are inert and should go.

- `-` `Config::ipc_name` (`Config.h:41`) — never read anywhere.
- `-` Commented `--tcov-ipc-name` target arg (`Coverage.cpp:131`) and help line (`tcov.cpp:157`).
- `-` Stale `Process()` header comment describing an `IPC_INTERRUPT` path that no longer exists
  (`Coverage.cpp:358-361`).
- `-` `${ipcsrcfiles}` / `${processsrcfiles}` linked into tcov (`app/tcov/CMakeLists.txt:17,19`)
  but never referenced by the coverage engine — remove from the target (or, if kept for a
  concrete reason, document why). Re-confirm the shared-source list builds cleanly after (§7).

---

## 4. Other dead / inert code (cleanup checklist)

- `-` `CoverageRunner::EnableSelfDebugging` (`Coverage.h:36`, `Coverage.cpp:200-232`) — no
  callers. An LLDB-internal-logging debug aid; delete or move behind an explicit debug hook.
- `-` `ReportBase::GetShortDisplayName` (`ReportBase.h:20-23`) — no callers.
- `-` Vestigial `SIGUSR2` handling in `SuppressSignals` (`Coverage.cpp:262-264`) — only `SIGUSR1`
  is ever awaited (`sig_DYNLIB_LOADED`, `Coverage.h:51`). Remove the `SIGUSR2` block (and see §5:
  the whole `SuppressSignals` call should become trun-only).
- `-` Dead `LLDB_DEBUGSERVER_PATH` env branch (`tcov.cpp:288-295`, latent bug): it reads the env
  var into `currentLLDB`, then tests a freshly-constructed **empty** `currentLLDBPath`
  (`tcov.cpp:289-290`), so `!currentLLDBPath.empty()` is always false and the branch can never
  fire. Either wire `currentLLDBPath = currentLLDB;` or delete the block.
- `-` `Config::cache_dir` + `--cache-dir` flag + `ResolveCacheDir()` — the flag is parsed
  (`tcov.cpp:168`), resolved (`Config.cpp:27`), stored (`tcov.cpp:172`), and logged
  (`tcov.cpp:201`), but the value is **never used to locate any file** (report names are plain
  relative — `Config::lcovReportFilename`, `diffReportFilename`). Either wire it into the report
  paths (`ReportDiff`/`ReportLCOV`) or remove **flag + resolver + field + logging together**.
- `-` `Config::diffClean` (`Config.h:38`, read at `ReportDiff.cpp:294`) — never set to `true`,
  so the snapshot-reset branch is dead. Either bind a CLI flag to it or remove field + branch.
- `-` Hardcoded dev path in DEBUG builds (`Coverage.cpp:86-89`,
  `/Users/gnilk/.../trun`) — remove, or gate behind an env var.

---

## 5. trun coupling & generic-target robustness  (auto-detection retained; mechanism open)

Automatic trun detection stays, but the mechanism needs hardening. This section presents the
issue and options; the specific choice is deferred to when Phase 3 is approved.

- `-` **`IsTrunTarget()` is a substring match** gating real behavior (`Config.cpp:21-24`), with
  the author's own `// FIXME: this is not correct!`. It matches any path containing `"trun"`
  (e.g. `trunembedded`, or any `.../testrunner/...` directory). A false positive makes tcov
  inject `--coverage`/`--sequential` (`Coverage.cpp:71-80` `PrepareTrunExecution`) and then hang
  in `RunInitialLLDBPhase` waiting for a `SIGUSR1` that never comes (`Coverage.cpp:191-194`).
- `-` **Signal trapping is unconditional.** `SuppressSignals` is called for every target right
  after launch (`Coverage.cpp:146`), so a non-trun target that legitimately uses `SIGUSR1`/
  `SIGUSR2` gets its signals trapped and mishandled → `"Unhandled stop reson"`
  (`Coverage.cpp:382`). This is exactly the open TODO at `tcov.cpp:45-46`. **Gate signal
  trapping (and the `PrepareTrunExecution` arg-injection + the second `WaitState`) to trun-sync
  mode only.** Note `RunInitialLLDBPhase` already early-returns for non-trun targets after the
  `main` breakpoint (`Coverage.cpp:177-179`) — the signal setup just runs too early to benefit.
- Options for keeping auto-detection but making detection robust (pick one at Phase 3):
  - **(a)** match basename `== "trun"` rather than substring — smallest change, still filename-based.
  - **(b)** after launch, probe the target via LLDB for a trun-specific exported symbol —
    robust, no reliance on the filename.
  - **(c)** auto-detect (a or b) **plus** an explicit `--trun` / `--no-trun` override for the
    ambiguous cases.
- `-` **Document the generic-target contract:** there is no late-`dlopen` sync outside trun, so a
  generic target that loads code after `main` won't get those symbols instrumented. Symbols must
  be resolvable at the `main` breakpoint.

---

## 6. Coverage-accuracy TODOs (carry forward)

These are accuracy items for beta, not dead code. They already have in-source markers in the
`tcov.cpp:27-47` TODO block and in commented filters in `CreateBreakpointsFunctionRange`
(`Breakpoint.cpp:163-195`) — carry them forward, don't duplicate them.

- `-` **Prologue lines** (bare `}` and other intermediate lines) reported as untested
  (`tcov.cpp:39`; the commented start-address/prologue filter at `Breakpoint.cpp:190-195`).
- `+` **Inline members / multi-statement** coverage — `if (X && Y)` where `Y` may not evaluate,
  and inlined functions leaking into the wrong compile unit (`tcov.cpp:34,38`; commented
  inline/column filters at `Breakpoint.cpp:163-180`; partial handling already in
  `ComputeCoverage`'s duplicate-line removal, `Breakpoint.cpp:324-339`).

The commented filter blocks note they should sit behind a flag ("aggressive filtering") — decide
per item whether to enable by default or expose as an option when Phase 4 is approved.

---

## 7. Build / docs hygiene

- `-` **Drop `binutils-dev` (headers/libbfd) from the docs — keep `binutils`.** tcov reads DWARF
  internally via LLDB and never links libbfd. `binutils` itself is still required, but by
  **trun**, which spawns the `nm` binary to scan libraries (`src/shared/unix/dynlib_unix.cpp:9`).
  So the `binutils-dev` line is dead:
  - `README.md:102-106` — the block says "also install the binutils headers" then
    `sudo apt install binutils binutils-dev`; keep `binutils`, drop `binutils-dev` and the
    "headers" wording.
  - `CLAUDE.md:48` — "`liblldb-dev` and `binutils-dev`" → drop `binutils-dev`.
  - CI (`.github/workflows/cmake.yml`) already installs neither, confirming `-dev` is unused.
- `-` Re-confirm the tcov target's shared-source list after the §3 IPC slim-down (it should still
  compile without `${ipcsrcfiles}`/`${processsrcfiles}`).

---

## 8. Definition of "beta"

tcov leaves *experimental* when:

1. `-` A **single, finished symbol-resolution path** (§2) — `SymbolResolver` authoritative for all
   *static* data (incl. `endAddr`), `BreakpointManager` dynamic-only, `SymbolTypeChecker` + Path B
   class code deleted.
2. `-` **Dead code removed** (§3 IPC remnants, §4 inert code).
3. `-` **Robust trun auto-detection + signal-trap gating** (§5) — no hang on a false positive, no
   mistrapped signals on a generic target.
4. `-` **Accuracy TODOs (§6) either fixed or explicitly deferred** with the known limitations
   documented for users.
5. `-` **tcov has unit tests** — a `tcovutest` target (§0) with regression coverage of the
   symbol/breakpoint layer; the behavior-preserving refactor (§2) is guarded by it.

---

## Suggested phasing

Each phase is separately approvable. Phase 0 (test scaffolding) precedes the Phase 2 refactor;
Phases 1 and 5 are safe to do first and independently.

| Phase | Scope | Risk |
|---|---|---|
| 0 | §0 test scaffolding: extract `tcovcore`, `tcovutest` target + TDD tests for §2 | build refactor + new tests |
| 1 | §3 IPC remnants + §4 dead / inert code | zero behavior change |
| 2 | §2 static/dynamic split (SymbolInfo single source incl. `endAddr`; delete SymbolTypeChecker + Path B class code) | behavior-preserving refactor |
| 3 | §5 trun detection + signal-trap gating | behavior change |
| 4 | §6 accuracy (prologue / inline / multi-statement) | behavior change |
| 5 | §7 build / docs hygiene | docs / build only |

## Branching & merge flow

All beta work lives on a dedicated line off `dev`; each phase is its own short-lived branch that
merges back to the integration branch, and only the finished beta merges down to `dev`.

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
- `src/coverage/SymbolResolver.{h,cpp}` — authoritative static resolver: add `endAddr`, resolve
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
- `src/coverage/tests/` — **new**: `test_main.cpp`, `test_symbolresolver.cpp`, `test_breakpoint.cpp`, `CMakeLists.txt` → `tcovutest` (§0)
- `CMakeLists.txt` — add the `tcovutest` subdir under the `BUILD_TCOV` block (§0)
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
