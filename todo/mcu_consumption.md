# Embedded library delivery — easy consumption + trunembedded split

Extracted 2026-07-01 from `todo/done/embedded_mcu_step3.md` (archived) so the
two remaining **open** follow-ons to the MCU engine stay visible after the design + roadmap
docs were closed. Both are the same theme: shipping the embedded engines as clean, well-named,
**easily-consumable** libraries rather than a copy-the-files job.

Context: the MCU engine (`trunmcu`, engine #3) Phase A is done and merged to `dev`. Design:
`todo/done/embedded_mcu_step3.md`; 3-engine roadmap: `todo/done/embedded_impl.md`. Neither
item below is greenlit — **capture only**.

## TODO  [ -:open  +:in progress  !:done ]
```
- Easy consumption: trunmcu as a first-class FetchContent dependency (not a copy job)
- trunembedded split: finish trunmcu (embedded) vs trunlib (desktop-embed); retire old trunembedded
- Consider a clearer name for trunlib (the desktop-embed library)
```

## 1. Easy consumption — make trunmcu a first-class dependency

**Motivation:** the current embedded engine is *not* easy to embed. In practice the maintainer
has "faked" inclusion by cloning the repo and hand-copying/including the required files into
the target project. To be genuinely useful the MCU engine must be **easy to include *and*
use** — a first-class dependency, not a copy job. This is the driving requirement.

**Target ergonomics:** a project pulls in trunmcu via `FetchContent` (or a vendored submodule)
and links one CMake target, no file cherry-picking:
```cmake
FetchContent_Declare(trunmcu GIT_REPOSITORY <...> GIT_TAG <...>)
FetchContent_MakeAvailable(trunmcu)
target_link_libraries(my_tests PRIVATE trun::mcu)
```

**Design constraints / open questions to settle at impl time:**
- The engine is **compiled *for the embedder's target*** with *their* cross-toolchain +
  flags — so a prebuilt host static lib is the wrong artifact to ship. Prefer an
  **`INTERFACE` (or `OBJECT`) library** target that exposes the `src/testrunner/mcu/` sources
  + the frozen `ext_testinterface` include dir, so the parent build compiles them in the
  target's context. (The existing host `trunmcu` static lib stays as the validation build —
  separate, still not installed.)
- Expose the `TRUN_MCU_*` capacities (`MAX_TESTFUNCS`/`MAX_MODULES`/`MSG_BUF_LEN`/
  `SINK_MAX_RETRY`) and `TRUN_USE_V1` as **CMake cache options** on that target so the
  embedder sets them from the parent project (no editing engine headers).
- Provide a namespaced alias (`trun::mcu`) and, for vendored use, keep `add_subdirectory()`
  working too — the same target, two entry paths.
- Decide what (if anything) gets `install(EXPORT)`'d. For MCU the answer is likely "nothing
  installed" (it's compiled-for-target); the FetchContent/`add_subdirectory` path is the whole
  story. Contrast `trunlib`, which *is* an installed desktop lib.

## 2. trunembedded split — trunmcu (embedded) vs trunlib (desktop-embed)

This is **not a new decision** — it completes the objective stated in the very opening of the
(archived) `todo/done/embedded_impl.md`: *"There are actually two types of embedded engines …
Both use cases should be supported — but not necessarily by the same engine."* The old
`trunembedded` conflated those two into one project: (1) genuine embedded/MCU use, and (2)
desktop "trun-as-library" — link the runner into a desktop app so it has no external runner
(reasons: memory model across Linux/macOS/Windows + execution speed). That conflation is what
made it awkward.

Per that objective they are **two separate engines**, which also settles the leftover "is
`trunmcu.h` the new `trunembedded.h`?" packaging note — it's a split, not a rename:
- **`trunmcu`** — engine #1, proper embedded / MCU. Owns the *embedded* facade + the
  FetchContent consumption story in §1.
- **`trunlib`** — engine #2, the desktop-embed library (link into a desktop app, no external
  runner). Owns the *desktop-embed* role; stays an installed desktop lib. Candidate for a
  **clearer name** than `trunlib`.

Follow-on work:
- Retire the old two-in-one `trunembedded` once trunlib fully covers the desktop-embed role —
  one atomic push (per the archived roadmap's sequencing), keeping the current engine as the
  baseline until then.
- Rename `trunlib` to something that reads as "embed the runner in your desktop app" (bikeshed
  at retirement time).
- Keep the two consumption stories distinct: trunmcu = FetchContent/compile-for-target (§1);
  trunlib = installed desktop lib.
