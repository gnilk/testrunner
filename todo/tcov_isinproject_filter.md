# tcov — resolve the `SymbolResolver::IsInProject` no-op filter

> **Status: open, needs a maintainer decision (implement vs delete).** Promoted 2026-07-14 from the
> `todo/open-bugs.md` catch-all to its own task. Split out of the tcov beta cleanup as *adjacent, not
> core* (`todo/done/tcov_cleanup.md` §2 last bullet) — it is a separate source-file filter, unrelated
> to the static/dynamic symbol-resolution split that Phase 2 finished.
>
> Markers: `-` open, `+` in progress, `!` done.

## What it is

`SymbolResolver::IsInProject` (`src/coverage/SymbolResolver.cpp:45-54`) is a **no-op**: it
`return true;` on its first line, so the real body below — path resolution plus a hardcoded
`/your/project/root/` placeholder that would never match anyway — is **dead code**.

It is called once, at `SymbolResolver.cpp:126`, guarding the resolve loop:

```cpp
if (!IsInProject(fileSpec)) {   // always false -> never skips anything
    continue;
}
```

The intent was "don't enumerate symbols whose source file lives outside the project root." Today
that filter does nothing; symbol scoping relies entirely on:

- **`IsCoverageSymbol`** (`:56`) — the `--symbols` glob/prefix filter (the real, user-driven scope);
- **`IsJunkSymbol`** (`:34`) — drops `@plt` / `__`-prefixed compiler noise.

So `IsInProject` adds no behavior and is the last `FIXME`-marked loose end in the coverage engine.

## Decision needed

- `-` **Option A — delete it.** Remove the function and the `:126` call site. `--symbols` already
  scopes coverage by name, so nothing is lost. Leanest option; matches the "prefer good defaults, no
  dead knobs" bias. **Leading candidate** unless there's a concrete case the name filter can't cover.
- `-` **Option B — implement a real project-root filter.** Needs a source of truth for "the root,"
  which tcov has no notion of today. Sub-decisions: where does the root come from (a new
  `--project-root` flag? infer from CWD? from the target binary's dir?), and does it earn its keep
  given `--symbols` largely overlaps it. Per the project's *prefer-good-defaults-over-flags* rule,
  **do not add a flag unilaterally** — this needs a maintainer call on whether the feature is wanted.

**Recommendation:** delete (Option A) unless a real "same symbol name, wrong source tree" case is
identified — in which case Option B, but only after deciding the root-resolution mechanism.

## If implementing (Option B) — TDD note

`ResolveForTarget` needs a live `SBTarget`, so the loop isn't unit-testable in `tcov_utests` (per the
`tcov_cleanup.md` §0 boundary). But a project-root **predicate** should be factored into a pure,
path-in / bool-out helper and pinned with a `test_symbolresolver` case (root match / non-match /
missing-path), mirroring the existing pure-logic tests. The live filtering itself is validated by the
integration coverage diff.

## Files

- `src/coverage/SymbolResolver.cpp` — the function (`:45-54`) + its call site (`:126`).
- `src/coverage/tests/test_symbolresolver.cpp` — where a predicate test would land (Option B).

## Cross-references

- `todo/done/tcov_cleanup.md` — §2 last bullet + the RESOLVED banner (originally pointed here via
  `open-bugs.md`).
- `todo/open-bugs.md` — Coverage section now carries a one-line pointer to this doc.
