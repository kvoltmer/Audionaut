# Wiring EventMerger into AnalysisProvider and AutoEdit

## 1. Context

[`EventMerger`](../../../Audionaut/Source/Engine/Analysis/EventMerger.h) landed on `develop` as a
standalone, reference-checked port of gaborgandalf's layer-6 event merge
([plan](../eventmerger-layer6-port/plan.md)). Decision D1 of that epic deliberately left it unwired.

This epic wires it up, replacing the Python segmentation round-trip in AutoEdit **and removing the
bounce**: AutoEdit reads the analyses already sitting in the cache instead of rendering a temporary
file and analysing that.

### What AutoEdit does today

1. [`AutoEdit.cpp:37`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L37) bounces the
   selected playlist item to `juce::File::createTempFile(".wav")` at 48 kHz.
2. [`AutoEdit.cpp:79`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L79) shells out to
   `$HOME/dev/gaborgandalf/gaborgandalf/automain.py autoedit` on that temp file.
3. `applyAutoEditResult` reads `count.txt`, then `createRegionsFromSegFile` turns
   `<base>-seg-data.json` into regions.

Two facts about that flow drive this plan.

**Only the segmentation half is live.** `createPlayListFromSongFile` — the consumer of the Python
assembly — is `#if 0`'d out at [`AutoEdit.cpp:159`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L159).
Replacing segmentation removes the subprocess from the live path entirely, not half of it.

**The bounce introduces a timeline mismatch.** Boundaries are derived from the bounced mix and
converted with the *bounce's* 48 kHz rate, but the regions are created against
`track->getResourceGroups()[0]` — the **source** resource
([`AutoEdit.cpp:192`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L192)). The two
timelines coincide only when the playlist item is a 1:1 pass-through of the whole source at 48 kHz.
Analysing the source directly removes the mismatch rather than papering over it.

### The analyses are already cached
`AnalysisWorker` runs **all four** analysis types on every newly added audio file by default
([`AnalysisWorker.h:48`](../../../Audionaut/Source/Engine/Analysis/AnalysisWorker.h#L48)) on a
low-priority background thread, and `AnalysisCache` persists them to `AnalysisData.json`. So SBic,
Onset and BeatDegara for the source file are normally present before AutoEdit is ever opened — at
zero cost and keyed to a stable path, unlike a per-run temp file.

### In scope
- A merged-analysis entry point on `AnalysisProvider`, fed from cached analyses.
- A native AutoEdit path that creates regions from merged boundaries of the **source resource**,
  with no bounce and no temp files.
- A UI control to pick between native and the retained Python path.

### Out of scope
- **Automatic assembly** (layer 7). Still unported; its consumer is still `#if 0`'d out.
- **Removing `invokePython`.** Retained behind the toggle.
- Changing `AnalysisWorker` or the background analysis path itself.

> **No ticket tracker.** Scope is this document plus the conversation that produced it.

---

## 2. Decisions confirmed

| # | Question | Decision |
|---|----------|----------|
| D1 | What happens to `invokePython`? | **Keep behind a config toggle.** `AutoEditConfig::Source` = `Native` (default) / `Python`. |
| D2 | Which analyses feed the merge? | **SBic, Degara, Onset.** No MultiFeature tracker, no decimated variants. |
| D3 | Cache the merged result? | **No — recompute.** Inputs are already cached; a key ignoring `EventMerger::Parameters` would go stale when `numSegments` changes. |
| D4 | How does AutoEdit reach `AnalysisProvider`? | **Existing path** — `getAudioTrackContainer()->getAnalysisProvider()`, already the idiom in six UI call sites. No new API. |
| D5 | Bounce, or use cached analysis? | **Cached analysis of the source resource. No bounce.** Removes the temp file, the `AudioExportThread` dependency, the cold-cache cost and the timeline mismatch above. |
| D6 | What if only some analyses are cached? | **Produce no boundaries.** Never merge a partial set: it yields plausible-looking but different cut points, which is worse than producing none because it looks like it worked. `findMissingMergeAnalyses()` names what is outstanding so the caller can say so. |

### Assumptions (not explicitly confirmed — overturn freely)
- **Onset maps to `Kind::Beat`.** `Kind` selects the reference's treatment: segmentation streams get
  the `dropLastSegBoundary` truncation and lead the column order. Onsets are dense event streams, not
  structural boundaries. Kernel widths end up SBic wide, Degara narrow, Onset narrowest.
- **The Python path also drops the bounce**, running on the same source file. Otherwise the two
  sources would analyse different material and the A/B comparison D1 exists for would be meaningless.
- **Cache-only, no compute-on-miss.** If any of the three analyses is absent, AutoEdit reports that
  rather than running Essentia on the message thread — which is the very cost D5 removes. In normal
  use the worker has already populated them.
- **The target comes from the chosen playlist item**, falling back to `getResourceGroups()[0]` when
  no item is named.

---

## 3. Architecture

### The dependency path already exists
`AudiumEngine` has no analysis accessor, but `AudioTrackContainer` does
([`AudioTrackContainer.h:189`](../../../Audionaut/Source/Engine/Group/AudioTrackContainer.h#L189)),
and so does `AudioTrack`
([`AudioTrack.h:222`](../../../Audionaut/Source/Engine/Group/AudioTrack.h#L222)). `AutoEdit` already
calls `getAudioTrackContainer()`. **No new accessor, no constructor injection.**

### Stream assembly is a pure function
Assembling three analyses into `EventStream`s is pure bookkeeping. Splitting it out keeps it testable
without Essentia, matching `EventMerger`'s own unconditional-test property:

```cpp
// AnalysisProvider
static std::vector<EventMerger::EventStream>
    makeMergeStreams (const std::vector<float>& sbic,
                      const std::vector<float>& degara,
                      const std::vector<float>& onsets);

/** Merges the cached analyses of a file. Returns an empty result if any is missing. */
EventMerger::Result mergeCachedAnalyses (const juce::File& audioFile,
                                         float durationSeconds,
                                         const EventMerger::Parameters& params) const;
```

`mergeCachedAnalyses` reads via `getSegments(type, file)` — cache-only, never computing (see
assumptions). It does not write to the cache (D3), so it is `const`.

### The native AutoEdit flow

```
invokeAutoEdit(config)
  track    = trackContainer->getAudioTrack(config.trackId)
  group    = track->getResourceGroups()[0]
  resource = group->getAudioResources()[0]
  file     = resource->getFullPathName()
  duration = resource->getFileLength(audium::seconds)

  Native:  provider->mergeCachedAnalyses(file, duration, params)
           -> createRegionsFromBoundaries(boundaries, track, group)
  Python:  invokePython(config with file)   // unchanged apart from its input
```

No `AudioExportThread`, no temp file, no `count.txt`, no JSON.

`createRegionsFromBoundaries` pairs consecutive boundaries into regions, mirroring the existing loop
but taking seconds directly. The undo wrapping in `applyAutoEditResult` is unchanged.

`EventMerger::Parameters` keeps its defaults, including the four reference quirks, except
`numSegments`, which comes from `AutoEditConfig::numSegments` (the existing UI slider).

---

## 4. Phases (one PR each)

Single track, three PRs off `feature/event-merger-wiring`.

### Phase 1 — Merged analysis on AnalysisProvider
**Scope:** `makeMergeStreams()` and `mergeCachedAnalyses()`. Nothing calls them yet.

- **Files:** `AnalysisProvider.{h,cpp}`, `Catch2Tests/Tests/AnalysisProviderMergeTests.cpp` (new).
- **Depends on:** nothing.
- **Reviewed in isolation:** `makeMergeStreams` is pure, so its tests run unconditionally. The
  cache-miss behaviour of `mergeCachedAnalyses` is testable by populating an `AnalysisCache`
  directly — no Essentia needed. One `ESSENTIA_ENABLED`-gated test covers a real analysis end to end.
- **Note:** no Projucer change — no new files under `Source/`.

### Phase 2 — Native AutoEdit path, bounce removed
**Scope:** `AutoEditConfig::Source`, `createRegionsFromBoundaries()`, the native branch, and deleting
the bounce.

- **Files:** `AutoEdit.{h,cpp}`, `Catch2Tests/Tests/AutoEditTests.cpp`.
- **Depends on:** Phase 1.
- **Reviewed in isolation:** the removal of `AudioExportThread` and the temp file is most of the
  diff and is mechanical; the new branch is small. `createRegionsFromSegFile`, `getCountFromFile`
  and `getBaseName` become unreachable once Python takes the source file directly — delete them in
  this phase rather than leaving dead code.
- **Rewrite the test.** `AutoEditTests` currently asserts nothing (see R2).

### Phase 3 — UI and menu cleanup
**Scope:** a source selector in `AutoEditComponent`; the duplicate menu item.

- **Files:** `Interface/Dialogs/AutoEditComponent.{h,cpp}`, `Application/AudiumApplication.cpp`.
- **Depends on:** Phase 2.
- **Reviewed in isolation:** UI-only. `AudiumApplication.cpp:325` adds `CommandIDs::autoEdit`
  unconditionally and again at line 329 inside `#if AUTO_EDIT_ENABLED`; with the flag at 0 the item
  appears once, so the `#if` gates nothing. Remove the redundant pair.
- The playlist-item combo keeps its meaning and needs no change (see R3).

---

## 5. Files to ADD / MODIFY

| Path | Add/Modify | Phase |
|------|-----------|-------|
| `Source/Engine/Analysis/AnalysisProvider.{h,cpp}` | MODIFY | 1 |
| `Catch2Tests/Tests/AnalysisProviderMergeTests.cpp` | ADD | 1 |
| `Source/Engine/AutoEdit/AutoEdit.{h,cpp}` | MODIFY (net deletion) | 2 |
| `Catch2Tests/Tests/AutoEditTests.cpp` | MODIFY | 2 |
| `Source/Interface/Dialogs/AutoEditComponent.{h,cpp}` | MODIFY | 3 |
| `Source/Application/AudiumApplication.cpp` | MODIFY | 3 |
| `Audionaut.jucer` | **no change** — no new files under `Source/` | — |

---

## 6. Tests

**Phase 1** (`[engine][analysis][merge]`)
1. Three non-empty analyses give three streams: SBic as `Segmentation`, Degara and Onset as `Beat`.
2. The segmentation stream leads the ordering.
3. An empty analysis contributes no stream; all-empty gives no streams.
4. Event times pass through unchanged, in seconds.
5. `mergeCachedAnalyses` returns an empty result when any of the three is absent from the cache.
6. With all three present in the cache, it returns ascending boundaries within the duration.
7. *(Essentia-gated)* end to end over `TestFiles/_export_TRK-18.wav`.

**Phase 2** (`[engine][autoedit]`)
8. The native path creates one region per consecutive boundary pair, on the source resource group.
9. Region positions are in seconds, ascending, and within the resource length.
10. Missing analyses create no regions and report an error rather than asserting.
11. No temp file is created and no subprocess is spawned on the native path.

**Phase 3** — none; UI-only.

---

## 7. Tracking

Not applicable — no analytics subsystem in this codebase.

---

## 8. QA

1. Add an audio file to a project and wait for background analysis to finish (the footer indicator).
2. Edit → Auto Edit, source **Native**. Regions should appear essentially instantly — no progress
   bar, no bounce.
3. Confirm the regions tile the source without gaps or overlaps, and that one Undo removes them all.
4. Vary `numSegments` and confirm the region count responds.
5. Invoke it on a file whose analysis has *not* finished; confirm a clear message rather than a hang.
6. **Compare against the previous behaviour**: check regions land on musically sensible points, since
   removing the bounce changes what is analysed.

---

## 9. Risks

- **R1 — Semantics change with the bounce removed.** Analysis now describes the source file rather
  than the rendered playlist item. This is the *intended* change and fixes the timeline mismatch
  described in Context, but boundaries will differ from what the Python path produced. Expect
  different — and more correct — cut points.
- **R2 — `AutoEditTests` passes regardless of outcome.** It calls `invokeAutoEdit`, prints the error
  callback, and asserts nothing; most of its body is commented out. The existing green means nothing.
  Phase 2 must add real assertions.
- **R3 — `playlistItemId` selects the target audio.** The arrangement is what is being edited, so the
  chosen playlist item decides which audio the edit applies to: its region names the resource group,
  and the group names the file. With the usual one item to one region to one file arrangement this is
  the same audio a first-group fallback would find, which is why the distinction is invisible by
  default - and why getting it wrong would go unnoticed until a track carries more than one. The
  combo stays; Phase 3 has nothing to do here.
- **R4 — Multi-resource groups.** Within the resolved group only the first resource is analysed. A
  group carrying several channels as separate resources gets boundaries from just one. Acceptable for
  now; call it out if it becomes real.
- **R5 — Grid mismatch (R5 of the port plan).** Segmenters analyse at 44100 Hz while `EventMerger`'s
  default grid is 22050 Hz. Harmless because the merge API takes seconds — but do not "align" them by
  passing the segmenter rate as `gridRate`, which would change every kernel width.

---

## 10. Verification

1. Full Catch2 suite green.
2. macOS app target builds.
3. Auto Edit with **Native** produces regions on real material, undoable in one step.
4. `grep -rn "AudioExportThread\|createTempFile" Source/Engine/AutoEdit/` returns nothing.
5. `grep -rn "gaborgandalf" Source/` returns only the retained `invokePython`.

---

## Branch structure

- **Epic base:** `feature/event-merger-wiring`, forked off `develop` with `--no-track`.
- **Planning branch:** `plan/event-merger-wiring` off the epic base, containing only this document.
- **Draft PR:** planning → epic base, so the reviewed diff is exactly the plan.
- **Each phase:** its own branch off the epic base, its own PR back into it.
- `feature/event-merger-wiring` → `develop` once all three phases have merged.
