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
    Ableton Link (GPL).
- [x] Mirror the weights on audionaut.app as a fallback download. Done
  2026-09-02: the app tries
  `https://audionaut.app/models/ggml-model-htdemucs-4s-f16.bin` when
  Hugging Face fails (same SHA-256 check), the file is uploaded to the
  Space and verified end to end - the hidden `[download-mirror]` test
  fetches it through the app's fallback path with the primary disabled.
  A copy is also staged in the web repo's `docs/models/`.
- [ ] Windows/Linux pass: check MSVC compile time for the transformer units.
  (The Eigen clash this item also covered went with Essentia: the demucs
  translation units are the only ones that see an Eigen now.)
- [ ] Long clips: the separator holds the whole clip and all stems in
  memory, so clips are capped at 10 minutes. Stream longer clips through
  the segmenter in windows.

## Build / tooling

- [x] **Wire code coverage in properly.** Done — `AUDIONAUT_ENABLE_COVERAGE`
  (default `OFF`) in `Audionaut/Catch2Tests/CMakeLists.txt`, plus a
  `Code coverage` workflow in `.github/workflows/coverage.yml`, run by hand
  since 2026-10-22 (the Linux tests on every push/PR are `linux-tests.yml`).

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

  Baseline measured 2026-08-09 (all 62 test cases passing, Essentia still linked then):
  **35.8% lines overall** (4,421/12,358) — Engine **66.8%** (4,209/6,300),
  Interface **3.5%** (212/6,025).

- [ ] **One static library for the CMake tests and the CLI.** With
  `AUDIONAUT_BUILD_CLI=ON` (the default) `Audionaut/Catch2Tests/CMakeLists.txt`
  compiles every engine source twice, once for `AudionautTests` and once for
  `audionaut-cli`. Build the shared sources once as a static library and link
  both targets against it. Per push, CI would go from 8 full compiles to 5:
  macOS 2→1, Linux tests 2→1, Windows 3→2, Makefile stays at 1. CMake only:
  the app keeps building from the Projucer projects, and the in-app CLI/MCP
  verbs are compiled into the app as before. Both targets already share
  `AUDIONAUT_HEADLESS`. What differs is `CURRENT_SOURCE_DIR` (tests only) and
  `GA4_MEASUREMENT_ID` (CLI only, kept out of the tests on purpose so they can
  never create an analytics destination). Those have to stay on the
  executables and must not move into the library.

## Agent access — follow-ups (feature/headless-cli, PR #66)

- [x] **Build the MCP wrapper as a follow-up branch.** Done — `Tools/audionaut-mcp/`
  (Node, `@modelcontextprotocol/sdk`): seven tools (`create_project`,
  `import_audio`, `get_project_info`, `analyze`, `auto_edit`, `assemble`,
  `export_audio`) each shell out to `audionaut-cli --json` and relay the
  `{ok, result|error}` envelope; CLI errors surface as tool errors with the
  CLI's own code/message. `npm test` drives the server end-to-end through the
  SDK's stdio client; registration instructions in its README.
  - [x] **Live-session control** (2026-09-24, the "fancier variant" that was
    deferred here): the app hosts agent commands for the project it holds, so
    they act on the open document - unsaved changes included - and arrive as
    one undo step instead of writing the project file. Discovery is a
    `Host.json` marker beside `Project.json`; a host that cannot be reached
    makes the command fail rather than fall back to the stale file.
    `AUDIONAUT_AGENT_ROUTING=0` opts out. See
    `docs/manual/11-cli-and-agents.md`.

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

- [x] **Auto-create symmetric fades when clips overlap.** Done 2026-10-15
  (feature/overlap-crossfade) as a **Shift-drag** gesture: while Shift is
  held during a horizontal clip drag (move/trim/stretch), every partial
  overlap of the dragged clips gets left fade-out = right fade-in = overlap
  (offsets reset to the edges), live per drag step from the drag-start fades
  (`PlayListContainer::crossfadeOverlappingNeighbours`,
  `PlayListItemDraggerControl::afterPositionCommit`); the undo step covers
  the whole playlist. Fades stay when clips separate later; no preference.
  Snap bypass replaced by *Options → Snap to Grid* (Cmd+G). Drops, paste and CLI verbs don't
  crossfade. Original notes: When a clip is
  dragged/trimmed so it overlaps its neighbour on the same playlist, create
  the crossfade automatically: the left clip gets a fade-out over the overlap
  (`setFadeOut`), the right clip a fade-in (`setFadeIn`), each clamped to the
  overlap range. Detection hooks exist —
  `PlayListContainer::itemIntersectingRange` / `itemsAtAbsoluteRange` — and
  the natural commit points are `DraggerControl::mouseUp` (after
  `validateData`) and `AudioTrack::dropPlayListItem`. Decide: recompute on
  every overlap change vs. only on first contact (protect user-edited fades),
  and whether a preference toggles the behavior.

## Arrangement editing

- [x] **Trimming a clip whose region is used elsewhere should edit a copy,
  not the shared region.** Done 2026-10-15 (feature/trim-shared-region,
  stacked on feature/overlap-crossfade): clip edge trims (not stretch, not
  moves) call `PlayListContainer::detachSharedRegion` in
  `PlayListItemDraggerControl::beforeFirstCommit`, right after the drag's
  undo snapshot; `UndoablePlayListItemAction` now records the clip's region,
  undo re-points the clip and removes the copy, redo re-adds it
  (`AudioRegionContainer::addAudioRegion`). No voice rebuild: the copy has
  the same resources. CLI `set-region` still retrims every clip, by design.
  Original notes: A `PlayListItem` holds a
  `std::shared_ptr<AudioRegion>`, so changing a clip's start/end/length on
  the timeline mutates the region every other clip referencing it plays —
  trimming one instance silently reshapes the others. When a trim gesture
  begins **on drag start** (`DraggerControl::mouseDown` /
  `DraggingHandle`), check whether the item's region is referenced by any
  other `PlayListItem` on the arrangement; if so, clone it first
  (`AudioRegionContainer::createRegion`) and point this item at the copy,
  so the whole gesture — including live preview while dragging — edits
  the private copy. On drag start specifically, not on mouse-up: the
  shared region must never be touched mid-gesture. The clone wants to be
  part of the same undo transaction as the trim (one gesture = one undo
  step), and naming should follow the existing duplicate convention in
  the Regions list.

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
- [ ] **One profiling and optimisation round before publishing the playback
  benchmark.** The hidden `[playback][bench]` scenario (PR #173, re-run
  2026-10-20 on an M5) gives, in % of the callback budget at 128 samples:
  plain stereo tracks 8 → 1.1, 16 → 3.5, 32 → 11.4, 64 → 22.2 (mean, no
  overruns); time-stretched (R3, ×0.9) ≈ 8 % per track at every buffer
  size, bursty (p99 ≈ 4× mean at ≤ 128 samples), 4 tracks overrun at
  128, 8 even at 512, 16 over budget on average. A "Performance" page
  with charts and tables is drafted in the website repo
  (`Audionaut-Web/docs/performance.md`, uncommitted) but should go live
  only after at least one round of work on the two findings:
  - [x] the plain-track cost grows superlinearly (8 → 32 tracks ≈ 10× the
    load for 4× the tracks). Fixed 2026-10-22 (perf/voice-channel-width):
    every voice processed the whole bus width, so a block cost
    voices × bus channels - quadratic in the track count.
    `VoiceSource::applyChannelMapping` gave the project's channel count to
    `ChannelRemappingAudioSource::setNumberOfChannelsToProduce`, which sizes
    the buffer the remapper asks of the source *below* it: every voice
    resampled, gained and faded all 2N channels (and reallocated that
    buffer on the audio thread when the project grew). `Playback::process`
    then added all 2N channels per voice into the bus. Now the remapper
    asks for the file's channels and `Playback` adds only the voice's
    destination channel. Also `MAX_VOICES` was 64, so a 64-stereo-track
    project played only half its voices (`getAvailableVoice` returns
    nullptr, jassert only) - now `MAX_AUDIO_CHANNELS * 2`.
    Bench, 128 samples, mean % before → after: 8 tracks 0.8 → 0.7,
    16 → 2.4 → 1.5, 32 → 8.8 → 3.3, 64 → 18.3 (64 voices) → 8.3 (all 128).
    Then the voice renders into a one-channel buffer (`Playback` and the
    bounce add it to `VoiceSource::getDestinationChannel()`), which ends the
    two bus-wide clears per voice and block, and
    `juce::ChannelRemappingAudioSource` went: it took its CriticalSection
    per block and per channel lookup and sized its buffer on the audio
    thread; `VoiceSource::renderChannel` copies the mapped file channel out
    of a buffer sized in prepareToPlay. Left, all linear: each mono voice decodes and
    resamples every channel of its file and keeps one (O(ch²) for
    multichannel recordings); `ClipResamplingSource` interpolates even at
    ratio 1.0; gain and fade-in still multiply once steady;
  - stretched clips: render ahead on a worker thread (the clip positions
    are known) so the hops stop landing in the callback, or find a
    cheaper per-track configuration of the stretcher.
  Then re-run the bench, refresh the page's numbers and "Last run" line,
  and publish.
  Re-run 2026-10-25 on the 1.7.0 release build (M5, idle): plain stereo
  tracks at 128 samples 8 → 0.4, 16 → 0.7, 32 → 1.5, 64 → 3.0 % (linear,
  no overruns at any buffer size); time-stretched 7–8 % per track, same
  bursty shape (4 tracks: 264 overruns at 128 samples; 8 tracks: 7 at
  512). The page is refreshed with these numbers (still uncommitted).
  Decision 2026-10-25: not published - eight time-stretched stereo tracks
  hitting 100 % of the callback is not a result to publish. Gated on the
  next item.
- [ ] **Profiling and optimisation round on time-stretched playback.**
  Goal: eight stretched stereo tracks (Rubber Band R3, ×0.9) stay under
  budget at 128 samples, including the 99th percentile, so the Performance
  page can go live. Today (1.7.0, M5, 128 samples) 8 tracks = 56 % mean,
  225 % p99, 3446 overruns in 20 s; 4 tracks = 27 % mean, 111 % p99, 264
  overruns. The mean is what R3 costs per track (7–8 % each, flat across
  buffer sizes), the p99 is the hop structure: the stretcher produces
  output in hops of its window size, so one callback pays for a whole hop
  while its neighbours pay nothing, and at ≤ 128 samples a hop is several
  callbacks' worth of budget. Steps:
  - Profile one stretched voice in the bench harness (Instruments, Time
    Profiler on the callback thread): how much is R3 itself, how much is
    `StretchAudioSource` / `ClipResamplingSource` / dynamics around it, and
    whether anything allocates or locks on the way (the standby lane and
    the prime path were the last offenders).
  - Flatten the bursts: render each stretched voice ahead on a worker
    thread into a FIFO the callback only drains (clip positions are known,
    so the lookahead can be a few hundred ms; seek, loop wrap and the
    clip-end re-prime - the item above - need a flush-and-refill path), or
    at least drive R3 in constant small chunks instead of letting it hop.
  - Halve the mean structurally: a voice is one channel, but its
    `ClipTransportSource` builds the `StretchAudioSource` with the file's
    channel count (`VoiceSource.cpp` passes `reader->numChannels`), so a
    stereo clip on a stereo track runs two R3 instances of two channels
    each - four channel-stretches for two output channels, and the same
    again for the resampler. One stretcher per clip shared by its voices
    (or a voice that stretches only its own channel) removes half the
    work, and more for multichannel files.
  - Lower the mean further with R3's cheaper options, measured against
    the stretch quality tests: `OptionWindowShort`, `OptionPitchHighSpeed`,
    `OptionChannelsTogether` once a stretcher sees both channels, and
    `OptionEngineFaster` (R2) as the fallback for many tracks. Today the
    backend sets `OptionEngineFiner | OptionProcessRealTime |
    OptionThreadingNever`.
  - Re-run `[playback][bench]` after each step, keep the before/after table
    in the PR, then refresh and publish the Performance page.

- [ ] **Double-precision mixing as an engine preference.** The renderer is
  a template and `AudioBusRenderer<double>` is instantiated (kept in the
  parallel-rendering branch as a compile check: the voice add and the
  recorder call convert under `if constexpr`), but nothing constructs it;
  the factory, `AudioBusInterface` and the tests all use the float
  variant. To offer it: a `Preferences` key ("Processing precision",
  Settings → Audio, next to the thread count) read by `AudiumFactory`,
  which builds the float or the double renderer; `AudioBusInterface`
  holds the renderer behind a sample-type-agnostic interface (or becomes
  a template itself) so `PlayListScheduler`, the exporter and the strips
  do not care; the device callback stays float, so the bus converts the
  input block on entry and the output block on exit (bus channels, partials,
  gain, pan and the master sum are then double); voices and the recorder
  stay float, the conversions are already in `renderTrack`. Effects
  (`docs/plans/effects/plan.md`) would need `process` for both sample
  types, which juce_dsp provides. Test: the same session rendered float
  and double matches within float precision, bit-identical for any worker
  count within one precision; bench the cost (memory doubles for the bus
  buffers, SIMD width halves).

- [ ] **Record another track's output (track-to-track recording).** Phase 8
  of `docs/plans/effects/plan.md` (section 1.10); the parallel renderer did
  the groundwork: every track is a job, and after its job the post-gain
  signal is in its bus channels and the panned stereo partial in
  `trackOutput`, both readable by another job behind a barrier. Missing:
  - an input source on `AudioChannelData` next to `inputChannel`: device
    channel, or a track's channel, or the L/R of a track's stereo output
    (scalars, two optional JSON keys; `commitChannelData`, the channel undo
    action and the strip's input selector extend as they are; removing a
    track resets the channels that point at it in the same snapshot);
  - job levels in `buildJobTable`: no track input = level 0, fed only by
    level 0 = level 1, ...; stage A runs one fork-join per level (an extra
    barrier only when used), the cost sort stays within a level; a cycle
    leaves a track unassigned, it reads silence and sets a flag for the UI;
  - the source switch in `renderInput`: the source track's bus channel
    (post-gain, pre-pan) for a mono patch, its partial for a stereo tap,
    instead of the device block; recorder, disk thread and take placement
    unchanged;
  - placement: a track-sourced take is generated in the same block for the
    block's position, so it lands exactly; a device take is late by the
    round trip (uncompensated today). Give the recorder a per-source offset
    now, so phase 4 (PDC) can subtract the source's pre-fader delay instead;
  - "internal only" as a third output next to main and direct out, so a
    source track feeds only the tracks that record it (mute would silence
    the recording too);
  - strip input menu Tracks >, routing indicator, cycle warning; CLI
    `track-input --track B --channel 0 --from-track A [--from-channel 0 |
    --left | --right]`, MCP tool, `info`;
  - tests: A's clip recorded onto B equals A's samples at the same position
    (0 and 3 workers); A -> B -> C chain; cycle silent and flagged;
    internal-only keeps A out of the mix; undo restores the routing. The
    serial bounce runs the levels in order, nothing new there.
  Engine side ~1 day, surfaces and tests 1-2 more; own branch after
  feature/parallel-rendering, independent of the effect phases.

## Mixer — follow-ups (found 2026-10-31 in PR #249)

- [ ] **Render a mono main mix like the stereo one and fold to mono at the
  very end.** Today a one-channel output (mono device, mono bounce) takes
  its own path through `AudioBusRenderer`: the bus channels sum unpanned
  into the main mix in `renderMaster`, the sends tap them unpanned on both
  sides (`tapSends`), the aux return ignores its balance and contributes
  its first side, and the mono mix runs through the master chain as two
  identical channels. That is a second set of rules to keep in step with
  the stereo path (pan law, send taps, aux balance, soon PDC and the
  per-track block). Instead render every main mix as the stereo one -
  `panIntoPartial`, the post-fader taps from the panned block, stage B
  with balance, dry gain, aux returns, master inserts and gain - and fold
  the finished stereo mix to one channel as the last step before the
  output block (and decide the fold: (L + R) / 2 keeps a centred source at
  its level, L + R keeps a hard-panned one). `monoMain` then disappears
  from `BlockContext`, `tapSends` loses its pan branch, `auxGainOf` goes,
  and the mono scenarios in `InsertRenderTests` and `SendRenderTests`
  state the fold instead of the unpanned sum. A stem export stays as it
  is: its outputs are the dry bus channels.

## Memory — follow-ups (found 2026-10-19 in PR #207)

- [ ] **Each undo step keeps two container snapshots.** An
  `UndoableContainerAction` stores the whole `AudioTrackContainer` twice
  as binary streams, `oldMemoryBlock` before the edit and `newMemoryBlock`
  after it, so the undo history grows with project size times steps:
  about 100 KB per split, measured in the live app while cutting one
  clip into 129. Clip-only edits
  already use the smaller `UndoablePlayListItemAction`. Options: store the
  new state as a diff against the old one, or move more edit kinds onto
  per-item actions. `getSizeInUnits` already counts both snapshots, so the
  UndoManager's trimming budget sees the real cost meanwhile.
- [ ] **Every edit still re-initialises every clip, about 20 ms per clip.**
  `PlayListItem::readFromJson` always calls `deinit()` and then `init()`,
  also on the non-rebuild read that applies an edit to the live
  container. So an agent verb applied to the open document retires and
  rebuilds the playback chain of every clip on the track, not only the
  ones it changed: measured in PR #205, the verb time grows by about
  20 ms per clip (1.65 s at 2 clips, 2.56 s at 40). The deinit is there
  because undo replays JSON into reused item objects whose region may
  change. Fix: on a non-rebuild read, keep the chain when region,
  resources and speed are unchanged and only refresh the cheap state
  (position, gain, fades).

## Analysis — follow-ups (found 2026-10-21 in the Essentia removal)

- [ ] **Show Analysis shows nothing for a type that is off in Settings.**
  The auto-analysis defaults (Settings > Analysis) are Segments and Beats;
  Onsets is off. Ticking *Onsets* in a track's Show Analysis menu only
  toggles the overlay's visibility (`toggleAnalysisVisibility` in
  `ChannelGroupHeaderComponent`), and with nothing in the cache for that
  type the overlay stays empty, without a word. Asking to see an analysis
  is asking for it: the toggle should enqueue the type for the track's
  files when the cache lacks it (`AnalysisWorker::enqueue` with an explicit
  type list runs even while auto-analysis is off) and show the clip
  header's progress meanwhile. Worth a hint that the type is off by
  default and where to turn it on, so the next file gets it without the
  detour.
- [ ] **The Auto Edit merge is chaotic on long material.** The hidden
  `[merge-jitter]` scenario (`MergeSensitivityTests`) moves Essentia's own
  recorded boundaries by 0.1 s and re-merges: on the 173 s recording that
  keeps only about half of the cuts, 0.25 s keeps a third, while the 15 s
  and 60 s recordings keep nearly all. Two inherited quirks of
  `EventMerger` do it: the top-N peak ranking of the summed activation,
  and the minimum-length pass that measures each gap against the previous
  peak rather than the last one kept, so a run of close peaks collapses to
  whichever follows a large enough gap. Neither is behind a flag yet, unlike
  the quirks `EventMerger::Parameters` already names; changing them changes
  every user's cuts, so it wants a listening session on the demo material,
  not only the harness.
