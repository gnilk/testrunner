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

- **[new, 2026-07-06] `int_tcov_begincov` / `ITestingCoverage::BeginCoverage` is now a no-op.**
  The `TRUN_HAVE_FORK` removal (`todo/done/remove_trun_have_fork.md`) deleted the code-driven
  coverage RPC but kept the `ITestingCoverage` interface + `QueryInterface` per that work package's
  scope. So `t->QueryInterface(ITestingCoverage_IFace_ID, ...)->BeginCoverage(sym)` now does nothing
  (`responseproxy.cpp`). The whole `QueryInterface`/`ITestingConfig`/`ITestingCoverage` extension
  mechanism is experimental + unused outside its own tests — candidate for removal from `ITesting`
  (out of scope there; noted here for discoverability). tcov's `--symbols` static-breakpoint path is
  the supported coverage workflow and is unaffected.
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

### testrunner core — audit 2026-07-05

Focused bug sweep of the runner core (coverage out of scope). The suite stays green
because the two worst defects sit on paths the self-suite doesn't exercise. Confirmed
real: #1, #2, #3.

- ✅ RESOLVED (`fix/fatal-abort-result-decision`, commit 9a3179a) — #1 below. The result decision is
  split out of the thread/exception control flow into a pure `TestResult::DeriveResult(...)`
  and the forced abort-unwind is flagged distinctly from a user C++ exception
  (`TestResponseProxy::SetForciblyTerminated`), so Fatal keeps ModuleFail / Abort keeps
  AllFail. Covered by pure unit tests (`tests/test_resultdecision.cpp`, module
  `resultdecision`) that assert `DeriveResult`/`CheckIfContinue` without terminating.
  The end-to-end abort fixtures stay excluded from the inline run via `run_test_suite.sh`
  (documented there) rather than a subprocess observer (build/maintenance cost not worth it).
  Follow-on work in the same branch then aligned fork with sequential for `Abort`/`AllFail`
  (see the fork-`AllFail` resolved item below).
- **[#1 — FIXED, see resolved item above] `t->Fatal()` / `t->Abort()` used to not stop the module /
  the run** in the
  default (threaded + exceptions) build. `testfunc.cpp:162-171`: forced termination
  throws `TestAbortException`; the catch sets `exceptionThrown`, and `CreateTestResult`'s
  `else` branch then overwrites the result with `kTestResult_TestFail`, clobbering the
  `ModuleFail`/`AllFail` that `Fatal`/`Abort` set (`responseproxy.cpp:188-212`). So
  `CheckIfContinue()` (`testresult.cpp:87-107`) returns `kContinue` instead of
  `kAbortModule`/`kAbortAll`. Affects V1 + V2. Proof it's a bug not intent: the
  no-exceptions (`pthread_exit`) path never sets `exceptionThrown`, so the severity
  survives there — only the default build is broken. Untested: the `abortmod`/`abortall`
  self-tests use **return codes** (`kTR_FailModule`/`kTR_FailAll`, a different, working
  path); the only `t->Fatal()` caller is `test_rust_fatal`, which asserts nothing. Same
  overwrite also replaces the real message with the literal
  `"aborted - better reason required"` and forces `numError = 1` (detail survives only in
  the separate `assertError` vector). Fix: don't route the internal abort-unwind through
  `exceptionThrown`; or only downgrade to `TestFail` when `proxy.Result()` is still `Pass`.
  Add a regression test (`t->Fatal()` ⇒ `ModuleFail` + module stops; `t->Abort()` ⇒
  `AllFail` + run stops), under both `--sequential` and fork.
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **[#2] Filter glob + negation unified.** The
  three divergent matchers (`caseMatch`, `ModuleMatch`, `TestCaseMatch`) are collapsed to the single
  shared `caseMatch` (`strutil.cpp`), now used by listing and by both execution paths. `caseMatch`
  keeps its first-match-wins semantics (the documented `!abortall,!exception,-` idiom depends on it)
  and drops its `goto`. `ModuleMatch` and `TestCaseMatch` (plus their `enum kMatchResult` and the
  `assert(matches.size()==1)`) are deleted; `TestModuleExecutorSequential::Execute` and
  `TestModule::DoExecute` now iterate the (name-sorted) candidates and select each via `caseMatch` -
  exactly what the fork executor already did. Fixes:
  - `--sequential -m 'ipc*'` now runs **all** matches (was: only the first, name-sorted) → seq == fork.
  - `-t '!spl*,-'` no longer `assert`-aborts a debug build on a multi-match negation; it excludes
    every match (verified split+split2 both excluded, exit 0).
  - `-l` (listing) and the sequential run now use the same matcher, so they agree.
  - Regression test rewritten from a scratchpad into `test_execorder_match` (pins glob, negated glob,
    the exclusions-first idiom, plain lists). Filtered-out modules still run as dependencies
    (`ExecuteDependencies` bypasses the filter - verified `-m mdepmodA` pulls B,C,D). Full suite
    fork == seq == 107/13.
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **Fork now forwards `-t` (case filter) to child
  processes.** `SubProcess::Start` (`subprocess.cpp`) built the child args (`-m <module>`, `-G/-D/-c/-C`,
  `--sequential/--subprocess/--ipc-name`) but never forwarded `Config::testcases`, so a forked child ran
  **all** cases of its module regardless of `-t` (`-t split` was honoured in `--sequential` but ignored
  under the default fork). Now rejoins `Config::testcases` into the comma-separated form and passes
  `-t <cases>` when spawning the child (the trivial default `{"-"}` is skipped to keep the child cmdline
  clean). Verified `-t split` / `-t '!spl*,-'` / `-t 'trim,split'` now give identical results in fork and
  `--sequential`; full suite fork == seq == 107/13.
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **[#3] `close(fifofd)` closed the owner's stdin
  (fd 0).** `mkfifo` returns **0 on success** (it makes a filesystem object, not a descriptor), so
  `fifofd` was always 0 and both `close(fifofd)` calls (in `Close()` and the `ConnectTo` error path)
  were `close(0)` - silently closing the owner's stdin. Removed both; `mkfifo`'s return is now just a
  success check, the `fifofd` member is gone (the only real fd is `rwfd`), and the `ConnectTo` error
  path now removes the fifo file it created (previously leaked). Regression test
  `test_ipcfifo_keepstdin` points fd 0 at `/dev/null`, runs an owner `Open()/Close()`, and asserts fd 0
  is still open (then restores real stdin) - verified it fails on the old code and passes on the fix.
  Full suite fork == seq == 108/13.
- **[latent, low priority] Concurrent large IPC frames > PIPE_BUF can interleave on the shared FIFO.**
  All children share one FIFO and run `maxConcurrency` at once; a child flushes its whole
  `IPCResultSummary` as one `write()`, only atomic up to `PIPE_BUF` (4096 Linux / 512 macOS), so two
  large concurrent frames can interleave into a spliced stream. **Probably not an issue in practice —
  result messages are small** — recorded for very large forked suites. The single-writer robustness that
  used to compound this is now fixed: the decoder reads the whole frame before parsing (short/partial
  reads can't desync - `test_ipcframe_chunked_read`), and `IPCFifoUnix::Write` loops on partial writes.
  A true fix for the *concurrent* case would need per-child FIFOs or a length-delimited multiplex.
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **`Abort`/`AllFail` now stops the fork run
  too.** Previously an `AllFail` in one child did **not** stop sibling module processes (fork just
  drained the result and kept dispatching the queue). `TestModuleExecutorFork::Execute` now flags
  `abortAll` when a drained child result yields `CheckIfContinue()==kAbortAll`: it stops launching
  pending modules and kills any in-flight sibling once (`Kill`/`AddIncompleteModule("run aborted")`,
  reusing the timeout path), sparing the reporting child (recorded in `abortingModules`, reaped
  normally). Verified fork `--max-concurrency 1 -m -` matches sequential exactly; auto-concurrency
  kills the running siblings and exits non-zero without hanging. See `todo/result_model.md`.
  - **Follow-up — DONE (same series):** sequential's own `kAbortAll` `break` only escaped the inner
    `matches` loop, so it truly stopped only when one `-m` arg matched many modules (`-m -`/glob); an
    explicit `-m a,b,c` list kept running `b,c`. `TestModuleExecutorSequential::Execute` now sets an
    `abortAll` flag and breaks the outer `-m` arg loop too (verified `-m abortall,strutil` stops after
    `abortall`; a non-abort `-m strutil,timer` still runs both).
  - `Fatal`/`ModuleFail` "stop this module's remaining cases" holds in both modes (unchanged).
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **[IPC hardening] Decoders now check every
  read-error return.** The three `Unmarshal`s in `IPCMessages.cpp` (`IPCResultSummary`,
  `IPCTestResults`, `IPCAssertError`) discarded every `Read*` return and `return true`d, so a
  corrupt/short frame (`Read` returns `-1` on frame overrun) was recorded as a real result with
  zero/garbage fields. They now `return false` on any read failure, so `Process()` fails and the drain
  loop skips the frame (the whole body is consumed first, so the stream stays frame-aligned).
  Regression test `test_ipcframe_truncated` shrinks a frame's declared body so a field overruns and
  asserts `Process()==false` (verified it fails on the old code).
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **[IPC hardening] `WriteStr` length is now 32-bit.**
  A 16-bit length prefix wrapped for strings > 65535 bytes while still writing the full payload →
  intra-frame desync. `WriteStr`/`ReadStr` now use a 32-bit length (safe: all IPC endpoints are the same
  binary). `ReadStr` also bound-checks the length against the remaining frame **before** `resize`, so a
  corrupt/oversized length fails the frame instead of forcing a huge (OOM) allocation. Regression test
  `test_ipcframe_largestring` round-trips a 70000-byte string (verified it fails on the old 16-bit code).
  The assert-count field (`IPCMessages.cpp` `WriteU16`) is left 16-bit - it is an item count, not a
  payload length, and each item is independently bounds-checked, so it cannot desync.
- ✅ RESOLVED (`fix/fatal-abort-result-decision`) — **[IPC hardening] `IPCFifoUnix::Write` loops on
  partial writes.** `write()` to a FIFO is only atomic up to `PIPE_BUF` and can short-count a large
  frame or be interrupted (`EINTR`); since the buffered writer clears its buffer after one `Write`, a
  short write silently truncated the frame. `Write` now loops until all bytes are sent (retrying
  `EINTR`), or returns `-1` on a hard error.
- ✅ RESOLVED (`fix/reporting-hardening`, commits `fdde5e1` + `f7b3509`) — **[reporting] JSON
  `Symbol`/`File` emitted unescaped + lossy `EscapeString` + 256-byte compose buffer.** `Symbol`,
  `File`, `Library`, `Module`, `Case` now all route through `EscapeString` (only `Message` did), so
  a Windows path `C:\src\foo.cpp` no longer breaks the JSON. `EscapeString` rewritten as a proper
  JSON escaper: `"`/`\` escaped, control chars via `\n`/`\t`/`\u00XX`, and bytes >= 0x80 passed
  through (the old signed-`char < 31` test dropped all UTF-8). The three reporting `Write*` methods
  no longer share a `static char[256]` - a `ComposeString()` helper sizes the buffer exactly
  (no mid-string truncation of long lines, reentrant). Tests: `test_jsonreport_escapestring`,
  `test_report_longline` (both proven to fail pre-fix).
- ✅ RESOLVED (`fix/reporting-hardening`, commit `ac7c7de`) — **[robustness] `CREATE_REPORT_STRING`
  truncation + `IsMsgSizeOk` vararg UB.** The grow-loop keyed on `res < 0`, but `vsnprintf` signals
  truncation with a large positive return, so a message > 1024 bytes was silently clamped to 1023.
  Replaced with a measure-once approach (`vsnprintf(NULL,0,...)` → size, cap at
  `responseMsgByteLimit`, single `alloca` + compose). `IsMsgSizeOk`'s `%d`-with-no-arg is fixed
  (passes size + limit, `%u`). Verified end-to-end (3000-byte `t->Error` reaches the reporter whole
  vs 1023 pre-fix); the static trampoline + live-runner requirement means no clean inline unit test.
- ✅ RESOLVED (`fix/reporting-hardening`, commit `7b2052d`) — **[robustness] `ConsumePipes` OOB
  write.** `read()` return is now `ssize_t`, guarded `> 0`; only the actual bytes are forwarded
  (`substr(0, bytes_read)`), so a `-1` no longer writes one byte before the buffer and child output
  is no longer padded to 1024 with an embedded NUL. Test `test_module_procoutput` (`echo hello` →
  exactly `"hello\n"`; proven to fail pre-fix).
- ✅ RESOLVED (`fix/reporting-hardening`, commit `4212a4e`) — **[robustness] Global main/exit
  null-deref.** `ExecuteMain`/`ExecuteMainExit` now `!= nullptr`-guard the `result->Result()` deref
  (mirroring every other `Execute()` call site); a global `test_main`/`test_exit` that can't run no
  longer crashes the runner. Defensive guard, no happy-path change (trigger not reproducible in the
  self-suite, so no dedicated test).
- ✅ RESOLVED (`fix/reporting-hardening`, commits `76b780d` + `aca1ddb`) — **[robustness] empty
  `-m`/`-t` filter + `PopIndent` underflow.** An all-separator/whitespace filter (`-t ,,,`) splits
  to `{}`; the parse layer no longer overwrites the `-` match-all default with it (keeps the filter,
  warns on stderr) - so the run isn't silently emptied. `split()` itself is unchanged (symbol/dep
  parsing rely on its empty-field dropping). `PopIndent`'s underflow branch got its missing
  `return` (it used to `pop_back()` an emptied string - UB / ASan SEGV). Tests
  `test_config_emptyfilter`, `test_report_popindent` (both proven to fail pre-fix).
- ✅ RESOLVED (`fix/reporting-hardening`, commit `142c916`) — **[dead/cosmetic]** removed
  `TestFunc::IsModuleMain` (FIXME'd, dup of `IsGlobalMain`, never called), the redundant
  `GetOrAddModule` re-call (`testrunner.cpp`), and the `fsync()`-on-a-FIFO no-op
  (`IPCFifoUnix::Write`). **Still open:** test-case dependency results computed but never
  `AddResult`'d (`ExecuteDependencies`) - left alone as a dependency-accounting *semantics* decision
  entangled with the `depends` tests, not a mechanical cleanup. Windows no-exceptions
  `TerminateThread(GetCurrentThread(),0)` unwinding gap (`responseproxy.cpp`, non-default build) also
  still open.
