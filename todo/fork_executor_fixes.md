## Bugs: Fork module executor (TestModuleExecutorFork)

This is the default module-execution path on macOS/Linux (`moduleExecuteType ==
kParallel` → `forkExecutor`). Several issues cluster here; #1 and #3 in
particular can bite intermittently. All references are
`src/testrunner/moduleexecutors.cpp` and `src/testrunner/subprocess.{h,cpp}`.

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

### Note

Worth doing #1–#4 as one pass — they're entangled (lifetime, threading, output).
