# Wiring EventMerger into AnalysisProvider and AutoEdit

## 1. Context

[`EventMerger`](../../../Audionaut/Source/Engine/Analysis/EventMerger.h) landed on `develop` as a
standalone, reference-checked port of gaborgandalf's layer-6 event merge
([plan](../eventmerger-layer6-port/plan.md)). Decision D1 of that epic deliberately left it
unwired: nothing in the running application calls it.

This epic wires it up, replacing the Python segmentation round-trip in AutoEdit.

### What AutoEdit does today

1. [`AutoEdit.cpp:37`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L37) bounces the
   selected playlist item to `juce::File::createTempFile(".wav")` at 48 kHz.
2. [`AutoEdit.cpp:79`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L79) shells out to
   `$HOME/dev/gaborgandalf/gaborgandalf/automain.py autoedit`.
3. `applyAutoEditResult` reads `count.txt`, then `createRegionsFromSegFile` turns
   `<base>-seg-data.json` into regions.

**Only the segmentation half is live.** `createPlayListFromSongFile` — the consumer of the Python
assembly — is `#if 0`'d out at [`AutoEdit.cpp:159`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L159).
So replacing segmentation removes the subprocess from the live path entirely, not half of it.

### In scope
- A merged-analysis entry point on `AnalysisProvider`, feeding `EventMerger` from existing analyses.
- A native AutoEdit path that creates regions from merged boundaries, with the Python path retained
  behind a config toggle for A/B comparison.
- A UI control to pick between them.

### Out of scope
- **Automatic assembly** (layer 7). Still unported, and its consumer is still `#if 0`'d out.
- **Removing `invokePython`.** Retained behind the toggle; deleting it is a later decision once the
  native path is trusted on real material.
- Changing `AnalysisWorker`, the background analysis path, or how non-AutoEdit views consume analyses.

> **No ticket tracker.** Scope is this document plus the conversation that produced it.

---

## 2. Decisions confirmed

| # | Question | Decision |
|---|----------|----------|
| D1 | What happens to `invokePython`? | **Keep behind a config toggle.** `AutoEditConfig::Source` = `Native` (default) or `Python`, so merged boundaries can be A/B'd against the reference on real material. |
| D2 | Which analyses feed the merge? | **SBic, Degara, Onset.** No MultiFeature beat tracker, no decimated variants. |
| D3 | Cache the merged result? | **No — recompute.** The expensive inputs are already cached per file; the merge is milliseconds. A cache key that ignored `Parameters` would go stale when `numSegments` changes. |
| D4 | How does AutoEdit reach `AnalysisProvider`? | **Via the existing path.** No new API needed — see Architecture. |

### Assumptions (not explicitly confirmed — overturn freely)
- **Onset maps to `Kind::Beat`.** `Kind` selects the reference's treatment: segmentation streams get
  the `dropLastSegBoundary` truncation and lead the column order. Onsets are dense event streams, not
  structural boundaries. The resulting kernel-width spread is SBic wide, Degara narrow, Onset narrowest.
- **`EventMerger::Parameters` keeps its defaults**, including the four reference quirks, except
  `numSegments`, which comes from `AutoEditConfig::numSegments` (the existing UI slider).

---

## 3. Architecture

### The dependency path already exists
`AudiumEngine` has no analysis accessor, but `AudioTrackContainer` does
([`AudioTrackContainer.h:189`](../../../Audionaut/Source/Engine/Group/AudioTrackContainer.h#L189)),
and that is the established idiom — six call sites already use it, e.g.
[`AudioClipView.cpp:25`](../../../Audionaut/Source/Interface/Views/AudioClipView.cpp#L25) and
[`PlayListItemDraggerControl.h:36`](../../../Audionaut/Source/Interface/Controls/PlayListItemDraggerControl.h#L36):

```cpp
audiumEngine->getAudioTrackContainer()->getAnalysisProvider()
```

`AutoEdit::applyAutoEditResult` already calls `getAudioTrackContainer()`. **No new accessor, no
constructor injection.**

### Stream assembly is a pure function
The merge inputs come from three `analyzeFile()` calls, but assembling them into `EventStream`s is
pure arithmetic-free bookkeeping. Splitting it out keeps it testable without Essentia — matching
`EventMerger`'s own unconditional-test property:

```cpp
// AnalysisProvider
static std::vector<EventMerger::EventStream>
    makeMergeStreams (const std::vector<float>& sbic,
                      const std::vector<float>& degara,
                      const std::vector<float>& onsets);

EventMerger::Result analyzeMerged (const juce::File& audioFile,
                                   float durationSeconds,
                                   const EventMerger::Parameters& params);
```

`analyzeMerged` runs the three `analyzeFile()` calls (each cache-consulting, as today), assembles,
merges, and returns. It does **not** write to the cache (D3).

### Region creation loses its JSON round-trip
`createRegionsFromSegFile` parses `{start, end}` sample pairs from disk. The native path has
boundaries in seconds already, so it needs a sibling that takes them directly:

```cpp
bool createRegionsFromBoundaries (const std::vector<float>& boundarySeconds);
```

Consecutive boundaries form each region, mirroring the existing loop's pairing. The undo/redo
wrapping in `applyAutoEditResult` is unchanged.

### Duration
`EventMerger::merge` needs the material's length. The bounced file's duration comes from the engine's
`AudioFormatManager` via a reader, which AutoEdit can reach through the resource container. Guard
against a zero-length read — `merge()` returns empty for a non-positive duration, which would
silently produce no regions.

---

## 4. Phases (one PR each)

Single track, three PRs off `feature/event-merger-wiring`.

### Phase 1 — Merged analysis on AnalysisProvider
**Scope:** `makeMergeStreams()` and `analyzeMerged()`. Nothing calls them yet.

- **Files:** `AnalysisProvider.{h,cpp}`, `Catch2Tests/Tests/AnalysisProviderMergeTests.cpp` (new).
- **Depends on:** nothing.
- **Reviewed in isolation:** `makeMergeStreams` is pure, so its tests run unconditionally — stream
  count, `Kind` assignment, ordering, and empty-input handling. `analyzeMerged` gets an
  `ESSENTIA_ENABLED`-gated test over `TestFiles/_export_TRK-18.wav`.
- **Note:** no Projucer change — no new source files under `Source/`.

### Phase 2 — Native AutoEdit path
**Scope:** `AutoEditConfig::Source`, `createRegionsFromBoundaries()`, and the branch in
`invokeAutoEdit`. Python retained, no longer the default.

- **Files:** `AutoEdit.{h,cpp}`, `Catch2Tests/Tests/AutoEditTests.cpp`.
- **Depends on:** Phase 1.
- **Reviewed in isolation:** the diff is one new branch plus one new region-creation method; the
  Python path is untouched, so the two can be compared side by side. Existing `AutoEditTests` is
  extended to drive the native path (it currently drives the Python one and passes regardless of
  outcome — see Risks).

### Phase 3 — UI and menu cleanup
**Scope:** a source selector in `AutoEditComponent`, and the duplicate menu item.

- **Files:** `Interface/Dialogs/AutoEditComponent.{h,cpp}`, `Application/AudiumApplication.cpp`.
- **Depends on:** Phase 2.
- **Reviewed in isolation:** UI-only. `AudiumApplication.cpp:325` adds `CommandIDs::autoEdit`
  unconditionally and again at line 329 inside `#if AUTO_EDIT_ENABLED`; with the flag at 0 the item
  appears once, so the `#if` block currently gates nothing. Remove the redundant pair.

---

## 5. Files to ADD / MODIFY

| Path | Add/Modify | Phase |
|------|-----------|-------|
| `Source/Engine/Analysis/AnalysisProvider.h` | MODIFY | 1 |
| `Source/Engine/Analysis/AnalysisProvider.cpp` | MODIFY | 1 |
| `Catch2Tests/Tests/AnalysisProviderMergeTests.cpp` | ADD | 1 |
| `Source/Engine/AutoEdit/AutoEdit.h` | MODIFY | 2 |
| `Source/Engine/AutoEdit/AutoEdit.cpp` | MODIFY | 2 |
| `Catch2Tests/Tests/AutoEditTests.cpp` | MODIFY | 2 |
| `Source/Interface/Dialogs/AutoEditComponent.{h,cpp}` | MODIFY | 3 |
| `Source/Application/AudiumApplication.cpp` | MODIFY | 3 |
| `Audionaut.jucer` | **no change** — no new files under `Source/` | — |

---

## 6. Tests

**Phase 1** (`[engine][analysis][merge]`)
1. Three non-empty analyses give three streams: SBic as `Segmentation`, Degara and Onset as `Beat`.
2. Segmentation stream leads the ordering.
3. An empty analysis contributes no stream; all-empty gives no streams.
4. Event times pass through unchanged, in seconds.
5. *(Essentia-gated)* `analyzeMerged` over the test file returns ascending boundaries within the file's duration.

**Phase 2** (`[engine][autoedit]`)
6. The native path creates one region per consecutive boundary pair.
7. Region positions are in seconds and ascending.
8. A merge yielding no boundaries creates no regions and reports an error rather than asserting.
9. `Source::Python` still takes the subprocess branch.

**Phase 3** — none; UI-only.

---

## 7. Tracking

Not applicable — no analytics subsystem in this codebase.

---

## 8. QA

Unlike the port epic, this one **is** user-visible.

1. Open a project with an audio clip; select a playlist item.
2. Edit → Auto Edit. Confirm the dialog offers a source selector (Phase 3).
3. With **Native**: confirm regions appear on the default track, that they tile the material without
   gaps or overlaps, and that a single Undo removes them all.
4. With **Python**: confirm the old behaviour still runs *if* a working gaborgandalf environment
   exists — see Risk R2.
5. Vary the `numSegments` slider and confirm the region count responds.

---

## 9. Risks

- **R1 — Every run does three cold analyses.** The bounce target is
  `juce::File::createTempFile(".wav")`, a fresh path each invocation, so the `AnalysisCache` key
  (path + size + modification time) can never hit. SBic, Degara and Onset therefore run in full on
  every AutoEdit, on the message thread via `runThread()`. On long material this will be slow.
  *Mitigation:* measure in Phase 2; if it hurts, either bounce to a deterministic path or run the
  merge on the `AnalysisWorker` thread. Not solved in this epic.
- **R2 — The Python path is currently broken anyway.** `python3` is 3.14 with no numpy, so
  `Source::Python` will fail on this machine regardless. The toggle is for later comparison against
  a repaired environment, not something Phase 2 can validate end to end.
- **R3 — `AutoEditTests` passes regardless of outcome.** It calls `invokeAutoEdit` and only prints
  the error callback; it asserts nothing about the result, and most of its body is commented out.
  Phase 2 must add real assertions rather than trusting the existing green.
- **R4 — Grid mismatch (R5 of the port plan).** The segmenters analyse at 44100 Hz while
  `EventMerger`'s default grid is 22050 Hz. Harmless because the merge API takes seconds, but do not
  "align" the two by passing the segmenter rate as `gridRate` — that changes every kernel width.

---

## 10. Verification

1. Full Catch2 suite green.
2. macOS app target builds.
3. Auto Edit with **Native** produces regions on real material, undoable in one step.
4. `grep -rn "gaborgandalf" Source/` returns only the retained `invokePython`.

---

## Branch structure

- **Epic base:** `feature/event-merger-wiring`, forked off `develop` with `--no-track`.
- **Planning branch:** `plan/event-merger-wiring` off the epic base, containing only this document.
- **Draft PR:** planning → epic base, so the reviewed diff is exactly the plan.
- **Each phase:** its own branch off the epic base, its own PR back into it.
- `feature/event-merger-wiring` → `develop` once all three phases have merged.
