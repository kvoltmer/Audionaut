# TODO

## Stem separation (feature/source-separation)

- [ ] **Confirm the licence of the htdemucs model weights before a release
  that ships the feature.** demucs.cpp and the Demucs code are MIT; the
  Demucs README says nothing explicit about the pretrained weights. Check
  facebookresearch/demucs (issues / model card) and decide whether the
  download prompt needs a notice. Also relevant for App Store review notes
  (the app downloads an 84 MB data file on first use).
- [ ] Mirror the weights on audionaut.app as a fallback download: the
  Hugging Face URL is pinned to a revision, but rate limits and outages are
  out of our hands.
- [ ] Windows/Linux pass: verify the `/external:I` / `-isystem` demotion of
  Essentia's Eigen keeps the demucs translation units on the vendored Eigen
  (they need >= 3.4), and check MSVC compile time for the transformer units.
- [ ] Long clips: the separator holds the whole clip and all stems in
  memory, so clips are capped at 10 minutes. Stream longer clips through
  the segmenter in windows.

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

## Agent access — follow-ups (feature/headless-cli, PR #66)

- [x] **Build the MCP wrapper as a follow-up branch.** Done — `Tools/audionaut-mcp/`
  (Node, `@modelcontextprotocol/sdk`): seven tools (`create_project`,
  `import_audio`, `get_project_info`, `analyze`, `auto_edit`, `assemble`,
  `export_audio`) each shell out to `audionaut-cli --json` and relay the
  `{ok, result|error}` envelope; CLI errors surface as tool errors with the
  CLI's own code/message. `npm test` drives the server end-to-end through the
  SDK's stdio client; registration instructions in its README. The fancier
  variant — an MCP server talking to the *running* GUI app over a local
  socket for live-session control — stays deliberately deferred.

- [x] **`split` + `create-region` verbs** (2026-08-27): musical positions
  (`--unit bars|beats|seconds|clocks`, bars/beats 1-based, 96 clocks/bar),
  wrapping `AudioRegionAdapter::splitRegions`/`createRegionsFromSelection`;
  MCP tools `split`/`create_region`.

- [ ] **Commands still needing CLI/MCP exposure** (collected 2026-08-27, in
  rough priority order; engine APIs exist unless noted):

  *Clip / arrangement editing*
  - [x] `remove-clip`, `move-clip`, `place-clip`, `set-region` — done
    2026-08-27 (`Source/Cli/Commands/ClipCommands.cpp` + `RegionCommands.cpp`,
    MCP tools `remove_clip`/`move_clip`/`place_clip`/`set_region`).
    `--delete-region` refuses while other clips still use the region (the
    engine itself does not check). Retrimming a region retrims every clip
    using it — clips have no length of their own; per-clip trim = `split`
    first, then `set-region` on the new region.
  - `duplicate-clip` — GUI's Duplicate (Cmd+D) lives in `MainComponent::duplicate()`;
    engine equivalent needs a look.
  - [x] `clip-gain`, `clip-fades` — done 2026-08-27 (MCP tools
    `clip_gain`/`clip_fades`). Fades are stored as fractions of the region
    length; the CLI converts from bars/beats/seconds/clocks. Also fixed
    `takeOptionValue` swallowing negative values (`--gain -6`).
  - [x] `cleanup-regions` — done 2026-08-27 (MCP tool `cleanup_regions`).

  *Track operations*
  - `create-track` — GUI's Create Audio Track dialog; verb takes `--channels`,
    `--name`.
  - `remove-track` — engine path needs a look.
  - `set-track` — mute/solo/pan/gain per channel, rename, colour (all
    persisted per-channel state).
  - `copy-channels-to-new-track` — GUI command `copyChansToNewTrackId`.

  *Project-level*
  - `set-tempo` — `TempoProvider::setTempo` + save.
  - `set-master-gain`.
  - [x] region-addressed export — done 2026-08-27: `export --region NAME
    [--track N]` bounces one region, always dry (no clip gains/fades —
    dynamics belong to timeline placements, the region is raw material).

  Deliberately excluded: transport/view/window commands (play, loop, zoom,
  snap, fullscreen, browsers — meaningless headless) and undo (history is not
  persisted across CLI invocations; every verb saves immediately).

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

## Clip crossfades — follow-ups (feature/clip-xfades)

The plumbing is complete: fades have adjustable start/end offsets, negative
offsets extend the audible material outside the region window (UI ghost
waveform + lane overlay ramps), and playback/export render them (equal-power
sqrt curves, overlapping clips sum as independent voices). What is missing is
the one-gesture crossfade UX on top:

- [ ] **Auto-create symmetric fades when clips overlap.** When a clip is
  dragged/trimmed so it overlaps its neighbour on the same playlist, create
  the crossfade automatically: the left clip gets a fade-out over the overlap
  (`setFadeOut`), the right clip a fade-in (`setFadeIn`), each clamped to the
  overlap range. Detection hooks exist —
  `PlayListContainer::itemIntersectingRange` / `itemsAtAbsoluteRange` — and
  the natural commit points are `DraggerControl::mouseUp` (after
  `validateData`) and `AudioTrack::dropPlayListItem`. Decide: recompute on
  every overlap change vs. only on first contact (protect user-edited fades),
  and whether a preference toggles the behavior.

- [ ] **Drag one handle across a cut to shape both clips at once.** At a butt
  joint (clip A ends where clip B starts, typically after a split), dragging
  A's fade-out-end handle past the cut should simultaneously set B's
  fade-in-start to the mirrored negative offset (and vice versa), producing a
  symmetric crossfade without touching clip positions. The handle wiring
  lives in `PlayListItemComponent::setPlayListItem` (onValueChange lambdas);
  the neighbour is found via the playlist container (items are kept sorted by
  position). Needs a paired undo action (one gesture = one transaction across
  two items) and the lane `ClipFadeOverlay` already draws both sides.
