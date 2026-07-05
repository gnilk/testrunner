# Result model — return codes vs `ITesting` callbacks, and the mapping to `kTestResult`

Reference/design map for how a test signals success/failure, how those signals reconcile into
a single `kTestResult`, and how that differs between `--sequential` and forked execution.

TODO markers (open items at the bottom): `-` open, `+` in progress, `!` done.

## Objective / premise

**Return codes are the primary channel.** They are how the maintainer writes all tests, and they
are universally supported — any language, any FFI, any toolchain can return an int.

The `ITesting` **function callbacks** (`Error`/`Fatal`/`Abort`/`AssertError`) are a **secondary**
channel, added later to signal the *same* thing more ergonomically:
- calling interface methods feels more "C++" than wrapping everything in `TR_*` macros, and
- a struct of function pointers is a decently supported **language-binding** primitive (Rust, etc.),
  whereas C macros don't cross a language boundary.

Because the callback channel mirrors an already-working return-code channel, it has had **limited
attention** — which is why the two don't line up cleanly. This doc records that seam so it can be
tidied (and is the reference for the fork-`AllFail` alignment work).

### History — the V1→V2 seam
- **V1**: a failing assert was fully handled by the **function call** `t->AssertError(exp,file,line)`
  (return type `void`). The macro just called it; there was **no return**, so the runner had to
  **force-terminate** the thread to stop a failing test mid-body.
- **V2**: changed to **call + return code** — `AssertError` returns `kTRContinueMode` and the macro
  does `if (... == kTRLeave) return kTR_Fail;`. The assert now cooperates with the return-code
  channel instead of relying on forced termination.

That single change is where the two channels started to blend, and it's the root of most of the
"flux" below. (Both `ext_testinterface/testinterface.h` (V2) and `testinterface_v1.h` (V1) are a
**frozen contract** — this doc describes them, it does not propose changing them.)

## The two channels

- **Channel A — return code (`kTR_*`)**: one value, returned **once** at end of the body (and from
  V2 pre/post hooks). `kTR_Pass 0x00`, `kTR_Fail 0x10`, `kTR_FailModule 0x20`, `kTR_FailAll 0x30`.
- **Channel B — `ITesting` callbacks**: `Error`/`Fatal`/`Abort`/`AssertError`, **called** 0..n times
  mid-body, with side effects (and sometimes forced termination).

A single case may use both (e.g. call `Error()` then `return kTR_Pass`); the runner reconciles them.

### Intended symmetry (from the header comments)
| Intended severity | Channel A (return) | Channel B (callback) | Header comment |
|---|---|---|---|
| test fails, go to next | `kTR_Fail` | `Error()`, `AssertError()` | "Current test, proceed to next" |
| stop this module | `kTR_FailModule` | `Fatal()` | "stop library and proceed to next" |
| stop the whole run | `kTR_FailAll` | `Abort()` | "stop execution" |

So `Error≈kTR_Fail`, `Fatal≈kTR_FailModule`, `Abort≈kTR_FailAll` — same 3-level ladder, different
mechanics.

## `kTestResult` (the internal outcome)
`Pass 0`, `TestFail 1`, `ModuleFail 2`, `AllFail 3`, `NotExecuted 4`, `InvalidReturnCode 5`.
Values 0–3 are **severity-ordered**, which the proxy relies on (`if (testResult < X) testResult = X`).

## Channel B mechanics (`responseproxy.cpp`)
Each callback (a) raises the proxy's recorded severity monotonically, (b) maybe force-terminates:

| Callback | raises `proxy.testResult` to | adds to `assertError` | force-terminates? |
|---|---|---|---|
| `Error` | `TestFail` | — (**no detail added**) | **only in forced mode** (V1 / `--allow-thread-exit`) |
| `AssertError` | `TestFail` | `kAssert_Error` | `continueOnAssert`→no. Else **V2: cooperative** (returns `kTRLeave`; macro `return kTR_Fail`); **V1: forced** (macro has no return) |
| `Fatal` | `ModuleFail` | `kAssert_Fatal` | **always** |
| `Abort` | `AllFail` | `kAssert_Abort` | **always** |

Forced termination (both run modes use the threaded case executor): throw `TestAbortException`
(exceptions build) → caught → `SetForciblyTerminated`; or `pthread_exit` (no-exceptions build). V2
assert does **not** terminate — it returns `kTRLeave` and the `TR_ASSERT` macro returns `kTR_Fail`.

## Reconciliation (`TestFunc::CreateTestResult` → `TestResult::DeriveResult`)
`DeriveResult(termination, proxyResult, returnCode, errors, asserts, discard)`, where `proxyResult`
= the max severity Channel B raised (or `Pass`), and `termination` = how the body ended:

| termination | when | result |
|---|---|---|
| **Returned** | body returned a value | map `returnCode`: `Pass`→(errors/asserts? `TestFail` : `Pass`); `Fail`→`TestFail`; `FailModule`→`ModuleFail`; `FailAll`→`AllFail`; unknown→(flagged? keep `proxyResult`/`TestFail` : `InvalidReturnCode`). `discard`→`proxyResult`. |
| **ForciblyTerminated** | `Fatal`/`Abort`/V1-assert/(`Error` in forced mode) | `proxyResult` (already the right severity) |
| **UserException** | a C++ exception escaped the body | `TestFail`, unless `proxyResult` was already `ModuleFail`/`AllFail` |

**Channel A flows through the `Returned` branch; Channel B flows through `proxyResult`** — surfaced
directly when the body was force-terminated, or blended with the return code when the body returned.
`CheckIfContinue` then maps the final `kTestResult` to a runner action: `ModuleFail`→`kAbortModule`
(if `skipOnModuleFail`), `AllFail`→`kAbortAll` (if `stopOnAllFail`), else `kContinue`.

## Run modes: mapping is mode-independent; only control flow differs

`--sequential` does **not** change how a case runs — it sets `moduleExecuteType = kSequential` only;
`testExecutionType` stays `kThreaded`, so cases run in a thread in **both** modes. "Sequential"
means *modules run in one process*, not *cases run inline*. So Channels A/B → `kTestResult` is
**identical** in both modes. What differs is how that result drives cross-module control flow:

| action | `--sequential` (1 process) | fork (subprocess per module) |
|---|---|---|
| within-module (`DoExecute` case loop) | stop remaining cases, run module exit | **same** — child runs its one module via the same `DoExecute` |
| `kAbortModule` (`Fatal`) | move to next module | child ends; parent starts next — **same effect** |
| `kAbortAll` (`Abort`) | module executor **breaks** the loop → no further modules | **ignored** — parent's fork loop records the drained `AllFail` but never acts on it |

The child is launched `trun --sequential --subprocess -m <module> <lib>` and forwards `-c`/`-C`, so
its own `CheckIfContinue` matches the parent — but a child owns one module, so `kAbortAll` there just
ends the child; propagating "stop everything" to the parent was never wired. The child also flushes
its `IPCResultSummary` only **once, at end of module**, so any future propagation is end-of-module
granular (a killed-mid-run sibling can't be un-run). See open item below.

## Open items

- `-` **fork `AllFail` propagation** — align fork with sequential: when a drained child result yields
  `CheckIfContinue()==kAbortAll`, stop launching further modules and `Kill()` the in-flight ones
  (reuse the timeout path's `Kill`/`AddIncompleteModule`/drop-output). Transport already carries the
  result code to the parent (`IPCResultSummary`→`drainResults`→`IPCTestResults::Result()`); the gap
  is only that the parent doesn't act on it. Granularity is end-of-module (see above); a tighter
  "immediate `Abort` → parent kill" would need an early streaming IPC signal (bigger change).
- `-` **`Error` adds no `assertError` detail** — it's the only failing callback that records no
  message into the assert list (Fatal/Abort/AssertError all do), so an `Error`-only failure has no
  detail for reporters keyed off `assertError`. Decide whether to align it.
- `-` **Dead sequential case-executor + naming** — `TestFuncExecutorSequential` is unreachable
  (no flag sets `testExecutionType = kSequential`; the factory always returns the threaded one).
  Either remove it or clarify that `--sequential` is module-scope, not case-scope.
- `-` **Codify A-vs-B authority** — when the return code and the callbacks disagree, the current rule
  is an implicit monotonic-max blend (a flagged `Error` overrides `kTR_Pass`; `discard` silences the
  return code entirely). Worth writing down as an intended contract rather than emergent behavior.
- `-` **V2 assert depth limit** (documented limitation) — V2's cooperative `return kTR_Fail` only
  escapes the direct test body; an assert in a helper returns to that helper, not out of the test.
  `Fatal`/`Abort` (terminate) stop at any depth. Frozen-header behavior; recorded so it's not
  mistaken for a bug.
- `-` **V1/V2 `AssertError` divergence** (frozen) — different arg order (`exp,file,line` vs
  `line,file,exp`) and return (`void` vs `kTRContinueMode`). Cannot change the headers; documented so
  the internal trampolines' swizzling in `responseproxy.cpp` reads as intentional.

## Related
- `todo/open-bugs.md` — "testrunner core — audit 2026-07-05" (the `Fatal`/`Abort` downgrade fix that
  produced the current `DeriveResult`, and the by-design `AllFail` fork-scope note).
- Memory: `external-interface-frozen`, `v2-assert-return-depth-limit`.
