# Consuming the trun libraries — trunmcu (FetchContent) + trunlib (find_package)

> ✅ RESOLVED (feature/library-consumption, merged `9f494fe`, 2026-07-02). Both headline
> deliverables shipped: §1 trunmcu FetchContent (`trun::mcu` source target) and §2 trunlib
> `find_package(testrunner)` (`trun::lib` + config package + two-component CPACK). Verified on
> macOS end-to-end. The **deferred** tail — trunlib rename + trunembedded facade retirement (§3),
> `include/testrunner/` header layout, and the Linux `.deb` **build** — was extracted to the
> active `todo/embedded_delivery_followups.md`. Archived here for the record.

Extracted 2026-07-01 from `todo/done/embedded_mcu_step3.md` (archived) and broadened 2026-07-02
to cover **both** consumable trun libraries. Unifying goal: make the trun test libraries **easy
to pull into a downstream project** instead of the current copy-the-files approach. Both stories
touch the build/packaging of the libs, so they live together.

Two libraries, two delivery mechanisms — because they have genuinely different natures (see the
trunembedded split, §3):
- **`trunmcu`** (engine #3, embedded/MCU) — shipped as **source**, compiled *for the embedder's
  target*; consumed via **`FetchContent`** / `add_subdirectory`. Nothing installed.
- **`trunlib`** (engine #2, desktop-embed) — a normal desktop static lib built with the host
  toolchain; shipped as an **installed `-dev` package** and consumed via **`find_package`**.

Context: all three engines are implemented and merged to `dev`. Design/roadmap:
`todo/done/embedded_impl.md`, `todo/done/embedded_mcu_step3.md`. None of the items below is
greenlit — **capture only**. (MCU Phase B — cross-toolchain / real board — is validated on
hardware *outside* this repo and is out of scope here.)

## TODO  [ -:open  +:in progress  !:done ]
```
! trunmcu consumption: first-class FetchContent dependency (source, compile-for-target)
!   -> trun::mcu INTERFACE source target + SOURCE_SUBDIR; TRUN_MCU_* capacity cache options
!      (V1 via the universal TRUN_USE_V1 - no MCU-specific option)
! trunlib consumption: install as a versioned -dev package (.deb) + find_package(testrunner)
!   -> trun::lib EXPORT + config-file package; two-component CPACK (testrunner / testrunner-dev)
+ trunembedded split: engines already split (merged); CPACK two-package split done.
    Retiring the old `trunembedded` name/facade is coupled to the trunlib rename -> DEFERRED.
- Consider a clearer name for trunlib (the desktop-embed library)   [DEFERRED - keep trunlib for now]
```

**Status (feature/library-consumption, 2026-07-02):** §1 + §2 implemented and verified on macOS
end-to-end (standalone consumers build/link/run; version gating checked). CPACK two-package
split authored + component assignment verified via `--component dev|runtime` installs; the `.deb`
generator itself is Linux-only (author-verified, not run here). Decisions taken: keep `trunlib`
name, two separate packages, trunlib = find_package only, Linux `.deb` + portable CMake config.

## 1. trunmcu — FetchContent (source, compile-for-target)

**Motivation:** the current embedded engine is *not* easy to embed. In practice the maintainer
has "faked" inclusion by cloning the repo and hand-copying/including the required files into the
target project. To be genuinely useful the MCU engine must be **easy to include *and* use** — a
first-class dependency, not a copy job. This is the driving requirement for the whole doc.

**Target ergonomics (IMPLEMENTED):** a project pulls in trunmcu via `FetchContent` (or a vendored
submodule) and links one CMake target, no file cherry-picking. **Use `SOURCE_SUBDIR
src/app/trunmcu`** so only that self-contained subdir is added — the repo root is NOT processed,
so fmt/cpptrace/gnklog are never fetched and the desktop core never builds:
```cmake
include(FetchContent)
# (optional) tune capacities from the parent build - no header edits:
set(TRUN_MCU_MAX_TESTFUNCS 128)   # + _MAX_MODULES / _MSG_BUF_LEN / _SINK_MAX_RETRY  (V1: define TRUN_USE_V1)
FetchContent_Declare(trunmcu
    GIT_REPOSITORY https://github.com/gnilk/testrunner
    GIT_TAG        <tag>
    SOURCE_SUBDIR  src/app/trunmcu)
FetchContent_MakeAvailable(trunmcu)
target_link_libraries(my_tests PRIVATE trun::mcu)
```
`trun::mcu` is an INTERFACE library carrying the `src/testrunner/mcu/*.cpp` sources +
`mcu/`/`ext_testinterface/` include dirs, so the parent build compiles them in ITS toolchain.
The host-validation lib + demos are guarded by `TRUN_IS_HOST_BUILD` (set only in our own root),
so a consumer that pulls the subdir does NOT inherit them. `add_subdirectory(.../src/app/trunmcu)`
is the same target for vendored use. (Verified: standalone consumer builds/runs 4/1;
`-DTRUN_MCU_MAX_TESTFUNCS=2` correctly makes the 3rd/4th registrations fail.)

**Design constraints / open questions to settle at impl time:**
- The engine is **compiled *for the embedder's target*** with *their* cross-toolchain + flags —
  so a prebuilt host static lib is the wrong artifact to ship. Prefer an **`INTERFACE` (or
  `OBJECT`) library** target that exposes the `src/testrunner/mcu/` sources + the frozen
  `ext_testinterface` include dir, so the parent build compiles them in the target's context.
  (The existing host `trunmcu` static lib stays as the validation build — separate, not installed.)
- Expose the `TRUN_MCU_*` capacities (`MAX_TESTFUNCS`/`MAX_MODULES`/`MSG_BUF_LEN`/`SINK_MAX_RETRY`)
  and `TRUN_USE_V1` as **CMake cache options** on that target so the embedder sets them from the
  parent project (no editing engine headers).
- Provide a namespaced alias (`trun::mcu`) and, for vendored use, keep `add_subdirectory()`
  working too — the same target, two entry paths.
- Decide what (if anything) gets `install(EXPORT)`'d. For MCU the answer is likely "nothing
  installed" (it's compiled-for-target); the FetchContent/`add_subdirectory` path is the whole
  story. Contrast `trunlib` (§2), which *is* an installed dev package.

## 2. trunlib — installed `-dev` package + `find_package`

**Nature:** unlike trunmcu, `trunlib` is a *desktop* library built with the host toolchain
(threaded, links fmt + cpptrace, C++20). A prebuilt binary is exactly the right artifact, so its
consumption story is the standard **install → `find_package`** flow, not FetchContent-source.

**Target ergonomics (IMPLEMENTED):** install a versioned dev package, then downstream:
```cmake
find_package(testrunner 3.0 REQUIRED)      # SameMajorVersion compat
target_link_libraries(my_app PRIVATE trun::lib)
```
Verified end-to-end on macOS: `cmake --install --component dev` lays down `lib/libtrunlib.a`,
the 3 public headers, and `lib/cmake/testrunner/{testrunnerConfig,ConfigVersion,Targets}.cmake`;
a downstream `find_package(testrunner 3.0 CONFIG REQUIRED)` yields `trun::lib`, compiles against
the installed `<trunembedded.h>`, links, and runs (4/1). Version gating checked (3.0 ✓; 3.9/4.0/99
rejected).

**IMPORTANT caveat - fmt/cpptrace resolution.** trunlib is a static archive of its OWN objects;
fmt + cpptrace symbols resolve at the CONSUMER's final link, so they must be discoverable. They
are FetchContent deps here and are **guarded out of trunlib's installed export** (via
`$<BUILD_INTERFACE:...>`, because a non-exported build target can't appear in an installed export
set); `testrunnerConfig.cmake` then `find_dependency(fmt)`/`find_dependency(cpptrace)` and
re-attaches them. For that to resolve downstream, fmt + cpptrace configs must be on
`CMAKE_PREFIX_PATH`. How that plays out:
Controlled by the **`TRUN_BUNDLE_DEPS`** option (default **OFF**):
- **OFF (default) - clean install.** fmt is not installed (`FMT_INSTALL OFF`) and cpptrace +
  libdwarf are added `EXCLUDE_FROM_ALL` so their install rules detach from testrunner's. A full
  `cmake --install` / `sudo ninja install` then lays down **only** testrunner's own files
  (trun/tcov, 3 headers, `libtrunlib.a`, the cmake config, manpage) - no third-party headers/libs/
  cmake in e.g. `/usr/local`. Downstream `find_package(testrunner)` then relies on **system**
  fmt/cpptrace (the real distro `-dev` scenario). Verified on macOS: clean tree, install succeeds.
- **ON - self-contained.** fmt + cpptrace + libdwarf co-install with their own configs, so the
  prefix resolves `find_package(testrunner)` -> `find_dependency` out of the box with no system
  deps. Verified on macOS: clean consumer (no hand-provided deps) builds+runs against the prefix.
- `EXCLUDE_FROM_ALL` in `FetchContent_Declare` needs CMake >= 3.28; older CMake falls back to
  bundling (guarded by `CMAKE_VERSION` check). cpptrace still BUILDS on demand (trun links it) -
  EXCLUDE_FROM_ALL only detaches it from the ALL target + install.
- The config skips `find_dependency` if the consumer already defines `fmt::fmt`/`cpptrace::cpptrace`
  (e.g. their own FetchContent), so it composes rather than double-finds.

(Also fixed in passing: the macOS manpage `file(ARCHIVE_CREATE)` used a relative OUTPUT that didn't
land where `install()` looked, failing `cmake --install` on a fresh build - now an absolute path.)

**What to build (explore):**
- **`find_package` support** — export an installed CMake **config-file package** so downstream
  `find_package(testrunner)` just works:
  - `install(TARGETS trunlib EXPORT testrunnerTargets ...)` + `install(EXPORT testrunnerTargets
    NAMESPACE trun:: DESTINATION lib/cmake/testrunner)`.
  - Generate `testrunnerConfig.cmake` + `testrunnerConfigVersion.cmake` via
    `CMakePackageConfigHelpers` (`configure_package_config_file` +
    `write_basic_package_version_file`), installed to `lib/cmake/testrunner/`.
  - Namespaced imported target `trun::lib`; carry public include dirs + transitive deps
    (fmt, cpptrace) as usage requirements so downstream links them automatically. The config
    file must `find_dependency(fmt)` / `find_dependency(cpptrace)` so a consumer resolves them.
- **Versioned `-dev` package** — ship the lib + public headers + the CMake config as a distro
  dev package, e.g. `testrunner-<x.y.z>-dev.deb`. The repo already produces a `.deb` via CPack
  (`ninja package`, see CLAUDE.md) and installs on Linux (`sudo ninja install`); the new work is
  (a) the EXPORT/config-file package above so `find_package` resolves, and (b) shaping a **`-dev`
  component** (headers + static lib + cmake config) distinct from the runtime `trun` CLI.
- **Public headers to install:** the frozen `ext_testinterface` headers (V1 + V2) + the trunlib
  facade header. Keep the frozen contract intact (memory `external-interface-frozen`).
- **Single version source:** drive both the package version and the `find_package` version off
  `project(... VERSION x.y.z)` / `testlibversion` — no second place to bump.

**Open questions:**
- ~~Package layout~~ **DECIDED: two separate packages** (`testrunner` CLI runtime +
  `testrunner-dev` lib/headers/cmake), via `CPACK_DEB_COMPONENT_INSTALL` + per-component names.
- ~~trunlib via FetchContent too?~~ **DECIDED: find_package only** (trunmcu stays source-only).
- ~~macOS/Windows equivalents of the `.deb`~~ **DECIDED: Linux `.deb` + portable CMake config**
  now; macOS/Windows consume via `cmake --install <prefix>` + `find_package` (no new mac/win pkg).
- **NEW follow-up (header namespace):** public headers install FLAT into `include/` (`trunembedded.h`
  etc.), matching the old install. On a machine with a stale `sudo ninja install` in
  `/usr/local/include`, that old header shadowed the new one (macOS searches `/usr/local/include`
  ahead of imported `-isystem` dirs). Harmless on a clean box, but installing under
  `include/testrunner/` would remove the flat-namespace collision risk. Not done (would change the
  include style from `<trunembedded.h>` to `<testrunner/trunembedded.h>`; couple with the rename).

## 3. trunembedded split — trunmcu (embedded) vs trunlib (desktop-embed)

This is **not a new decision** — it completes the objective stated in the very opening of the
(archived) `todo/done/embedded_impl.md`: *"There are actually two types of embedded engines …
Both use cases should be supported — but not necessarily by the same engine."* The old
`trunembedded` conflated those two into one project: (1) genuine embedded/MCU use, and (2)
desktop "trun-as-library" — link the runner into a desktop app so it has no external runner
(reasons: memory model across Linux/macOS/Windows + execution speed). That conflation is what
made it awkward — and it's also why the two consumption stories above differ.

Per that objective they are **two separate engines**:
- **`trunmcu`** — engine #1, proper embedded / MCU. Owns the *embedded* facade + the source /
  FetchContent consumption story (§1).
- **`trunlib`** — engine #2, the desktop-embed library. Owns the *desktop-embed* role + the
  installed `-dev` package / `find_package` story (§2). Candidate for a **clearer name** than
  `trunlib`.

Follow-on work:
- Retire the old two-in-one `trunembedded` once trunlib fully covers the desktop-embed role —
  one atomic push (per the archived roadmap's sequencing), keeping the current engine as the
  baseline until then.
- Rename `trunlib` to something that reads as "embed the runner in your desktop app" (bikeshed
  at retirement time).
- Keep the two consumption stories distinct: trunmcu = source / FetchContent / compile-for-target
  (§1); trunlib = installed `-dev` package / `find_package` (§2).
