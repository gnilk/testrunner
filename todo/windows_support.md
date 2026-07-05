## Feature: Re-introduce Windows support (V4 milestone)

Windows support was dropped after V1. This document is the analyzed, phased plan to bring it back
under the V4 product line. **"V4" is the product/release milestone — NOT a new interface version.**
The Windows fix changes no interface signature, so the external interface stays **V2**; Windows
simply becomes a first-class V2 platform.

> Status: **All four phases DONE** (2026-07-05, branch `feature/windows-phase0`, not yet merged to
> `dev`). Built and verified end-to-end on a real MSVC target (Visual Studio Community 2026 / MSVC
> 19.51, Windows 11). `trun.exe` builds, correctly detects V2, and its own self-test suite runs clean
> in **both** sequential and parallel/fork mode (15/15 documented self-fails, no crash, no hang,
> identical `90`/`15` counts in both modes) — the fork executor (`process_win32` + `IPCPipeWin`,
> `TRUN_HAVE_FORK` live for Windows) spawns real per-module subprocesses and captures their
> output/results correctly, verified at default and forced `--max-concurrency 1`. Two genuine runtime
> bugs were found and fixed getting here (a pre-existing `AddArgument` truncation bug that corrupted
> every subprocess's command line, and a `PeekNamedPipe`/`ReadFile` visibility race in the post-exit
> stdout drain) — see 3d below. Phase 4 (packaging) is a working NSIS installer (`runtime`/`dev`
> components, "Add to PATH" page) — built, silently installed, and uninstalled clean on this box; see
> Phase 4 below for the one correction found along the way (CPack's PATH page defaults unchecked,
> not checked as first assumed).
> Related: `todo/SESSION-HANDOFF.md`, `README.md` (Building / Windows §158-166),
> `todo/deprecated/signal_handling.md`.

### Work items  ( `-` open / `+` in progress / `!` done )

Phase 0 — CMake unblock (build)  [! DONE 2026-07-05]
- ! Fix wrong win32 source dir in `src/app/trun/CMakeLists.txt:82-83` (`src/testrunner/win32/` → `src/shared/win32/`)
- ! Guard `unixsrcfiles` behind `if(UNIX)`; add parallel `win32srcfiles` list in `cmake/CMakeShared.cmake`
- ! Per-compiler flags: `-O0 -g` / `-fno-exceptions -fno-rtti` → MSVC `/Od /Zi` / `/EHs-c- /GR-`
- ! Fix stale include `src/shared/win32/dynlib_win32.h:4`
- ! Add `WINDOWS` project macro in `cmake/TrunCommonOptions.cmake`; normalize `#elif __linux` → `LINUX` in `trun.cpp`
- ! Force `BUILD_TCOV OFF` on WIN32
- ! Restructure `responseproxy.cpp` `TerminateThreadIfNeeded` guard (exceptions-first)
- ! cpptrace builds clean under MSVC — auto-selected the `dbghelp` backend for unwind/symbols/demangle, no gating needed.

**Additional bugs found only by actually compiling under MSVC (not visible from static analysis):**
- ! `subprocess.cpp`/`subprocess.h` unconditionally `#include`s the UNIX-only `src/shared/unix/process.h`
  (needs `spawn.h`). It only backs `TestModuleExecutorFork` (already `#ifdef TRUN_HAVE_FORK`-guarded
  in `moduleexecutors.cpp`, not yet defined on Windows), so excluded `subprocess.cpp` from the
  Windows source list (`src/app/trun/CMakeLists.txt`) rather than stub a `Process` type early —
  Phase 3 still owns the real `process_win32.{h,cpp}` + `TRUN_HAVE_FORK` wiring.
- ! fmt 12.x requires `/utf-8` under MSVC — added globally (`add_compile_options(/utf-8)` in root
  `CMakeLists.txt`, before `FetchContent`) so it also reaches fmt/gnklog as fetched subprojects.
- ! `<Windows.h>`'s `min`/`max` macros clashed with `std::min`/`std::max` call sites (e.g.
  `test_ipc_framing.cpp`). Added `NOMINMAX`/`WIN32_LEAN_AND_MEAN` as project-wide compile
  definitions on `trun_common_options` (WIN32 branch) — include-order-proof, since `src/shared/dynlib.h`
  and others also pull in `Windows.h`. Also added the same guarded `NOMINMAX` define next to the
  existing `WIN32_LEAN_AND_MEAN` one in `ext_testinterface/testinterface.h`'s own `<Windows.h>`
  block (additive, `_MSC_VER`-agnostic, no signature change — consistent with the frozen-header rule).
- ! `trun_utests` target was missing the `ext_testinterface` include dir that `trun`/`trunlib` already
  had (`src/app/trun/CMakeLists.txt`) — a pre-existing, platform-independent gap that just hadn't
  been hit; fixed alongside.
- ! `std_backport.h`'s `std::erase_if` shim guarded on raw `__cplusplus`, which MSVC pins to `199711L`
  unless `/Zc:__cplusplus` is passed (it isn't, for `trunlib`) — collided with the real
  `std::erase_if` MSVC's `<vector>` already provides in C++20 mode. Fixed the guard to check
  `_MSVC_LANG` first (the MSVC macro that's always accurate), independent of any CMake wiring.
- ! `trun.cpp` used POSIX-only `SIGUSR1`/`raise` (tcov-coverage IPC signal — dead code path on
  Windows since `isCoverageRunning` can never be true without `tcov`; guarded `#ifndef WIN32`) and
  `PATH_MAX`/`getcwd` (added `#include <direct.h>` + `PATH_MAX`→`MAX_PATH` fallback + `getcwd`→`_getcwd`
  shim, next to the existing `STDOUT_FILENO` shim).

**Verified (2026-07-05, MSVC 19.51.36248.0 / VS 2026, clean `cmake-build-win` from scratch):**
all 162 ninja targets build with zero errors — `trun.exe`, `trun_utests.dll`, `trunlib.lib`,
`trunmcu`/`trunmcu_demo`/`trunmcu_demo_v1`, `trunembedded.exe`, `otherexe.exe` (`tcov` correctly
excluded). `trun.exe -h` reports "Windows x64 (64 bit)" and `trun.exe -lx trun_utests.dll` correctly
enumerates all test modules via PE export parsing. As expected (Phase 1 not started), the DLL still
reports version **1.0.0** (V1 fallback — no `TRUN_MAGICAL_IF_VERSION` symbol yet), and a sequential
self-test run (`--sequential -m "!abortall,!exception,-"`) crashes (access violation) once real test
execution starts — expected, this is Phase 1/2 territory, not a Phase 0 regression.

Phase 1 — V2 detection on Windows  [! DONE 2026-07-05]
- ! Add MSVC `__declspec(selectany)` version symbol branch in `testinterface.h` (additive, guarded)
- ! `DynLibWin::Open()` reads `TRUN_MAGICAL_IF_VERSION` via `GetProcAddress`

**One deviation from the plan's exact §2 snippet:** MSVC rejects `__declspec(selectany)` combined
directly with `__declspec(dllexport)` on the same declaration (`error C2496: '__declspec(selectany)'
can only be applied to data items with external linkage`). Fixed by dropping `DLL_EXPORT` from the
declaration and instead forcing the export via a linker pragma:
`#pragma comment(linker, "/export:TRUN_MAGICAL_IF_VERSION,DATA")`, right after the `selectany`
definition. Same effect (symbol lands in the PE export table, readable via `GetProcAddress`), still
header-only/no `TR_IMPL`, still `_MSC_VER`-only so no effect on gcc/clang. The `GetProcAddress`
read in `DynLibWin::Open()` happens on the *final* `LoadLibrary` handle (`hLibrary`), not the earlier
`DONT_RESOLVE_DLL_REFERENCES` enumeration pass — confirms the plan's §2 caveat was correct to flag.

**Verified (2026-07-05):** `trv2_utest.dll` (plain `testinterface.h`) reports **2.0.0**;
`trv1_utest.dll` (`TRUN_USE_V1`, no version symbol) correctly still reports **1.0.0**; the main
`trun_utests.dll` now also reports **2.0.0** (previously fell back to 1.0.0 under Phase 0 alone).
Bonus: the sequential self-test run that crashed (access violation) at the end of Phase 0 now runs
substantially further under V2/cooperative asserts — no longer crashes immediately, executes many
modules including `rust`, exits 0 — though it stops without printing a final pass/fail summary line,
which is Phase 2's job to run down. Full rebuild (`trun`, `trun_utests`, `trunlib`, `trunmcu`,
`trunembedded`) still clean after this change.

Phase 2 — self-test green (sequential)  [! DONE 2026-07-05]
- ! Build `trun_utests.dll`; run `trun.exe trun_utests.dll` sequentially; reproduce known baseline

**Root cause of the missing summary / garbled trailing output (from Phase 1's note):** the root
`CMakeLists.txt` already set `CMAKE_MSVC_RUNTIME_LIBRARY` to the **static** CRT (`/MTd`) for WIN32,
pre-dating this work. `trun.exe` dynamically `LoadLibrary`s test-case DLLs at runtime, so with a
statically-linked CRT, **each binary gets its own independent CRT instance and its own independent
stdout `FILE*` buffer**, both writing to the same underlying OS pipe. Two independently-buffered
streams flush on their own schedule, so combined output interleaves out of chronological order -
in the captured log this showed up as test-body-internal `printf` output (module-main debug prints,
`rust` case output) appearing bunched up *after* the run summary, once the DLL's own CRT buffer
finally flushed. Fixed by switching to the **DLL (shared) CRT**
(`MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`) so every loaded module shares one CRT instance and one
stdout buffer. Verified: the same sequential run's output now ends cleanly right after the failed-
tests list, no trailing content, in one from-scratch full rebuild (all targets clean).

**Tests-Executed count (94 ran / 90 counted) - by design, not a bug:** `TestFunc::ExecuteDependencies`
(`testfunc.cpp`) calls a case-dependency's `Execute()` directly, which prints `=== RUN`/`=== PASS`
(that happens unconditionally inside `Execute()`) but does **not** route through
`ResultSummary::Instance().AddResult()` - that call only happens in `TestModule::DoExecute`/
`ExecuteMain`/`ExecuteExit` for a case's own top-level invocation. So a test that only runs to
satisfy another test's `CaseDepends` (4 of the 94 here) executes and prints normally but isn't
counted a second time in the tally - existing, platform-independent behavior, not something this
work introduced or needs to change.

**Verified (2026-07-05):** sequential run (`--sequential -m "!abortall,!exception,-"`) against
`trun_utests.dll` completes cleanly, exit code 0, correctly-ordered output, ending in a clean
summary: `Tests Executed: 90`, `Tests Failed: 15` - **the failed count matches the documented
baseline (15 intentional self-fails) exactly**. The executed count doesn't literally read "102"
because the Windows build's `trun_utests.dll` has fewer total tests than macOS/Linux (3 UNIX-only
test files - `test_module_nix.cpp`, `test_dirscanner_nix.cpp`, `test_ipcfifo_nix.cpp` - are
correctly excluded by the existing `if(UNIX)` guard, not compiled in on Windows at all) plus the
dependency-execution counting behavior above; every one of the 94 tests that *should* run under
this platform's build did run (diffed the full `-lx` symbol list against every `=== RUN` line -
zero missing). Full rebuild clean across all targets after the CRT change.

Phase 3 — subprocess parity  [! DONE 2026-07-05]
- ! 3a: Decompose `TestModuleExecutorFork::Execute` OS seam into `CreateModuleIPCServer()` factory (pulled `IPCBase::EndpointName()` forward from 3c - needed for the `->` call site to compile)
- ! 3b: Split `Process`/`Process_Unix` into portable `Process` (`src/shared/procspawn.{h,cpp}`) + `ProcessImpl` per OS (`unix/process_unix.{h,cpp}`, **new** `win32/process_win32.{h,cpp}`)
- ! 3c: `IPCPipeWin : IPCBase` over named pipes (+ the reconnect-cycle it needs, see below)
- ! 3d: Define `TRUN_HAVE_FORK` for Windows (final switch) — found and fixed two real runtime bugs getting fork mode to actually work (see below)

**Analyzed and implemented 2026-07-05. Split-vs-guard decision per sub-system, applying §3's own
metric — all four sub-phases done and verified below:**

### 3a. `TestModuleExecutorFork::Execute` — guard, not a split  [! DONE 2026-07-05]

Re-confirmed directly in the current `moduleexecutors.cpp:182-300` (the file has grown since §3 was
first written — a concurrency window + timeout/reap loop were added later — but the analysis still
holds): the *only* OS-specific line remains `gnilk::IPCFifoUnix ipcServer;` (line 186) plus its one
use of `.FifoName()` (line 253). Everything else (`drainResults` lambda, the `pending` module list,
the concurrency window, timeout/reap) already routes through portable types (`SubProcess`,
`gnilk::IPCBinaryDecoder`, `ResultSummary`) — zero large blocks to duplicate.

- Add a tiny factory directly in `moduleexecutors.cpp` (1-line body per branch, well under the
  ~5-line split threshold — a guard, not a new file):
  ```cpp
  static IPCBase::Ref CreateModuleIPCServer() {
  #ifdef WIN32
      return std::make_shared<gnilk::IPCPipeWin>();
  #else
      return std::make_shared<gnilk::IPCFifoUnix>();
  #endif
  }
  ```
  `ipcServer` becomes `IPCBase::Ref` (a pointer) — call sites change `.` → `->`.
- The file-top `#ifndef WIN32 #ifdef TRUN_HAVE_FORK #include "unix/process.h" ... #endif #endif`
  block (lines 32-40) loses its `#ifndef WIN32` wrapper — Windows will define `TRUN_HAVE_FORK` too,
  just with different concrete headers — becoming a plain `#ifdef WIN32` branch selecting
  `win32/IPCPipeWin.h` vs `unix/IPCFifoUnix.h`.
- **Dead code found in passing:** the existing `#ifdef WIN32 #include <process.h> #endif`
  (lines 42-44 — the **CRT** header, not ours) is unused — nothing in the file calls
  `_getpid`/`_spawn*`. Leftover from the stale prior port; delete it during this pass.
- The `drainResults` lambda → `DrainResults(IPCBase &ipc)`, `CollectPendingModules(testModules)`,
  `StartPending(window)`/`ReapAndTimeout(window)` extraction (readability only, zero platform
  coupling) is independent of the win32 work and may land as its own hygiene pass first or be
  folded into this one — maintainer's call, not gating.

**Implemented as analyzed, with one pull-forward from §3c:** `.FifoName()` on a concrete
`IPCFifoUnix` has no equivalent on the `IPCBase::Ref` the factory returns, so the `.` → `->` call-site
change couldn't compile without the interface actually having a name accessor. Added
`virtual const std::string &EndpointName() const` to `IPCBase` (default: empty string) now, and
renamed `IPCFifoUnix::FifoName()` to override it — the 4 call sites (`moduleexecutors.cpp` +
3 in `Coverage.cpp`, tcov-only) all updated. This is the *rename* half of §3c's generalization;
§3c itself still owns creating `IPCPipeWin` as a second override. The dead
`#ifdef WIN32 #include <process.h> #endif` (CRT header) was deleted from `moduleexecutors.cpp` as
planned — this specific file never needed it (confirmed by a clean MSVC build); it turned out a
*different* file did, see 3b's build-fix note below.

**Verified (2026-07-05, MSVC 19.51 / VS 2026, `cmake-build-win`):** `moduleexecutors.cpp` compiles
clean for `trun`/`trun_utests`/`trunlib` — the `#ifdef WIN32` branch referencing `win32/IPCPipeWin.h`
and `gnilk::IPCPipeWin` (neither exists yet, that's 3c) is inert dead code on today's Windows build:
`TRUN_HAVE_FORK` is only defined for `APPLE`/`UNIX` in `TrunCommonOptions.cmake` (3d hasn't flipped
it for `WIN32` yet), so the entire `#ifdef TRUN_HAVE_FORK` block — factory included — is never
preprocessed under `WIN32` until 3d lands. Safe by construction, not by luck.

### 3b. Process spawn (`Process`/`Process_Unix` → `Process`/`ProcessImpl`) — split  [! DONE 2026-07-05]

Read `src/shared/unix/process.h/.cpp` in full. Finding: the portable `Process` wrapper
(`SetCallback`/`AddArgument`/`ExecuteAndWait`/`Kill`/`GetExitStatus`, ~80 lines, zero POSIX
dependency) is currently **colocated** with the POSIX-only `Process_Unix` impl in the same `unix/`
files — so today any file including it (e.g. `subprocess.h`, included widely) drags in `<spawn.h>`
even where nothing POSIX is used. Pre-existing smell, not something Windows introduces, but worth
fixing while touching this code.

- **`src/shared/process.h`/`.cpp`** (new, portable) — `ProcessCallbackInterface`,
  `ProcessCallbackBase`, `ProcessExitStatus`, `Process`. `Process` holds
  `std::unique_ptr<ProcessImpl> impl;` — `ProcessImpl` is only forward-declared here (true pointer
  pimpl, so the portable header pulls in neither `<spawn.h>` nor `<Windows.h>`).
- **`src/shared/unix/process_unix.h/.cpp`** (renamed from today's `unix/process.h/.cpp`) —
  `class ProcessImpl` with the exact same POSIX guts (`posix_spawn`, pipes, `poll`, `waitpid`), just
  renamed from `Process_Unix`.
- **`src/shared/win32/process_win32.h/.cpp`** (new) — `class ProcessImpl` via `CreateProcess` +
  anonymous pipes (`CreatePipe`/`SetHandleInformation` for inheritance), non-blocking read via
  `PeekNamedPipe`+`ReadFile`, `IsFinished` via `GetExitCodeProcess`, `Kill` via `TerminateProcess`.

Zero shared logic between the two impls (POSIX spawn/poll vs. Win32 `CreateProcess`/pipes are
unrelated APIs) — a guard would mean ~150 duplicated lines per branch, so this is the textbook split
case per §3's own metric. `subprocess.h/.cpp` needs exactly one include-path fix
(`"../shared/unix/process.h"` → `"../shared/process.h"`) and nothing else — it already only touches
`Process`'s portable public surface.

**Implemented largely as analyzed, with two corrections found only by actually building under MSVC
(not visible from the design alone) and one CMake-wiring refinement:**

1. **Renamed the new portable file from `process.h`/`.cpp` to `procspawn.h`/`.cpp`.** A file named
   `process.h` sitting in a directory that's already on the include path (`src/shared`, via
   `target_include_directories`) **shadows the CRT's own `<process.h>`** — the header that declares
   `_beginthreadex`, which MSVC's `<thread>` needs whenever a TU constructs a `std::thread` with a
   callable. Found the hard way: `subprocess.cpp` (see point 2) compiled on Windows for the first
   time as a direct result of 3b, its `std::thread thread{...}` construction triggered
   `<thread>`'s internal `_beginthreadex` call, and the angle-bracket lookup resolved to *our*
   `shared/process.h` instead of the toolchain's, producing `error C2039: '_beginthreadex' is not a
   member of the global namespace`. Renaming sidesteps the collision entirely and also retroactively
   protects two *pre-existing* same-shaped landmines that never fired only because nothing had
   exercised that code path on Windows yet: `src/testrunner/platform.h`'s own `WIN32`-guarded
   `#include <process.h>`, and a dead `#include "process.h"` in `src/shared/unix/dynlib_unix.cpp`
   that — now confirmed via grep — referenced nothing in the file it pointed at and was deleted
   outright (a quote-include picking up the sibling `unix/process.h` purely by directory-relative
   lookup, broken by the 3b rename since nothing in `dynlib_unix.cpp` actually used it).
2. **`subprocess.cpp` needs an explicit `#ifdef WIN32 #include <process.h> #endif`.** Once `Process`
   became portable, nothing prevented `subprocess.cpp` (previously excluded from the Windows build
   entirely, see Phase 0) from compiling there too — and per point 1, it's the one file that actually
   needs the *real* CRT `<process.h>` for `_beginthreadex`, since it's the only Windows caller that
   constructs a `std::thread` with a callable (`SubProcess::Start`'s worker thread).
3. **CMake wiring refinement vs. this doc's own original 3d text:** the new portable
   `procspawn.cpp` is **not** added to `sharedsrcfiles` (which the original 3d draft assumed) —
   `sharedsrcfiles` also feeds `trunlib`, and `trunlib` is deliberately the no-fork embedded engine
   (see root `CLAUDE.md`); giving it process-spawn code it will never call is unwanted scope creep.
   Instead added a dedicated `processsrcfiles` list (`cmake/CMakeShared.cmake`) containing
   `procspawn.{h,cpp}`, linked only into `trun`/`trun_utests` (`src/app/trun/CMakeLists.txt`) — the
   same scope `unixsrcfiles`/`win32srcfiles` already have, so `trunlib`'s existing “no fork” boundary
   is unchanged, just now explicit instead of accidental. `process_win32.cpp` joins `win32srcfiles`
   (compiles now, even though nothing calls it until 3d flips `TRUN_HAVE_FORK` for Windows) and
   `process_unix.cpp` replaces `unix/process.cpp` in `unixsrcfiles`, both mechanical renames.
   Additionally flipped the Phase-0 `if(UNIX)` guard around `subprocess.cpp` in
   `src/app/trun/CMakeLists.txt` to unconditional now (rather than waiting for 3d) — the only reason
   it was guarded was `Process` being Unix-only, which 3b resolves directly; it's still unreachable
   at runtime on Windows until `TRUN_HAVE_FORK` is defined there (3d), but it compiles standalone now
   and doing so is what surfaced point 2.

**Verified (2026-07-05, MSVC 19.51 / VS 2026, `cmake-build-win`, full reconfigure + rebuild):**
`trun`, `trun_utests`, `trunlib`, `trunembedded` all build clean (14/14 ninja steps on the affected
targets; `ninja -t` confirms nothing else needed rebuilding) — `process_win32.cpp`, `procspawn.cpp`,
and `subprocess.cpp` all now compile under MSVC for the first time. Ran the sequential self-test
baseline afterward as a regression check (3a/3b touch only dead-until-3d code paths on Windows, so
no behavior change is expected): `trun.exe --sequential -m "!abortall,!exception,-" trun_utests.dll`
→ exit 0, `Tests Executed: 90` / `Tests Failed: 15`, identical to the Phase 2 baseline above.

### 3c. IPC transport (`IPCFifoUnix` → + `IPCPipeWin`) — split, following an existing pattern  [! DONE 2026-07-05]

`IPCBase` (`src/shared/ipc/IPCBase.h`) is already a clean virtual interface (`Open`/`Close`/
`Available`/`Write`/`Read`) — the same pattern `IDynLibrary` already uses elsewhere, so
`IPCPipeWin : public IPCBase` is a zero-invention addition: named pipes via `CreateNamedPipe`+
`ConnectNamedPipe` mirroring `IPCFifoUnix::Open()`/`ConnectTo()`, `PeekNamedPipe` for `Available()`.

One generalization: add `virtual const std::string &EndpointName() const` to `IPCBase` (replacing
`IPCFifoUnix::FifoName()`, not currently part of the base interface). Grepped every call site — only
4 total: `moduleexecutors.cpp` (1, the one that matters for Windows) and `Coverage.cpp` (3, tcov-only,
never compiled on Windows) — small, safe rename across both.

**Implemented as analyzed, plus one structural difference the analysis didn't call out and a second
generalization it needed:**

- **A named pipe instance is a 1:1 server/client connection; a POSIX FIFO path is not.**
  `TestModuleExecutorFork` opens one `IPCBase` up front and feeds it one module-subprocess client
  after another for the whole run — trivial for a FIFO (any process can `open()` the same path any
  number of times), but a Win32 named pipe needs an explicit `DisconnectNamedPipe` +
  `ConnectNamedPipe` cycle between clients. `IPCPipeWin` hides this inside `Open()`/`Available()`
  (`ResetForNextClient()` on broken-pipe detection) so the caller never sees the difference.
  `Open()`'s `ConnectNamedPipe` is posted overlapped/asynchronous (`BeginConnect()`/`PollConnect()`)
  because `Open()` is called before any module subprocess exists yet and must return immediately
  rather than block waiting for a client that isn't spawned.
- **Second real consumer found beyond the one the analysis named:** `resultsummary.cpp`'s
  `SendResultToParentProc()` — the **client** side, run inside every module subprocess to report its
  results back — directly instantiated `gnilk::IPCFifoUnix` too (`#ifndef WIN32`-guarded include).
  Given the OS-conditional treatment alongside `moduleexecutors.cpp`'s server side.
- **`responseproxy.cpp`'s `int_tcov_begincov()`** (tcov coverage-signal IPC, unrelated to the fork
  executor) also unconditionally used `IPCFifoUnix` under `#ifdef TRUN_HAVE_FORK` with no `WIN32`
  guard — would have failed to compile once 3d defines `TRUN_HAVE_FORK` for Windows. Since tcov has
  no Windows backend (`isCoverageRunning` can never be true there — "Out of scope" above), guarded
  the whole body `#if defined(TRUN_HAVE_FORK) && !defined(WIN32)` rather than porting it to
  `IPCPipeWin` for functionality that can never run.

### 3d. `TRUN_HAVE_FORK` for Windows — last step  [! DONE 2026-07-05]

Once `process_win32` + `IPCPipeWin` exist and compile, define `TRUN_HAVE_FORK` in
`cmake/TrunCommonOptions.cmake`'s WIN32 branch alongside `WINDOWS`/`NOMINMAX` — the switch that turns
`TestModuleExecutorFork` on for Windows (the existing `#ifdef TRUN_HAVE_FORK` guard around the whole
class needs no change). This define lives on the `trun_common_options` INTERFACE target, which
`trunlib` deliberately does **not** link — so `trunlib` stays fork-free automatically, no extra
guarding needed. CMake wiring matched 3b's already-corrected shape: `procspawn.{h,cpp}` via the
dedicated `processsrcfiles` list (trun/trun_utests only); `process_unix.cpp`/`IPCFifoUnix.cpp` in the
`if(UNIX)` branch; `process_win32.cpp`/`IPCPipeWin.cpp` in the `elseif(WIN32)` branch; `subprocess.cpp`
already unconditional since 3b (no further change needed there).

**This is where the two real, previously-invisible runtime bugs surfaced — both only reachable once
`TRUN_HAVE_FORK` was actually live and a real parent→child→parent round trip ran on Windows for the
first time:**

1. **`Process::AddArgument(const char *format, ...)` (`procspawn.cpp`) silently truncated the
   Windows command line, corrupting every module subprocess's arguments.** The variadic overload
   builds a fixed `std::string newstr(1024, ' ')`, `vsnprintf`s into it, but never resizes it to the
   actual formatted length — the *string* stays 1024 characters (real text, an embedded early `\0`,
   then ~1000 bytes of leftover space padding). Harmless on POSIX: `posix_spawnp` takes an `argv`
   array of separate `.c_str()` pointers, each read only up to its own embedded `\0` — the trailing
   garbage is simply invisible. Fatal on Windows: `subprocess.cpp` calls
   `AddArgument("--sequential")`/`"--subprocess"`/`"--ipc-name"`/`"-m"` with **string literals**,
   which C++ overload resolution binds to this variadic overload (`const char*` is an exact match,
   beating the user-defined conversion `const char* → std::string` that the other `AddArgument(std::string)`
   overload would need) — and `process_win32.cpp`'s `SpawnAndLoop` concatenates all arguments into
   **one** Win32 command-line string via ordinary (length-based) `std::string` `+=`. The embedded
   `\0` from the *first* affected argument then truncates everything `CreateProcess` and any `%s`-style
   diagnostic ever sees (confirmed via a temporary debug print: the "full" command line read as
   `"...trun.exe" "--sequential` — cut off mid-argument, no closing quote, nothing after it). Net
   effect: every module subprocess ran with silently mangled arguments (wrong/no module filter, wrong
   IPC endpoint name) — it still executed *something* and exited 0, which is why this stayed hidden
   through a routine "does it crash" check. **Fix:** `newstr.resize(std::min<size_t>(res, newstr.size()))`
   after `vsnprintf`, using its own returned length — a one-line, root-cause, both-platforms fix (no
   call-site changes; harmless memory bloat removed on POSIX too).
2. **`process_win32.cpp`'s post-exit stdout/stderr drain had a `PeekNamedPipe`/`ReadFile` visibility
   race.** The live monitoring loop (`while (!IsFinished()) ConsumePipes(...)`) correctly polls
   non-blocking via `PeekNamedPipe` before every `ReadFile` — necessary there, since the child may run
   for a while and blocking would stall the timeout/reap logic. The original post-exit drain reused
   the same Peek-gated `ConsumePipes()` once child exit was detected — but empirically,
   `PeekNamedPipe`'s "bytes available" count could still read 0 for a moment even though the data was
   already fully written and a direct blocking `ReadFile` on the very same handle immediately returned
   it (confirmed by bypassing `ConsumePipes` with a raw `ReadFile` call: it returned real, correct
   captured text — `"=== RUN test_main\n\n=== PASS: test_main..."` — right after the Peek-gated drain
   had already given up reporting nothing left). **Fix:** once the child is confirmed exited there is
   no reason left to avoid blocking — added `DrainAfterExit()`, a plain `while (ReadFile(...) &&
   bytesRead > 0)` loop per stream. Safe unconditionally: the child is gone and the parent's own
   write-end copies are already closed, so the read either returns the last buffered bytes or an
   immediate EOF/broken-pipe — never hangs.

Bug 1 alone was enough to make every subprocess run with corrupted arguments (module filter and IPC
name both silently wrong) while still *looking* healthy (clean exit, no error path hit) — the parallel
run's test counts stayed stuck at the 2 tests belonging to the top-level global module (`test_main`/
`test_exit`) with zero contribution from any of the 25 real modules, and zero captured subprocess
stdout, until this was found and fixed via a temporary debug print of the actual command line.
Bug 2 surfaced immediately afterward as the last remaining "stdout capture is silent" gap once
arguments were correct, and was root-caused via a temporary raw-`ReadFile` bypass of the drain path.

**Verified (2026-07-05, MSVC 19.51 / VS 2026, `cmake-build-win`):** full rebuild clean across all
targets. Sequential baseline unchanged: `Tests Executed: 90` / `Tests Failed: 15`. **Parallel/fork
mode (the actual `TestModuleExecutorFork` path, exercised on Windows for the first time) now matches
the sequential baseline exactly** — same `90`/`15`, same failed-test list, all 25 module subprocesses
spawn via `process_win32`, report through `IPCPipeWin`, and their live stdout (`=== RUN`/`=== PASS`/
`=== FAIL`) is correctly captured and forwarded by the parent — verified at both default concurrency
(`hardware_concurrency()`, ~6.9s) and forced `--max-concurrency 1`.

Phase 4 — packaging  [! DONE 2026-07-05, full smoke test incl. real installer + install/uninstall]
- ! WIN32 CPACK branch — NSIS generator (`CMakeLists.txt`), reusing the existing `runtime`/`dev`
  component split (no manpage/`.dll` handling needed - Windows already skips the Unix manpage
  block, and `trunlib` is `STATIC` everywhere so the existing `ARCHIVE DESTINATION` install rule
  already covers its `.lib` on Windows too, no separate `.dll`/import-lib case to add).

**Chose NSIS over WiX/MSI:** CPack's NSIS generator has `CPACK_NSIS_MODIFY_PATH` built in - an
installer page offering to add the install directory to `PATH` - vs. WiX, which has no equivalent
without hand-authoring an `<Environment>` element via a `CPACK_WIX_PATCH_FILE`. Neither `trun` nor
`tcov` need MSI-specific capabilities (COM registration, GPO-driven enterprise deployment), so the
simpler generator wins. Component packaging mirrors the Linux DEB split: `runtime` (`trun.exe`) and
`dev` (`trunlib.lib` + headers + the `find_package(testrunner)` CMake config), both selectable via
`CPACK_NSIS_COMPONENT_INSTALL`.

**Correction found via actually building and running the installer (not just reading CPack docs):**
`CPACK_NSIS_MODIFY_PATH` does **not** default to checked - there is no CMake variable to change
which radio button is pre-selected. Confirmed by inspecting the generated
`_CPack_Packages/win64/NSIS/NSIS.InstallOptions.ini`: "Do not add testrunner to the system PATH" is
`State=1` (pre-selected), both "Add to PATH" options are `State=0`, hard-coded by CMake's NSIS
generator. A silent install (`/S`) confirmed this empirically: `HKCU\Environment\Path` was untouched
afterward. Getting "optional, checked by default" instead would require a hand-rolled NSIS page
(nsDialogs + custom `.nsh`) that the project would then own and maintain, replacing CPack's built-in
mechanism — discussed with the maintainer, who chose to **keep CPack's built-in page as-is**
(unchecked default, standard/zero-maintenance) over that added complexity.

**Verified (2026-07-05, MSVC 19.51 / VS 2026, NSIS 3.12 installed via `winget install NSIS.NSIS`):**
- `cmake ..` configures cleanly with the new `elseif(WIN32)` CPack branch.
- `cpack -G NSIS` builds a real installer: `testrunner-4.0.0-win64.exe`.
- Silent install (`testrunner-4.0.0-win64.exe /S /D=<dir>`) lays out both components correctly:
  `bin\trun.exe` (runtime), `include\{testinterface.h, testinterface_v1.h, trunembedded.h}`,
  `lib\trunlib.lib`, `lib\cmake\testrunner\{testrunnerConfig,testrunnerConfigVersion,testrunnerTargets}*.cmake`
  (dev) — matches the `install(...)` rules exactly.
- `Uninstall.exe /S` removes the install directory and the
  `HKCU\...\Uninstall\testrunner`/`HKLM\...\Uninstall\testrunner` registry keys cleanly - verified
  both are absent afterward.
- PATH-page default behavior confirmed empirically (see correction above) rather than assumed from
  documentation.

Out of scope
- [!] `tcov` on Windows — needs a full debug-engine backend (SEH / `AddVectoredExceptionHandler` / DbgHelp)
- [!] Retiring the legacy `trunwindows/` VS solution — keep as source-list reference until MSVC-via-CMake is proven

---

## 1. Context & objective

The old note cited three blockers from memory: weak-symbol version detection (MSVC-hostile),
un-mitigated forking, and the threading model. Two facts reshape it:

- **The tree already contains a Windows port** (`src/shared/win32/…`, `TerminateThread` assert
  path, `DLL_EXPORT`, PE symbol enumeration) — but it is **stale**: moved files, wrong CMake paths,
  a loader that predates version-symbol reading, a stale relative include. This is *reconcile +
  finish a diverged port*, not greenfield.
- **The interface stays V2.** The Windows fix changes no signature; Windows becomes a first-class
  V2 platform.

**Objective:** produce `trun.exe` under MSVC that discovers, loads, and runs **V2** test DLLs on
Windows with cooperative (`kTR_xxx`-returning) assert semantics — by letting MSVC emit the version
symbol it currently can't, reconciling the stale win32 code, and following the base/OS
implementation-split convention.

---

## 2. Version detection — the core blocker

`src/testrunner/ext_testinterface/testinterface.h:166-172` emits `TRUN_MAGICAL_IF_VERSION =
GNK_0200` only `#ifndef _MSC_VER`; the comment says *"No clue how to achieve this with MSVC."* The
loader (`src/shared/win32/dynlib_win32.cpp`) never reads it either. Net: every Windows DLL falls
back to the ctor default V1 (`src/shared/dynlib.h:39-41`) → auto-promoted to `kThreadedWithExit`
(`src/testrunner/testrunner.cpp:113-118`) → forced `TerminateThread` on every assert.

**Current versioning scheme (unchanged by this work):** `testinterface.h` *is* the V2 interface
and the single public entry point; plain include = V2. `#define TRUN_USE_V1` redirects it to
`testinterface_v1.h` via the shared include guard. There is no `testinterface_v2.h` and no
`TRUN_USE_V2` — V2 is the default. **No file moves, no new `TRUN_USE_Vx` guards.**

**The fix stays in the current header, guarded for MSVC only:**

```c
#if defined(_MSC_VER)
extern "C" __declspec(selectany) DLL_EXPORT const uint64_t TRUN_MAGICAL_IF_VERSION = STR_TO_VER("GNK_0200");
#elif defined(__cplusplus)
extern "C" const uint64_t TRUN_MAGICAL_IF_VERSION __attribute__((weak)) = STR_TO_VER("GNK_0200");
#else
const uint64_t TRUN_MAGICAL_IF_VERSION __attribute__((weak)) = STR_TO_VER("GNK_0200");
#endif
```

`__declspec(selectany)` is MSVC's weak-data equivalent (header-emitted duplicates collapse to one —
no LNK2005), preserving the header-only / "no `TR_IMPL`" property. `[dcl.link]` treats
`extern "C" const … = …` as `extern`+definition → external linkage, so `selectany` is valid;
`DLL_EXPORT` puts it in the PE export table.

**Why this is in-bounds despite the "frozen header" rule:** the version is locked on *interface
changes* — signatures and struct/macro layout. This branch changes **no signature, no struct, no
macro expansion**, and is `_MSC_VER`-only → gcc/clang preprocess byte-identically → **zero effect
on any current non-Windows consumer.** Windows becomes a first-class **V2** consumer (cooperative
asserts, no forced thread kill). A V1 DLL on Windows (`TRUN_USE_V1`, no symbol) still resolves to
V1 as today.

**Caveat to verify:** reading a *data* export via `GetProcAddress` when the DLL was opened with
`DONT_RESOLVE_DLL_REFERENCES` for enumeration — may need reading on the real `LoadLibrary` pass.
Isolated to `dynlib_win32.cpp`.

### Loader wiring (Phase 1)

- `src/shared/version_t.h` / `src/shared/testlibversion.cpp` already know `GNK_0200`; no new
  constant (interface stays V2).
- `DynLibWin::Open()` reads the symbol, mirroring `src/shared/unix/dynlib_unix.cpp:154`:
  ```cpp
  auto ptrMagic = (version_t*)GetProcAddress(hLibrary, "TRUN_MAGICAL_IF_VERSION");
  if (ptrMagic) { SetVersion(*ptrMagic); }
  ```
- `testrunner.cpp` / `responseproxy.cpp` already treat major==2 as V2 — no change once the version
  is read.

---

## 3. Convention: split vs. guard

Platform variance lives in **per-OS implementation files** (portable base + OS-specific leaf),
selected by CMake. **Metric:** *favour splitting; never duplicate a "large" (>~5-line) block across
OS files* — if a split would force duplicating common logic, extract a shared helper and split only
the small OS leaf.

**Small guards below the split threshold (~90/10) stay as guards** — but structured well:
- `responseproxy.cpp` `TerminateThreadIfNeeded`: **restructure** so `TRUN_HAVE_EXCEPTIONS` is the
  outer axis and `WIN32` only its `#else` branch. Keeps the throw path primary on *all*
  exception-capable platforms (incl. Windows) and demotes `TerminateThread`/`pthread_exit` to the
  no-exceptions fallback — also fixes today's latent "Windows always `TerminateThread` even with
  exceptions" behavior. Stays a guard (a few lines); no OS file.
- `moduleexecutors.cpp` include guards, `timer.{h,cpp}`: trivial — leave as guards.

### The genuine split target: `TestModuleExecutorFork::Execute`

`moduleexecutors.cpp:182-300`, ~120 lines, built in one session. Analysis: the *only* OS-specific
line is `gnilk::IPCFifoUnix ipcServer;` (line 186). Everything else already routes through portable
seams (`IPCBase` virtual API; `SubProcess`→`Process` spawn abstraction). So there is **no large
block to duplicate** — the split is clean. Decompose (readability + isolates the one seam):

- `CreateModuleIPCServer() → IPCBase::Ref` — the **only** OS seam: a 1-line-body factory,
  CMake-selected (`IPCFifoUnix` / `IPCPipeWin`). Add a virtual `EndpointName()` to the IPC base so
  the executor reads the FIFO/pipe name generically (replaces `IPCFifoUnix::FifoName()` at line
  253; generalize `SubProcess::Start`'s param to `endpointName`).
- `CollectPendingModules(testModules)` (lines 211-223), `DrainResults(ipcServer)` (promote the
  lambda 192-209 to a private method), `StartPending(window)` + `ReapAndTimeout(window)` (inner
  loop bodies 248-255, 258-289).
- `Execute()` keeps only the outer while-loop orchestration.

Result: `Execute` + every helper take `IPCBase&`/`SubProcess` — **zero `#ifdef`**; only the 1-line
IPC factory and `process_win32.cpp` are OS files. (The helper-extraction half is pure hygiene and
may land early, independent of the win32 impls.)

---

## 4. What already exists vs. what's missing

| Concern | Portable seam | Windows status |
|---|---|---|
| Dyn-lib load (`dlopen`→`LoadLibraryEx`) | `src/shared/dynlib.h` `IDynLibrary` | **exists** `src/shared/win32/dynlib_win32.{h,cpp}` (PE parse) — stale |
| Symbol enum (`nm`→PE export table) | part of dynlib | **exists** — walks `IMAGE_EXPORT_DIRECTORY`, no `nm` |
| Dir scan | `src/shared/dirscanner.h` | **exists** `src/shared/win32/dirscanner_win32.cpp` |
| Thread kill | `responseproxy.cpp` (small guard — §3) | **exists** `TerminateThread` branch (only needed for V1) |
| Symbol export | `DLL_EXPORT` (4 headers) | **exists** — `__declspec(dllexport)`, manual per `test_*` |
| **Version detect** | `TRUN_MAGICAL_IF_VERSION` | **MISSING on MSVC** — not emitted, not read → §2 |
| Process spawn (`posix_spawnp`→`CreateProcess`) | `Process`/`SubProcess` (`subprocess.{h,cpp}`) | **MISSING** — only `Process_Unix` |
| IPC transport (FIFO→named pipe) | `src/shared/ipc/IPCBase.h` | **MISSING** — only `IPCFifoUnix` |
| Coverage (`tcov`) | LLDB backend | **out of scope** — no Windows debug backend |

For a *working sequential* `trun.exe`, the only genuinely missing pieces are the **§2 version fix +
§5 CMake/path fixes**. Process-spawn + IPC (and the §3 Fork decomposition) are needed only for
subprocess-per-module isolation (later phase) — hence sequential-in-process is the right first
milestone.

---

## 5. Concrete bugs blocking any MSVC build (must fix, independent of scope)

1. **Wrong source directory.** `src/app/trun/CMakeLists.txt:82-83` lists `src/testrunner/win32/…`
   which does not exist; files live in `src/shared/win32/`.
2. **`unixsrcfiles` linked unconditionally.** `src/app/trun/CMakeLists.txt:94,130`,
   `src/app/tcov/CMakeLists.txt:18` add the unix impls (`posix_spawn`/`mkfifo`/`dlfcn`/spawns `nm`)
   on every platform. Guard behind `if(UNIX)`; add a parallel `win32srcfiles` list in
   `cmake/CMakeShared.cmake` (mirror `unixsrcfiles`) — the split mechanism from §3.
3. **GCC/Clang-only flags on MSVC.** `src/app/trun/CMakeLists.txt:107` `-O0 -g`;
   `src/app/trunmcu/CMakeLists.txt:98` `-fno-exceptions -fno-rtti`. Select per compiler in CMake →
   `/Od /Zi`, `/EHs-c- /GR-`.
4. **Stale include.** `src/shared/win32/dynlib_win32.h:4` `#include "../testinterface_internal.h"`
   → non-existent `src/shared/testinterface_internal.h` (real: `src/testrunner/testinterface_internal.h`).
5. **No project platform macro on WIN32.** `cmake/TrunCommonOptions.cmake:16-18` sets MSVC flags
   but (unlike APPLE/LINUX) defines no `WINDOWS` project macro. Add one; normalize the
   `#elif __linux` builtin in `trun.cpp:50,190` to the `LINUX` macro.
6. **Force `BUILD_TCOV OFF` on WIN32** — no Windows debug backend.
7. **cpptrace under MSVC — check it, but it is a nice-to-have, not a blocker.** cpptrace only
   enriches crash *diagnostics* (telling the user where their code faulted); the test engine runs
   fine without it (the Windows assert path uses `TerminateThread`, not exception unwinding). If it
   builds cleanly under MSVC, keep it; if it resists, gate it out on Windows with no functional
   loss. **libdwarf is not a Windows concern at all** — it is cpptrace's ELF/DWARF backend; on
   Windows cpptrace symbolizes via DbgHelp/PDB, so there is nothing to port.

---

## 6. Phased plan

**Core milestone (gets `trun.exe` self-testing on Windows):**

- **Phase 0 — CMake unblock (build).** Fix §5 bugs 1–6; add `win32srcfiles`; per-compiler flags;
  stale include; `WINDOWS` macro; the §3 `responseproxy` guard restructure.
  *Acceptance:* `trun.exe` links under MSVC.
- **Phase 1 — V2 detection on Windows.** §2 guarded symbol in `testinterface.h`; loader read (§2
  loader wiring). *Acceptance:* a DLL built against `testinterface.h` is reported **V2** (not V1)
  by `trun.exe -lx` on Windows.
- **Phase 2 — self-test green (sequential).** Build `trun_utests.dll`; run
  `trun.exe trun_utests.dll` sequentially; reconcile any `nm`/path assumptions. *Acceptance:*
  reproduces the documented baseline (102 run / 15 intentional self-fails with
  `-m !abortall,!exception,-`) in sequential mode on Windows.

**Later phases:**

- **Phase 3 — subprocess parity.** §3 decomposition of `TestModuleExecutorFork::Execute` +
  `CreateModuleIPCServer` seam + `IPCBase::EndpointName()`. New
  `src/shared/win32/process_win32.{h,cpp}` (`CreateProcess`; generalize `Process`'s impl member to
  a CMake-selected pimpl) and `src/shared/win32/IPCPipeWin.{h,cpp}` (`IPCBase` over named pipes).
  Define `TRUN_HAVE_FORK` for Windows. *Acceptance:* per-module subprocess isolation matches Linux.
- **Phase 4 — packaging.** WIN32 CPACK branch (WIX/NSIS) reusing the `runtime`/`dev` components;
  skip the unix manpage; handle `.dll` (RUNTIME) + import `.lib` (ARCHIVE) destinations.

**Out of scope:** `tcov` (needs a full Windows debug-engine backend — SEH /
`AddVectoredExceptionHandler` / DbgHelp); retiring the legacy `trunwindows/` VS solution (keep as
source-list reference until MSVC-via-CMake is proven).

---

## 7. Files this effort will touch

- **Version fix:** `src/testrunner/ext_testinterface/testinterface.h` (additive, `_MSC_VER`-only,
  signature-neutral — §2); `src/shared/win32/dynlib_win32.{h,cpp}` (read symbol; fix stale include).
- **Guard restructure (small):** `src/testrunner/responseproxy.cpp` (exceptions-first — §3).
- **Build wiring:** `src/app/trun/CMakeLists.txt`, `cmake/CMakeShared.cmake`,
  `cmake/TrunCommonOptions.cmake`, `src/app/tcov/CMakeLists.txt`, `src/app/trunmcu/CMakeLists.txt`,
  root `CMakeLists.txt`.
- **(Phase 3) split/refactor:** `src/testrunner/moduleexecutors.cpp` (decompose Fork executor),
  `src/shared/ipc/IPCBase.h` (`EndpointName()`), `src/testrunner/subprocess.{h,cpp}`
  (`endpointName` param), `src/shared/unix/process.{h,cpp}` (impl-member pimpl); **new**
  `src/shared/win32/process_win32.{h,cpp}`, `src/shared/win32/IPCPipeWin.{h,cpp}`.
- **Untouched:** `ext_testinterface/testinterface_v1.h` (V1); the V2 body of `testinterface.h`
  (only the MSVC version-symbol branch is added).

---

## 8. Verification

Each implementation phase carries a concrete build/run acceptance check (Phase 0 links; Phase 1
detects V2; Phase 2 reproduces the sequential pass/fail baseline). When implementation begins,
extend the CI matrix (`.github/workflows/cmake.yml`, currently `ubuntu-latest` only) with
`windows-latest` + MSVC building `trun`/`trun_utests`, then run `trun.exe trun_utests.dll` and
confirm the self-test baseline in sequential mode.

---

## 9. Open question still to settle

**First-milestone reach:** stop at Phase 2 (sequential self-test green) as the shippable proof, or
push through Phase 3 (subprocess+IPC parity) now. The phases above are listed core-vs-later
regardless, so this only sets execution order.
