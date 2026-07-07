## Open bugs / cleanups (ungrouped)

Catch-all for one-off findings that don't yet warrant their own file. Promote to a
dedicated doc if a cluster grows around any of them.

> Resolved items were archived to `todo/done/resolved-bugs.md` (2026-07-06). This file is
> **open items only**.

### Coverage

- **`int_tcov_begincov` / `ITestingCoverage::BeginCoverage` is now a no-op** (since 2026-07-06).
  The `TRUN_HAVE_FORK` removal (`todo/done/remove_trun_have_fork.md`) deleted the code-driven
  coverage RPC but kept the `ITestingCoverage` interface + `QueryInterface` per that work package's
  scope. So `t->QueryInterface(ITestingCoverage_IFace_ID, ...)->BeginCoverage(sym)` now does nothing
  (`responseproxy.cpp`). The whole `QueryInterface`/`ITestingConfig`/`ITestingCoverage` extension
  mechanism is experimental + unused outside its own tests — candidate for removal from `ITesting`.
  tcov's `--symbols` static-breakpoint path is the supported coverage workflow and is unaffected.
- **`SymbolResolver::IsInProject`** (`src/coverage/SymbolResolver.cpp:41-50`) is a no-op: it
  `return true;` on the first line, leaving the real path-filtering logic dead below it — including a
  hardcoded `/your/project/root/` placeholder that would never match anyway. The "don't enumerate
  symbols outside the project root" filter does nothing; coverage relies entirely on the
  `IsCoverageSymbol` name filter. Either implement the project-root resolution or drop the function.

### IPC (latent, low priority)

- **Concurrent large IPC frames > PIPE_BUF can interleave on the shared FIFO.** All children share
  one FIFO and run `maxConcurrency` at once; a child flushes its whole `IPCResultSummary` as one
  `write()`, only atomic up to `PIPE_BUF` (4096 Linux / 512 macOS), so two large concurrent frames
  can interleave into a spliced stream. **Probably not an issue in practice — result messages are
  small** — recorded for very large forked suites. The single-writer robustness that used to compound
  this is fixed (the decoder reads the whole frame before parsing; `IPCFifoUnix::Write` loops on
  partial writes). A true fix for the *concurrent* case would need per-child FIFOs or a
  length-delimited multiplex.

### Semantics / platform (deferred)

- **Test-case dependency results are computed but never `AddResult`'d** (`ExecuteDependencies`). Left
  alone as a dependency-accounting *semantics* decision entangled with the `depends` tests, not a
  mechanical cleanup.
- **Windows no-exceptions `TerminateThread(GetCurrentThread(), 0)` unwinding gap**
  (`responseproxy.cpp`, non-default build). Forced thread termination without stack unwinding; only
  the `TRUN_HAVE_EXCEPTIONS` build (the default) unwinds cleanly via the thrown `TestAbortException`.
