## Bugs: Fork module executor (TestModuleExecutorFork)

This is the default module-execution path on macOS/Linux (`moduleExecuteType ==
kParallel` → `forkExecutor`). Several issues cluster here; #1 and #3 in
particular can bite intermittently. All references are
`src/testrunner/moduleexecutors.cpp` and `src/testrunner/subprocess.{h,cpp}`.

> **Note (count inflation):** the fork path used to report far more tests than
> sequential (e.g. ~150 vs 93). Two distinct causes:
> - Global `test_main`/`test_exit` counted once per forked child — **FIXED**
>   on `dev` (commit 13102e5): children run the globals for setup but only the
>   parent reports them (`!isSubProcess` guard in `testrunner.cpp`).
> - Module-dependency double-execution across forks — reporting side **FIXED**
>   on branch `fix/resultsummary-dedup`: `ResultSummary` now de-duplicates by
>   test symbol, so fork counts match sequential (95/15). See #5. The redundant
>   *execution* of dependency modules remains (benign perf cost).

### 1. Static accumulation + leaks across libraries

`moduleexecutors.cpp:234`
```cpp
static std::vector<SubProcess *> subProcesses;   // static -> survives between Execute() calls
...
SubProcess *process = new SubProcess();          // never deleted
```
`TestModuleExecutorFork::Execute` runs once per loaded library. When
`DirScanner` finds multiple `.so`/`.dylib` files the `static` vector still holds
the previous library's (finished) subprocesses, so the next run re-iterates and
re-prints them. On top of that:
- `SubProcess` objects are `new`'d and never freed.
- Inside `SubProcess`, `Process *proc` (`subprocess.h:78`, default dtor) leaks too.

- Make the vector a local, owning container (`std::vector<std::unique_ptr<SubProcess>>`)
- Give `Process *proc` an owner (member `unique_ptr`) or delete it in `~SubProcess`

### 2. Output re-dumped on every spin of the wait loop

`moduleexecutors.cpp:284-289`
```cpp
while (true) {
    for (auto &p : subProcesses) {
        ...
        if (p->GetExitStatus() == ProcessExitStatus::kNormal) {
            for (auto &s : p->Strings()) printf(...);   // runs every iteration
        }
    }
    if (bAllFinished) break;
    std::this_thread::yield();
}
```
A module that finishes early has its captured output reprinted on every busy-wait
spin until *all* modules finish. `GetExitStatus()` is also read each iteration,
including before the process has set it.

- Dump each process's output exactly once, after that process finishes
- Only inspect exit status once the process has actually reached kFinished

### 3. Data race on SubProcess state / exitStatus

`subprocess.h:85-86`, written in `subprocess.cpp:19,47-48`, read by the parent
poll loop in `moduleexecutors.cpp`.

`state`, `exitStatus`, `wasProcessExecOk` are plain members written by the worker
thread and read by the parent poll loop with no synchronization -> undefined
behaviour. `exitStatus` has no initializer, so it is also read uninitialized
before the worker sets it.

- Make `state` and `exitStatus` `std::atomic` (or guard with a mutex)
- Initialize `exitStatus` to a defined "unknown/running" value

### 4. SubProcess::thread is never joined

The active wait loop never calls `SubProcess::Wait()` — the old loop that did is
commented out (`moduleexecutors.cpp:301-332`). The `std::thread` member is
therefore leaked; if a `SubProcess` were ever destroyed while the thread is
joinable it would `std::terminate()`. This is currently only avoided *because*
the objects themselves are leaked (see #1) — the two bugs mask each other, so
fixing #1 without this will surface a terminate.

- Join (or detach) each `SubProcess::thread` before the object is destroyed
- Fix together with #1 so the lifetime is correct end-to-end

### 5. Module-dependency modules re-run and re-reported across forks — ✅ RESOLVED (reporting) (fix/resultsummary-dedup)

> Resolved via result de-duplication in `ResultSummary` (keep-first by test
> symbol) rather than changing dependency resolution — deps must stay declared
> in `test_main` (declaring them in the module main would force a rollback/abort
> mid-execution, which was deliberately rejected). The dependency modules are
> still *executed* by multiple children (benign perf cost); only the duplicate
> *reporting* was the bug.


`test_main` declares module dependencies (`test_main.cpp:49-50`):
`mdepmodA -> mdepmodB -> {mdepmodC, mdepmodD}`. In sequential the whole chain
resolves once in a single process (each module executes once via the
already-executed / `IsIdle` check). Under fork each module gets its own child,
and **each child independently resolves its dependency closure**:
- child `mdepmodA` runs A + B + C + D
- child `mdepmodB` runs B + C + D
- child `mdepmodC` runs C, child `mdepmodD` runs D

So the dependency modules are reported by several children -> over-count.
Confirmed: `-m mdepmodA,mdepmodB,mdepmodC,mdepmodD` gives fork=11 vs
sequential=6 (the +5 residual in the full suite).

This is the same shape as the (now fixed) globals problem: a child legitimately
*runs* the dependency module for its own setup, but should only **report** the
module it was actually asked to run (`-m` target), not the ones pulled in as
dependencies.

- Option A (child-side): in subprocess mode only report results for the
  explicitly requested module(s), not dependency modules executed for setup.
- Option B (parent-side): de-duplicate by symbol name while draining child
  summaries (a module's results should be aggregated once).
- Option A is closer to the globals fix and keeps the parent drain simple;
  needs a way to mark "requested vs pulled-in" in the child.

### Note

Worth doing #1–#4 as one pass — they're entangled (lifetime, threading, output).
#5 is independent (reporting semantics, not lifetime) and can be done separately.
