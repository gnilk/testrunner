## Open bugs / cleanups (ungrouped)

Catch-all for one-off findings that don't yet warrant their own file. Promote to
a dedicated doc if a cluster grows around any of them.

### Dead code drifting from the live paths

- ✅ RESOLVED (`cleanup/dead-module-parallel`, 2026-06-29) —
  `TestModuleExecutorParallel` (thread-per-module, was `moduleexecutors.cpp:167`)
  deleted. It was unreachable in every build: desktop has `TRUN_HAVE_FORK` so the
  factory maps `kParallel -> forkExecutor`; embedded lacks `TRUN_HAVE_THREADS` so it
  never compiled. Suite unchanged (fork == seq == 102/15).
- [folded into `embedded_impl.md`] `old_Config_FromArguments` switch-parser
  (`config.cpp`) — guarded by `TRUN_EMBEDDED`, only call site commented out
  (`config.cpp:121`), returns `kError` under EMBEDDED, never called (embedded uses
  `RunTests()`). It's embedded-specific dead code living in a file the embedded
  rewrite will replace, so the cleanup is tracked there, not here.

### Coverage

- `SymbolResolver::IsInProject` (`src/coverage/SymbolResolver.cpp:41-50`) is a
  no-op: it `return true;` on the first line, leaving the real path-filtering
  logic dead below it — including a hardcoded `/your/project/root/` placeholder
  that would never match anyway. The "don't enumerate symbols outside the project
  root" filter does nothing; coverage relies entirely on the `IsCoverageSymbol`
  name filter. Either implement the project-root resolution or drop the function.

### Minor

- ✅ RESOLVED (`cleanup/dead-module-parallel`, 2026-06-29) —
  `--continue_on_assert` / `--continue-on-assert` deprecation handling
  (`config.cpp`) simplified: each spelling is now checked exactly once
  (was four `IsPresent` calls across two spellings).
