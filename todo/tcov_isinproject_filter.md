# tcov — implement the `SymbolResolver::IsInProject` project-scope filter

> **Status: open — design + implement (do NOT delete).** Intentional skeleton, not dead code.
> Promoted 2026-07-14 from the `todo/open-bugs.md` catch-all to its own task; split out of the tcov
> beta cleanup as *adjacent, not core* (`todo/done/tcov_cleanup.md` §2).
>
> Markers: `-` open, `+` in progress, `!` done.

## Intent (maintainer)

`IsInProject` is a deliberate placeholder for an unfinished idea: while scanning a target's symbols,
detect whether a symbol **belongs to the project** vs comes **from the outside** (system /
third-party libraries), so out-of-project symbols can be **skipped early** in the filtering loop
instead of being instrumented.

**Preferred approach — derive the root, don't require a flag.** The best outcome is to **safely
detect the project root from the input tcov already has** and use it automatically — a good default,
no configuration. A **`--project-dir <path>` flag stays available only as an *override***: to force
the root when auto-detection is wrong or ambiguous, or to deliberately narrow/widen scope. The flag
is an escape hatch, **not** the primary mechanism. (Matches the project's
*prefer-good-defaults-over-flags* rule.)

## Current state

`SymbolResolver::IsInProject` (`src/coverage/SymbolResolver.cpp:45-54`) is a **no-op**: it
`return true;` on its first line, so the body below — path resolution plus a hardcoded
`/your/project/root/` placeholder — is inert. It is called once, at `SymbolResolver.cpp:126`:

```cpp
auto fileSpec = lineEntry.GetFileSpec();   // the symbol's SOURCE file
...
if (!IsInProject(fileSpec)) {   // today: always true -> nothing is ever skipped
    continue;
}
```

Scoping today relies entirely on:
- **`IsCoverageSymbol`** (`:56`) — the `--symbols` glob/prefix filter (user-driven, name-based);
- **`IsJunkSymbol`** (`:34`) — drops `@plt` / `__`-prefixed compiler noise.

`IsInProject` adds an **orthogonal, path-based** scope on top of the name-based one.

## Direction

### 1. Derive the project root (primary path)

Candidates from input tcov already has — to evaluate, likely combine:

- `-` **Target / library-under-test debug info** — the primary module's DWARF `comp_dir`
  (`DW_AT_comp_dir`, the build directory) and/or the source-file dirs of its own symbols. The thing
  being covered defines the project.
- `-` **Common ancestor of the in-scope source files** — every `--symbols`-matched symbol carries a
  source `fileSpec`; their longest common directory prefix is a strong project-root signal, derived
  straight from what the user asked to cover.
- `-` **System-prefix backstop** — treat well-known external roots (`/usr/`, `/Library/`, SDK /
  toolchain / sysroot paths) as definitely-outside, so derivation still degrades safely when the
  positive signal is weak.
- `-` **Safety fallback (must-have):** if no root can be confidently derived **and** no
  `--project-dir` is given, keep today's behavior (`return true` → scan everything). The filter must
  only ever *narrow* when it is sure — never silently over-filter and drop real coverage.

### 2. `--project-dir <path>` override (secondary)

- `-` New `Config` option (`src/coverage/Config.{h,cpp}` + `tcov.cpp` parse + `--help`). When set it
  **forces** the root and skips derivation.

### 3. Apply it — `IsInProject` + skip granularity

- `-` `IsInProject(fileSpec)` → true iff the source dir resolves **under** the (derived or forced)
  root.
- `-` Decide the "skip early" granularity — two levels, maybe both:
  - *Per-source-file* (the current call site, `:126`): only reachable **after** line-info resolution
    (you need the line entry to know the source file), so it is not actually "early".
  - *Per-module (genuinely early)*: skip a whole outside module up front via `module.GetFileSpec()`
    (the `.so`/`.dylib`/exe path, in the module loop `:80`) — avoids walking every symbol of a system
    library. This is the real "skip early" efficiency win the intent points at.

## Gotcha (the original `FIXME: which can be tricky`)

Source paths come from **DWARF**, recorded at **build time** — they may be absolute (build-machine
paths) or relative (to `comp_dir`), and need not match the runtime machine's layout. Both derivation
and matching have to handle: absolute-vs-relative fileSpecs, symlink / `..` normalization, and
cross-machine builds (debug info produced elsewhere). Normalize both sides and compare path prefixes;
document what is and isn't supported. This trickiness is exactly why the root should be **derived and
sanity-checked**, with the flag as the manual override when the heuristic can't be sure.

## TDD

`ResolveForTarget` needs a live `SBTarget`, so the loop isn't unit-testable in `tcov_utests`
(`tcov_cleanup.md` §0 boundary). Factor **both** the root-**derivation** and the under-root
**predicate** into pure functions (paths in → root / bool out) and pin them in `test_symbolresolver`:
common-prefix derivation, system-prefix exclusion, under/outside match, relative + `..` +
trailing-slash normalization, and the "can't derive + no flag → scan-all" fallback. The live
filtering is validated by the integration coverage diff (scan with/without a root, confirm
out-of-project symbols drop and in-project coverage is unchanged).

## Files

- `src/coverage/SymbolResolver.cpp` — `IsInProject` (`:45-54`) + its call site (`:126`); the
  derivation (reads module `comp_dir` / source paths) and optionally an early module-level skip in
  the module loop (`:80`).
- `src/coverage/Config.{h,cpp}` — new `--project-dir` override field + parse.
- `src/app/tcov/tcov.cpp` — arg wiring + `--help` line.
- `src/coverage/tests/test_symbolresolver.cpp` — the pure derivation + predicate tests.

## Cross-references

- `todo/done/tcov_cleanup.md` — §2 last bullet + RESOLVED banner (item handed off here).
- `todo/open-bugs.md` — Coverage section carries a one-line pointer to this doc.
