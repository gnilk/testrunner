## Bug: `APPLE` / `LINUX` preprocessor macros only defined for tcov

### Root cause

`-DAPPLE` and `-DLINUX` are added in **tcov's** CMakeLists only
(`src/app/tcov/CMakeLists.txt:29` and `:76`, via `add_definitions`, which is
directory-scoped). `trun` and `trun_utests` are separate `add_subdirectory`
scopes (`CMakeLists.txt:65` vs `:68`) and never receive these definitions.

But shared and test code compiled into `trun` / `trun_utests` branch on
`#ifdef APPLE`. So in those targets the macro is silently **undefined** and the
`#else` (Linux/`.so`) branch is taken on macOS. The compiler-builtin `__APPLE__`
/ `__linux__` would have been defined correctly — note `dynlib_unix.cpp:140`
already uses `#ifdef __linux` the right way.

### Confirmed impact

1. **Directory scanning broken on macOS** — `src/shared/unix/dirscanner_unix.cpp:47`
   ```cpp
   #ifdef APPLE
       extensions.push_back(".dylib");
   #else
       extensions.push_back(".so");      // taken on macOS in the trun build
   #endif
   ```
   This file is compiled into `trun` (where `APPLE` is undefined), so the scanner
   collects `.so` files. The actual artifact on macOS is `.dylib`
   (`cmake-build-debug/lib/libtrun_utests.dylib`). Running `trun` against a
   **directory** — including the default input `"."` — finds nothing on macOS.
   Explicit file paths still work (they bypass the extension filter in
   `ScanLibraries`, `trun.cpp:204-231`), which is why the bug hides.

2. **Three internal tests fail on macOS** — `src/testrunner/tests/test_module_nix.cpp:32,47,65`
   `test_module_scan`, `test_module_symbol`, `test_module_copysym` each do
   `Scan("lib/libtrun_utests.so")` under the `#else` branch. Compiled into
   `trun_utests` (no `APPLE` define), so on macOS they open a non-existent `.so`
   and the leading `TR_ASSERT(t, res)` fails.

### Other sites (currently OK, but on the same fragile mechanism)

These are in the **tcov** build where `APPLE`/`LINUX` *are* defined, so they work
today — but they rely on the inconsistent build-injected macro:
- `src/coverage/Coverage.cpp:144` (`#ifdef APPLE`), `:293` (`#ifdef LINUX`)
- `src/coverage/Config.cpp:36`
- `src/app/tcov/tcov.cpp:85,218,285`

### Fix

Note: do NOT switch to compiler-builtin macros (`__APPLE__` / `__linux__`) —
GCC/Clang differences in those have bitten this project before. Keep
project-controlled defines set explicitly in CMake, so what we depend on is
visible and deterministic.

- Define `APPLE` / `LINUX` for *all* targets that compile the shared code, not
  just tcov. Best place is the shared `trun_common_options` INTERFACE library
  (`cmake/TrunCommonOptions.cmake`), which already branches on `if(APPLE)` /
  `if(UNIX)` and carries `TRUN_HAVE_FORK` — add the platform defines there so
  `trun`, `trun_utests` and `tcov` all inherit them consistently.
- Once carried by common options, drop the now-redundant
  `add_definitions(-DAPPLE)` / `-DLINUX` from `src/app/tcov/CMakeLists.txt`.
- Consider a `TRUN_`-prefixed name (e.g. `TRUN_PLATFORM_APPLE`) to avoid clashing
  with anything else called `APPLE`/`LINUX` — optional.
- For consistency, `dynlib_unix.cpp:140` currently uses the builtin
  `#ifdef __linux` — fold it onto the same project macro while in there.
- Root cause was a regression: these defines were dropped from the non-tcov
  targets during the CMake cleanup earlier this spring (2026).
- After the fix, re-run the internal tests on macOS to confirm the three
  `test_module_*` cases pass and a bare `./trun` (directory scan of `.`) finds
  the `.dylib`.
