# TODO

## Build / tooling

- [ ] **Wire code coverage in properly.** There is currently no coverage
  instrumentation in the repo — `Audionaut/Catch2Tests/CMakeLists.txt` sets no
  `--coverage` flags, and the CI workflows only run `ctest`, so coverage is
  never measured or tracked.

  - Add an `AUDIONAUT_ENABLE_COVERAGE` option to
    `Audionaut/Catch2Tests/CMakeLists.txt`, defaulting to `OFF`. When `ON`,
    apply `--coverage -fprofile-update=atomic` (compile *and* link) to the
    `AudionautTests` target only, so JUCE, Catch2 and the other submodules stay
    uninstrumented and out of the report.
  - Coverage needs `-O0`; make the option force it (or document that it must be
    paired with a `Debug` build) so inlining doesn't distort the line data.
  - Add a coverage job to `.github/workflows/` that configures with the option
    on, runs `ctest`, and reports the number. `gcovr` is the easiest reporter —
    neither `lcov` nor `gcovr` is installed on the Linux dev box today, only
    `gcov` itself, so the job should install its own.
  - Report only `Audionaut/Source/**`; exclude `Submodules/`,
    `JuceLibraryCode/` and `Audionaut/Catch2Tests/Tests/**`.
  - Add the coverage build directory to `.gitignore`. The existing `build`
    entry (line 57) matches that exact name only, so a sibling dir such as
    `build-coverage/` is *not* ignored.

  Baseline measured 2026-08-09 (all 62 test cases passing, Essentia enabled):
  **35.8% lines overall** (4,421/12,358) — Engine **66.8%** (4,209/6,300),
  Interface **3.5%** (212/6,025).
