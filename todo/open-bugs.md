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

### testrunner core — audit 2026-07-05

Focused bug sweep of the runner core (coverage out of scope). The suite stays green
because the two worst defects sit on paths the self-suite doesn't exercise. Confirmed
real: #1, #2, #3.

- 🔧 IN PROGRESS (`fix/fatal-abort-result-decision`) — #1 below. The result decision is
  split out of the thread/exception control flow into a pure `TestResult::DeriveResult(...)`
  and the forced abort-unwind is now flagged distinctly from a user C++ exception
  (`TestResponseProxy::SetForciblyTerminated`), so Fatal keeps ModuleFail / Abort keeps
  AllFail. Covered by pure unit tests (`tests/test_resultdecision.cpp`, module
  `resultdecision`) that assert `DeriveResult`/`CheckIfContinue` without terminating.
  The end-to-end abort fixtures stay excluded from the inline run via `run_test_suite.sh`
  (documented there) rather than a subprocess observer (build/maintenance cost not worth it).
- **[#1] `t->Fatal()` / `t->Abort()` no longer stop the module / the run** in the
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
- **[#2] Filter glob + negation is broken; a debug build can `assert`-abort the whole run.**
  Three divergent matchers disagree: `caseMatch` (`strutil.cpp:96`, listing + fork module
  filter), `ModuleMatch` (`moduleexecutors.cpp:76`, sequential module filter), `TestCaseMatch`
  (`testmodule.cpp:88`, case filter).
  - `moduleexecutors.cpp:93-100` — positive module glob `goto leave`s after the first hit, so
    `--sequential -m "mod*"` runs only the first matching module.
  - `moduleexecutors.cpp:83-92` — negative module glob also `goto leave`s, so `-m "!mod*,-"`
    excludes only the first match; the rest run. Silent (only one is ever pushed).
  - `testmodule.cpp:93-101` + `:130` — negative **case** glob has no `goto`, pushes every match,
    then `assert(matches.size()==1)` **aborts the run in a debug build** (`-t "!case*,-"`), and
    in release silently removes only `matches[0]`.
  - Order-sensitivity: execution applies negation by list position (a `!x` after `-` can't
    un-run `x`) while `caseMatch` is first-match-wins → `-l` and the real run can disagree.
  - This is the tracked "glob/negation (!mod)" item (`todo/done/embedded_mcu_step3.md`), now
    with concrete repros. Fix: collapse to one shared matcher with agreed semantics.
- **[#3] `close(fifofd)` closes the parent's stdin (fd 0).** `IPCFifoUnix.cpp:36-42, 49-56,
  70-77`: `fifofd = mkfifo(...)`, but `mkfifo` returns **0 on success**, not a descriptor (the
  real fd is `rwfd`, closed separately). Every `close(fifofd)` in `Close()` / the `ConnectTo`
  error path is `close(0)`. Low real-world impact (runner doesn't read stdin) but a real
  fd-table/double-close bug. Fix: drop the `close(fifofd)` calls.
- **[latent, low priority] Large IPC frames > PIPE_BUF can interleave/tear on the shared FIFO.**
  `IPCFifoUnix.cpp:97-109`, `resultsummary.cpp:156-189`, `moduleexecutors.cpp:235-260`: a child
  flushes its whole `IPCResultSummary` as one `write()`, only atomic up to `PIPE_BUF` (4096
  Linux / 512 macOS); all children share one FIFO and run `maxConcurrency` at once → torn frames
  with no resync. **Probably not an issue in practice — result messages are small** — but recorded
  for large forked suites. Compounding, same area: `IPCDecoder.cpp:128-142` drops the header +
  partial body on a short read (poll-gated non-blocking `Read` returns 0 mid-frame) → desync;
  `IPCBufferedWriter.cpp:24-28` clears the buffer even on a short `write()`.
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
- **[IPC hardening] Decoders ignore all read-error returns** — `IPCMessages.cpp:36-54, 74-105`
  discard every `Read*` return and `return true`, so a corrupt/short frame (`Read` returns `-1`)
  is recorded as a real result with zero-filled fields (`moduleexecutors.cpp:209-214`).
- **[IPC hardening] `WriteStr` truncates the length to 16 bits but writes the full string** —
  `IPCEncoder.h:50-59` + `IPCDecoder.cpp:52-64`: a string > 65535 bytes wraps the length while
  the payload is full → intra-frame desync. Same 16-bit assumption for the assert count
  (`IPCMessages.cpp:122`).
- **[reporting] JSON `Symbol`/`File` emitted unescaped** (`reportjson.cpp:114-115, 166`) — only
  `Message` goes through `EscapeString`; a Windows path `C:\src\foo.cpp` or any `"`/`\` breaks the
  JSON. Related: 256-byte static compose buffer truncates long lines mid-string
  (`reportingbase.cpp:51-75`); `EscapeString` drops non-ASCII (signed `char < 31`) and control
  chars (`reportjson.cpp:172-186`).
- **[robustness] `CREATE_REPORT_STRING` silently truncates messages > 1024 bytes** —
  `responseproxy.cpp:393-410`: grow-loop keys on `res < 0`, but `vsnprintf` signals truncation
  with a large positive return, so it never grows. Plus `IsMsgSizeOk` (`:415`) has a `%d` with no
  argument.
- **[robustness] `ConsumePipes` OOB write** — `process_unix.cpp:157-178`: `buffer[bytes_read]='\0'`
  with `bytes_read` possibly `-1` (e.g. `EINTR` after `poll`) writes one byte before the heap
  buffer; also forwards trailing pad/NUL as captured output.
- **[robustness] Global main/exit null-deref** — `testrunner.cpp:163, 200`: `result->Result()`
  without the `!= nullptr` guard every other call site uses (`TestFunc::Execute` can return
  `nullptr`).
- **[robustness] `split()` drops empty fields** — `strutil.cpp:59-83`: `-t ",,,"` / `-t "  "`
  overwrites the default `{"-"}` with `{}` → nothing runs, no diagnostic. Also `PopIndent`
  (`reportingbase.cpp:38-48`) pops 8× even after the underflow guard empties the string (missing
  `return`).
- **[dead/cosmetic]** `TestFunc::IsModuleMain` (`testfunc.cpp:70-73`, `// FIXME: This is wrong.`,
  duplicates `IsGlobalMain`, never called); redundant `GetOrAddModule` re-call
  (`testrunner.cpp:266`); `fsync()` on a FIFO is a no-op / `EINVAL` (`IPCFifoUnix.cpp:107`);
  test-case dependency results are computed but never `AddResult`'d (`testfunc.cpp:204-211`);
  Windows no-exceptions `TerminateThread(GetCurrentThread(),0)` does no unwinding
  (`responseproxy.cpp:249-253`, non-default build).
