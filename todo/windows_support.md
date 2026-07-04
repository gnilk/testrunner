## Feature: Re-introduce Windows support (V4 milestone)

Windows support was dropped after V1. This document is the analyzed, phased plan to bring it back
under the V4 product line. **"V4" is the product/release milestone — NOT a new interface version.**
The Windows fix changes no interface signature, so the external interface stays **V2**; Windows
simply becomes a first-class V2 platform.

> Status: planned (2026-07-03). No Windows code written yet. Pick up from Phase 0.
> Related: `todo/SESSION-HANDOFF.md`, `README.md` (Building / Windows §158-166),
> `todo/deprecated/signal_handling.md`.

### Work items  ( `-` open / `+` in progress / `!` done )

Phase 0 — CMake unblock (build)
- Fix wrong win32 source dir in `src/app/trun/CMakeLists.txt:82-83` (`src/testrunner/win32/` → `src/shared/win32/`)
- Guard `unixsrcfiles` behind `if(UNIX)`; add parallel `win32srcfiles` list in `cmake/CMakeShared.cmake`
- Per-compiler flags: `-O0 -g` / `-fno-exceptions -fno-rtti` → MSVC `/Od /Zi` / `/EHs-c- /GR-`
- Fix stale include `src/shared/win32/dynlib_win32.h:4`
- Add `WINDOWS` project macro in `cmake/TrunCommonOptions.cmake`; normalize `#elif __linux` → `LINUX` in `trun.cpp`
- Force `BUILD_TCOV OFF` on WIN32
- Restructure `responseproxy.cpp` `TerminateThreadIfNeeded` guard (exceptions-first)
- Check whether cpptrace builds under MSVC; gate it out if it resists (nice-to-have crash-location diagnostics only, not a blocker). libdwarf is not a Windows concern (cpptrace's ELF/DWARF backend; Windows uses DbgHelp/PDB)

Phase 1 — V2 detection on Windows
- Add MSVC `__declspec(selectany)` version symbol branch in `testinterface.h` (additive, guarded)
- `DynLibWin::Open()` reads `TRUN_MAGICAL_IF_VERSION` via `GetProcAddress`

Phase 2 — self-test green (sequential)
- Build `trun_utests.dll`; run `trun.exe trun_utests.dll` sequentially; reproduce known baseline

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
