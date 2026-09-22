# TODO

## Stem separation (feature/source-separation)

- [x] **Confirm the licence of the htdemucs model weights.** Checked
  2026-09-02, and the answer is bad: they are **not MIT and research-only**.
  facebookresearch/demucs issue #327, answered by the maintainer
  (adefossez, 2022-05-23): "The model weights are not covered by the MIT
  license, and are provided only for scientific purposes." The LICENSE is
  MIT over the code only; htdemucs was trained on MUSDB HQ (research-only
  dataset) + 800 internal Meta songs; the repo is archived (Jan 2025), so
  no relicensing is coming. The Hugging Face re-upload we download from
  labels the weights MIT, which the uploader had no authority to do.
- [ ] **Decide what to do about the research-only weights** before a
  release ships the feature:
  - The audionaut.app mirror redistributed the weights - taken down
    2026-09-02 (removed from the Space; the staged copies in the web
    repo's `docs/models/` and `site/models/` are deleted too). The app's
    fallback URL stays in place but is dormant: a download simply fails
    over to it and reports both hosts if Hugging Face is also unreachable,
    and the hidden `[download-mirror]` test will fail until something is
    served there again.
  - Downloading on first use points users at the weights without
    redistributing them ourselves (what UVR, demucs.cpp's own site and
    others do), but a consumer app steering users to research-only weights
    is still a judgement call - and App Store review may ask.
  - Alternatives with genuinely permissive weights are thin: Spleeter's
    models are MIT (lower quality); Open-Unmix weights are CC-BY-NC.
    Training our own htdemucs on licensed data is the clean long-term fix.
  - Adds to the LICENSE.md commercial-licensing blockers alongside
    Essentia (AGPL) and Ableton Link (GPL).
- [x] Mirror the weights on audionaut.app as a fallback download. Done
  2026-09-02: the app tries
  `https://audionaut.app/models/ggml-model-htdemucs-4s-f16.bin` when
  Hugging Face fails (same SHA-256 check), the file is uploaded to the
  Space and verified end to end - the hidden `[download-mirror]` test
  fetches it through the app's fallback path with the primary disabled.
  A copy is also staged in the web repo's `docs/models/`.
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

- [x] **Agents can transmit feature requests** (2026-09-13): MCP tool
  `request_feature` files a GitHub issue (`enhancement`) with
  title/description/agent context; server instructions steer agents to it
  whenever a task hits a missing verb/option. Transports: relay
  (`AUDIONAUT_FEATURE_REQUEST_URL` → `Tools/feature-request-relay`, a
  Cloudflare Worker holding a GitHub token) → `gh issue create` on
  developer machines → prefilled new-issue URL. Web3Forms (the website's
  contact form) was the first idea and rejects server-side posts on the free
  plan. Smoke test uses a local stand-in relay.
  - [x] **Relay deployed and default** (2026-09-13) at
    `https://audionaut-feature-requests.feature-request-relay.workers.dev`;
    `GET` on it is a health check (`githubStatus` 200 = token works). Token
    = fine-grained `audionaut-cli-request` (Issues read/write on the repo);
    rotate with `pbpaste | npx wrangler secret put GITHUB_TOKEN` from
    `Tools/feature-request-relay` - fine-grained tokens show their value
    only once, right after (re)generation.
  - [x] Mention the tool in the manual's CLI-and-agents chapter (web repo
    PR #3, 2026-09-13).
  - [x] **`report_bug`** (2026-09-14): same transport chain, `kind: "bug"`
    → labels `bug` + `agent-report` (relay reads `LABELS_FEATURE` /
    `LABELS_BUG`); inputs add steps / expected / context.

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

## Playback / time stretch — follow-ups (found 2026-09-21 in PR #105)

- [ ] **End-of-clip re-priming of stretched clips (DSP spike at every
  stretched clip end).** A Stretch-mode voice runs out of input about
  0.1 s before its timeline end: the stretcher pulls its look-ahead
  (`RubberBandStretchBackend::primeInputLength`, ~6,744 source samples at
  ratio 1.31) ahead of the output, so the reader hits EOF and the transport
  reports the stream finished while the clip's audible range still
  intersects the transport. `PlayListScheduler::process` then sees a
  non-playing voice inside an audible clip and re-schedules it every block
  until the remaining duration drops below a block - each restart a seek
  plus an in-block `StretchAudioSource::prime` (8 primes at 512-sample
  blocks, 31 at 128 - measured with a `std::cout` in `prime()`; the
  `[snapshot]` scenario in `LoopStandbyTests.cpp` had to end its bounce
  before the clip end to keep its prime counts clean). Fix candidates: let
  the voice keep rendering the stretcher's buffered tail after the reader's
  EOF (`ClipTransportSource::hasStreamFinished` should account for the
  look-ahead the stretcher still holds), or have the scheduler stop
  restarting a voice that ended less than a look-ahead before the clip's
  end. Re-pitch clips are unaffected.
  - Related, not defects to fix: Rubber Band R3's real-time output count
    wanders by a few samples per second after every prime (direction
    differs per prime; two renders restarted at different points slide
    apart ~17 samples over 0.75 s - compare short windows in tests), and its
    start alignment after a prime is off by 20-35 samples at 128-sample
    blocks (the in-block restart is worse than the standby one).
