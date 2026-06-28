## Open bugs / cleanups (ungrouped)

Catch-all for one-off findings that don't yet warrant their own file. Promote to
a dedicated doc if a cluster grows around any of them.

### Dead code drifting from the live paths

- `old_Config_FromArguments` switch-parser (`config.cpp:217-359`) — guarded by
  `TRUN_EMBEDDED` and the only call site is commented out (`config.cpp:121`).
  Ironically it has the *correct* `-t`/`-m` handling that the live parser gets
  wrong (see `config_arg_bugs.md`). Either delete it or make the embedded path
  actually use it.
- `TestModuleExecutorParallel` (thread-per-module, `moduleexecutors.cpp:167`) —
  unreachable whenever `TRUN_HAVE_FORK` is defined (always on macOS/Linux),
  since the factory maps `kParallel -> forkExecutor`. Superseded by the fork
  executor on purpose; remove or clearly mark as legacy.

### Coverage

- `SymbolResolver::IsInProject` (`src/coverage/SymbolResolver.cpp:41-50`) is a
  no-op: it `return true;` on the first line, leaving the real path-filtering
  logic dead below it — including a hardcoded `/your/project/root/` placeholder
  that would never match anyway. The "don't enumerate symbols outside the project
  root" filter does nothing; coverage relies entirely on the `IsCoverageSymbol`
  name filter. Either implement the project-root resolution or drop the function.

### Minor

- `--continue_on_assert` / `--continue-on-assert` deprecation handling
  (`config.cpp:130-137`) is more convoluted than needed (checks presence three
  times across two spellings). Simplify.
