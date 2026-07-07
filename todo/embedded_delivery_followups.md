# Embedded-delivery follow-ons — trunlib_example rewrite / include/testrunner/ header layout
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
