## Feature: Re-introduce Windows support (V4 milestone)

Windows support was dropped after V1. This document is the analyzed, phased plan to bring it back
under the V4 product line. **"V4" is the product/release milestone — NOT a new interface version.**
The Windows fix changes no interface signature, so the external interface stays **V2**; Windows
simply becomes a first-class V2 platform.

> Status: Core milestone (Phase 0 + 1 + 2) DONE (2026-07-05, branch `feature/windows-phase0`, not yet
> merged to `dev`). Built and verified end-to-end on a real MSVC target (Visual Studio Community
> 2026 / MSVC 19.51, Windows 11) — the hardware blocker noted below is resolved. `trun.exe` builds,
> correctly detects V2, and its own sequential self-test suite runs clean (15/15 documented
> self-fails, no crash, no hang). Phase 3 (fork/IPC parity) and Phase 4 (packaging) remain later/
> not greenlit, per the plan below.
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

Phase 3 — subprocess parity (later)
- Decompose `TestModuleExecutorFork::Execute` into helpers + `CreateModuleIPCServer` seam + `IPCBase::EndpointName()`
- New `src/shared/win32/process_win32.{h,cpp}` (`CreateProcess`); make `Process` impl-member a CMake-selected pimpl
- New `src/shared/win32/IPCPipeWin.{h,cpp}` (`IPCBase` over named pipes); define `TRUN_HAVE_FORK` for Windows

Phase 4 — packaging (later)
- WIN32 CPACK branch (WIX/NSIS)

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
