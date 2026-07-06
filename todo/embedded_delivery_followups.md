# Embedded-delivery follow-ons — trunlib rename / trunembedded retirement / header layout

Extracted 2026-07-02 from `todo/done/library_consumption.md` (archived — the two headline
deliverables, trunmcu FetchContent + trunlib find_package, shipped and merged in `9f494fe`).
What remains here is the **deferred, not-greenlit** tail of that delivery work. Capture only.

## TODO  [ -:open  +:in progress  !:done ]
```
+ trunembedded retirement + trunlib rename (COUPLED - do together, one atomic push)
    -> retire the old two-in-one `trunembedded` name/facade now that trunlib covers the
       desktop-embed role; rename `trunlib` to something that reads as "embed the runner in a
       desktop app". Maintainer chose to KEEP `trunlib` for now (2026-07-02) -> deferred.
    ! PARTIAL (2026-07-06): public header `trunembedded.h` -> `trunlib.h` and engine source
       `trunembedded.cpp` -> `trunlib.cpp` done, with a deprecated `trunembedded.h` shim
       (`#pragma message`, forwards to `trunlib.h`, removed in v5.0.0). Both headers installed.
       STILL DEFERRED: renaming the `trunlib` *target* itself, and retiring the `trunembedded`
       *demo app* (src/app/trunembedded dir + target). See section 1.
- include/testrunner/ header layout (couple with the rename)
    -> public headers currently install FLAT into include/ (<trunembedded.h> etc.). Installing
       under include/testrunner/ removes the flat-namespace collision risk. Changes the include
       style to <testrunner/...>, so bundle it with the rename, not as a standalone churn.
- Linux .deb build validation
    -> the EXPORT/config-file package + two-component CPACK split (testrunner / testrunner-dev)
       are authored and the component installs verified on macOS; the actual `.deb` GENERATOR is
       Linux-only and has NOT been run. Build `ninja package` on Linux and confirm the two
       packages lay down the right files.
```

## 1. trunembedded retirement + trunlib rename  (coupled)

Completes the objective in the opening of the archived `todo/done/embedded_impl.md`: *"There are
actually two types of embedded engines … Both use cases should be supported — but not necessarily
by the same engine."* The engines are already split and merged; what's left is naming/facade:
- **Retire** the old two-in-one `trunembedded` once `trunlib` fully covers the desktop-embed role
  — one atomic push (per the archived roadmap's sequencing), keeping the current engine as the
  baseline until then.
- **Rename** `trunlib` to something that reads as "embed the runner in your desktop app"
  (bikeshed at retirement time). The public alias `trun::lib` and `EXPORT_NAME lib` were chosen
  so the *consumer-facing* name is already `trun::lib` regardless of the internal target rename.

Why deferred: the maintainer chose to keep `trunlib` for now — a rename touches the installed
package/target names and every consumer snippet, so it's a deliberate, standalone change.

**Done 2026-07-06 (header + engine-source rename, `refactor/rename-trunembedded-header`):** the
consumer-facing header is now `trunlib.h` (matches `trun::lib`), engine source is `trunlib.cpp`.
`trunembedded.h` remains as a thin deprecated shim (`#pragma message` → forwards to `trunlib.h`;
verified it compiles clean even under `-Werror`, so strict consumers aren't broken) slated for
removal in **v5.0.0**. Both headers ship in the install package. Still open here: the `trunlib`
target rename (kept by choice) and retiring the separate `trunembedded` demo app.

## 2. include/testrunner/ header layout  (couple with the rename)

Public headers install FLAT into `include/` (`trunembedded.h`, the two `ext_testinterface`
headers). On a box with a stale `sudo ninja install` in `/usr/local/include`, the old flat header
shadowed the new one (macOS searches `/usr/local/include` ahead of imported `-isystem` dirs;
worked around in testing with `NO_SYSTEM_FROM_IMPORTED`). Harmless on a clean box, but installing
under `include/testrunner/` removes the collision risk. It changes the include style from
`<trunembedded.h>` to `<testrunner/...>`, so do it together with the rename, not on its own.

## 3. Linux .deb build validation

The consumption machinery is authored and the CMake config/export/install flow is verified on
macOS end-to-end (both `find_package` and FetchContent paths; `--component dev|runtime` installs).
The one thing NOT exercised is the **`.deb` generator itself**, which is Linux-only:
- `ninja package` on Linux with `CPACK_DEB_COMPONENT_INSTALL` should emit two packages:
  `testrunner` (runtime CLI: `trun`/`tcov` + manpage) and `testrunner-dev`
  (`libtrunlib.a` + public headers + `lib/cmake/testrunner/*.cmake`).
- Confirm `TRUN_BUNDLE_DEPS` OFF keeps third-party (fmt/cpptrace/libdwarf) out of both packages,
  and that a downstream `find_package(testrunner)` resolves against **system** fmt/cpptrace.
- `TRUN_BUNDLE_DEPS` ON is the self-contained-prefix case; not necessarily a `.deb` concern.
