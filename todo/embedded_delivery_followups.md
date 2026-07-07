# Embedded-delivery follow-ons — trunlib internals (embedded/ -> lib, drop standalone logger),
#   trunlib_example rewrite, include/testrunner/ header layout
# (trunlib rename + Linux .deb validation now DONE — see below)

Extracted 2026-07-02 from `todo/done/library_consumption.md` (archived — the two headline
deliverables, trunmcu FetchContent + trunlib find_package, shipped and merged in `9f494fe`).
What remains here is the **deferred, not-greenlit** tail of that delivery work. Capture only.

## TODO  [ -:open  +:in progress  !:done ]
```
! trunlib rename (header + engine source + target) — DONE (2026-07-06/07)
    -> public header `trunembedded.h` -> `trunlib.h` and engine source `trunembedded.cpp` ->
       `trunlib.cpp`, with a deprecated `trunembedded.h` shim (`#pragma message`, forwards to
       `trunlib.h`, removed in v5.0.0; both headers installed). The `trunlib` *target* rename is
       satisfied by the **consumer-facing name `trun::lib`** — `add_library(trun::lib ALIAS trunlib)`
       + `EXPORT_NAME lib` (app/trun/CMakeLists.txt:183-188) — so `find_package(testrunner)` links
       `trun::lib` regardless of the internal target still being literally `trunlib` (kept
       deliberately; an impl detail consumers never see). See section 1.
- trunembedded demo app -> rewrite as `trunlib_example`  (deferred, NOT now — doc input)
    -> src/app/trunembedded is the old two-in-one demo. Recast it as a clean "how to embed trunlib
       in a desktop app" example, target `trunlib_example`, so it reads as example code rather than a
       legacy facade. Captured now as input for the docs; not scheduled.
- include/testrunner/ header layout (couple with the rename)
    -> public headers currently install FLAT into include/ (<trunembedded.h> etc.). Installing
       under include/testrunner/ removes the flat-namespace collision risk. Changes the include
       style to <testrunner/...>, so bundle it with the rename, not as a standalone churn.
- trunlib: rename src dir `src/testrunner/embedded/` -> `src/testrunner/lib/`  (old MCU-era name)
    -> holdover name from when `trunlib` was the two-in-one "trunembedded" engine. Rename to `lib`
       (matches trunlib / trun::lib). Touches embedsrc + the trunlib & trunembedded include dirs +
       trunlib.cpp's `#include "embedded/dynlib_embedded.h"`. See section 4a.
- trunlib: drop the standalone logger, link gnklog instead  (MCU split removed the reason)
    -> trunlib compiles its own stripped logger (`.../embedded/logger.{cpp,h}`, API-compatible with
       gnklog) rather than linking gnklog. That was only to keep the old trunlib-is-also-MCU engine
       dependency-free; MCU now has its own engine and trunlib already links fmt+cpptrace, so it
       should use gnklog like trun/tcov. See section 4b.
! Linux .deb build validation — DONE (2026-07-07)
    -> the EXPORT/config-file package + two-component CPACK split (testrunner / testrunner-dev)
       are authored, component installs verified on macOS, the Linux `.deb` GENERATOR ran in CI
       (the `v0.0.0-ci-test` release run emitted `testrunner-4.0.0-Linux-runtime.deb` +
       `-Linux-dev.deb`, green), AND the maintainer has now **installed the produced `.deb` on
       Linux — works fine**. Package build + install validated end-to-end.
```

## 1. trunlib rename — DONE; trunembedded demo app -> trunlib_example (deferred)

Completes the objective in the opening of the archived `todo/done/embedded_impl.md`: *"There are
actually two types of embedded engines … Both use cases should be supported — but not necessarily
by the same engine."* The engines are already split and merged; the naming/facade is now resolved:

**Rename — DONE (`refactor/rename-trunembedded-header`, 2026-07-06/07):**
- The consumer-facing header is now `trunlib.h` (matches `trun::lib`), engine source is
  `trunlib.cpp`. `trunembedded.h` remains as a thin deprecated shim (`#pragma message` → forwards
  to `trunlib.h`; verified clean even under `-Werror`) slated for removal in **v5.0.0**. Both
  headers ship in the install package.
- The **`trunlib` target rename is satisfied without renaming the internal target**: the
  consumer-facing name is `trun::lib` via `add_library(trun::lib ALIAS trunlib)` +
  `EXPORT_NAME lib` (app/trun/CMakeLists.txt:183-188). `find_package(testrunner)` links `trun::lib`;
  the literal `trunlib` target name is an implementation detail consumers never see, so it's kept as
  is. (This was always the plan — the alias/EXPORT_NAME were chosen up front for exactly this.)

**Remaining (deferred, NOT now — doc input):** rewrite the `src/app/trunembedded` demo app as
`trunlib_example`. It's the old two-in-one demo; recast it as clean "how to embed trunlib in a
desktop app" example code (target `trunlib_example`) rather than a legacy-named facade. Not
scheduled — captured here as input for the docs.

## 2. include/testrunner/ header layout  (couple with the rename)

Public headers install FLAT into `include/` (`trunembedded.h`, the two `ext_testinterface`
headers). On a box with a stale `sudo ninja install` in `/usr/local/include`, the old flat header
shadowed the new one (macOS searches `/usr/local/include` ahead of imported `-isystem` dirs;
worked around in testing with `NO_SYSTEM_FROM_IMPORTED`). Harmless on a clean box, but installing
under `include/testrunner/` removes the collision risk. It changes the include style from
`<trunembedded.h>` to `<testrunner/...>`, so do it together with the rename, not on its own.

## 3. Linux .deb build validation

**DONE (2026-07-07).** The consumption machinery is authored, the CMake config/export/install flow
verified on macOS end-to-end (both `find_package` and FetchContent paths; `--component dev|runtime`),
the `.deb` **generator** ran in CI (the `v0.0.0-ci-test` release run emitted both
`testrunner-4.0.0-Linux-runtime.deb` and `-Linux-dev.deb`), and the maintainer has now **installed
the produced `.deb` on Linux — works fine**. Original remaining checklist (now satisfied by the
install):
- `ninja package` on Linux with `CPACK_DEB_COMPONENT_INSTALL` should emit two packages:
  `testrunner` (runtime CLI: `trun`/`tcov` + manpage) and `testrunner-dev`
  (`libtrunlib.a` + public headers + `lib/cmake/testrunner/*.cmake`).
- Confirm `TRUN_BUNDLE_DEPS` OFF keeps third-party (fmt/cpptrace/libdwarf) out of both packages,
  and that a downstream `find_package(testrunner)` resolves against **system** fmt/cpptrace.
- `TRUN_BUNDLE_DEPS` ON is the self-contained-prefix case; not necessarily a `.deb` concern.

## 4. trunlib internals — retire the `embedded/` dir name + standalone logger

Two coupled cleanups on the trunlib target, both rooted in the same history: `src/testrunner/embedded/`
and the stripped logger inside it date from when a single `trunlib` served BOTH the desktop-embed role
AND the no-heap/no-dep MCU role. The MCU engine (`trunmcu`, `src/testrunner/mcu/`) is now a **separate
implementation**, so trunlib is purely the desktop-embed engine (threaded; already links fmt + cpptrace)
— the "self-contained, no external logger" constraint no longer applies. Do 4a + 4b together (4b removes
one of the files 4a would move).

### 4a. Rename `src/testrunner/embedded/` -> `src/testrunner/lib/`
The dir holds `dynlib_embedded.{cpp,h}` (the in-process `AddTestCase` dynlib) and `logger.{cpp,h}` (4b).
"embedded" is the old two-in-one name; `lib` matches `trunlib` / `trun::lib`. Touch points:
- `src/app/trun/CMakeLists.txt` — `embedsrc` (dynlib_embedded.{cpp,h}, and logger.cpp until 4b drops it)
  and the trunlib PRIVATE include dir (`target_include_directories(trunlib PRIVATE .../embedded)`).
- `src/app/trunembedded/CMakeLists.txt` — its `.../embedded` include dir.
- `src/testrunner/trunlib.cpp` — `#include "embedded/dynlib_embedded.h"` -> `lib/...`.
- (optional bikeshed) also rename `dynlib_embedded` -> `dynlib_lib`? Separate; not required by the dir move.

### 4b. Drop the standalone logger; link gnklog
`src/testrunner/embedded/logger.{cpp,h}` is a stripped but **API-compatible** reimplementation of gnklog
(`gnilk::Logger` — `Initialize` / `GetLogger` / `SetAllSinkDebugLevel` / `Consume`; `ILogger::Debug/
Info/Warning/Error`; a `LogLevel` enum). trunlib compiles it and puts `src/testrunner/embedded` on its
include path, so trunlib's `#include "logger.h"` resolves HERE; trun/trun_utests/tcov instead link
`gnklog` (extlibs, `cmake/CMakeDeps.cmake`) + include `ext/gnklog/src`.

Migration is mostly CMake: drop `logger.cpp` from `embedsrc`, take the embedded logger off trunlib's
include path, add `gnklog` to trunlib's link + `ext/gnklog/src` to its includes. The **function** surface
matches (only the functions are called — the level enums are an implementation detail), so call sites are
unchanged in substance. gnklog needs `Logger::Initialize()` — **already called** in the shared `Config`
ctor (`config.cpp:65`, which trunlib reaches via `Config::Instance()`), and it auto-adds a console sink
("Console already added"), so no extra sink wiring. The one code touch: trunlib.cpp's `ConfigureLogger`
still uses the old leaky `Logger::kMCError/kMCInfo/kMCDebug` names (the embedded header exposed its
internals); gnklog is more conservative, so match trun.cpp's own `ConfigureLogger` (or just unify the two
near-duplicates).

Wrinkles / considerations:
- **Installed-export dependency.** trunlib is an installed lib (`find_package(testrunner)` -> `trun::lib`).
  Linking gnklog means installed consumers must resolve gnklog — the same situation fmt/cpptrace already
  have, handled by the `$<BUILD_INTERFACE:>` guard + `find_dependency` in `testrunnerConfig.cmake.in`.
  gnklog is a FetchContent source dep (no clean config package today), so it needs the same treatment (or
  a bundle decision). The embedded logger was partly how trunlib kept its installed dep surface minimal;
  dropping it trades that for consistency with trun.
- **This is the root cause of the 2026-07-06 Linux build break.** trunlib resolving `logger.h` to the
  *stripped* logger (fewer transitive STL includes than gnklog's) is why the fork/IPC sources dragged into
  trunlib failed on libstdc++ (`process_unix.cpp` missing `<vector>` etc.; fixed with explicit includes in
  `b00902c`). Switching to gnklog makes trunlib compile the shared code identically to trun and removes
  that whole divergence class (keep the explicit includes regardless — IWYU-correct either way).
