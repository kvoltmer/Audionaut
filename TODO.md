# TODO

## Build / tooling

- [x] **Wire code coverage in properly.** Done — `AUDIONAUT_ENABLE_COVERAGE`
  (default `OFF`) in `Audionaut/Catch2Tests/CMakeLists.txt`, plus a
  `Code coverage` workflow in `.github/workflows/coverage.yml`.

  Locally:

  ```
  pipx install gcovr                       # once; gcovr is optional but gives the report
  cmake -B build-coverage -S Audionaut/Catch2Tests \
        -DCMAKE_BUILD_TYPE=Debug -DAUDIONAUT_ENABLE_COVERAGE=ON
  cmake --build build-coverage -j8 --target coverage
  ```

  Writes `build-coverage/coverage.xml` and a browsable
  `build-coverage/coverage-html/index.html`. `Debug` is required, not
  incidental: the coverage flags are attached to the target, and JUCE's
  recommended config flags inject `-O3` for other build types through
  INTERFACE options that land later on the command line.

  Baseline measured 2026-08-09 (all 62 test cases passing, Essentia enabled):
  **35.8% lines overall** (4,421/12,358) — Engine **66.8%** (4,209/6,300),
  Interface **3.5%** (212/6,025).

## Follow-ups surfaced by the coverage work

- [ ] **The test suite writes into its own fixture directory.** Running
  `AudionautTests` leaves `_export_TRK-18.wav` (17MB), `dc-offset.wav` and
  `slow-saw.wav` behind in
  `Audionaut/Catch2Tests/TestFiles/Sessions/move-channels.audium/Media/Audio/`,
  next to the tracked `4-bars-120-sr.wav` fixture. Outputs should go to a temp
  dir — some tests already do this correctly (they create under `/tmp`).

- [ ] **Dead globs in `Audionaut/Catch2Tests/CMakeLists.txt`.**
  `Interface/Dialogs/*.cpp` and
  `Interface/Components/MiddlePanel/EditView/*.cpp` match nothing — both
  directories are gone since the Auto Edit dialog moved into the arrangement
  (commit `69aec9f`).

- [ ] **`Engine/Selection/SelectionManager.cpp` has 0% coverage** (141 lines).
  Pure state logic with no external dependency — the most testable uncovered
  thing in the engine.

- [ ] **`Engine/Group` (56%) and `Engine/Region` (53%)** carry ~1,100
  uncovered lines between them and are the multitrack model core.
