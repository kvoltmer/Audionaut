# EventMerger — port of gaborgandalf layer-6 event merge to C++

## 1. Context

Audionaut's AutoEdit currently shells out to a Python package on the developer's machine:

```cpp
commandString += python + " $HOME/dev/gaborgandalf/gaborgandalf/automain.py autoedit";
```
— [`AutoEdit.cpp:79`](../../../Audionaut/Source/Engine/AutoEdit/AutoEdit.cpp#L79)

That Python pipeline ([gaborgandalf](https://github.com/Audionatomist/gaborgandalf), `autoedit.py:main_autoedit`)
runs seven layers. Layers 3–5 (SBic segmentation, onset detection, beat tracking) already have native
C++ equivalents in `Source/Engine/Analysis/`. **Layer 6 — the event merge — does not.** It is the step
that reconciles several independent event streams (segment boundaries + beat grids) into one set of
final cut points, and it is the actual "autoedit intelligence".

This plan ports layer 6 only.

**Reference implementation:** `~/dev/gaborgandalf/gaborgandalf/segments.py`
- `compute_event_mean_intervals` (`segments.py:22`) → Stage A
- `compute_event_merge_mexhat` (`segments.py:75`) → Stage B
- `compute_event_merge_heuristics` (`segments.py:149`) → Stage C
- `compute_event_merge_index_to_file` (`segments.py:193`) → **out of scope** (writes segment `.wav`s + JSON)
- `compute_event_merge_combined` (`segments.py:303`) → the orchestrator that chains A→B→C

> **No ticket tracker.** This repo has no issue tracker and no ticket IDs in its git history. The
> source of truth for scope is this document plus the reference Python. If a tracker is adopted later,
> link the epic here.

### In scope
- A standalone, pure-C++ `EventMerger` engine class implementing Stages A–C.
- Unit tests covering each stage plus an end-to-end parity check against the Python reference.

### Out of scope (explicitly)
- **Wiring into the app.** `AnalysisProvider` and `AutoEdit` are untouched by this epic. Nothing calls
  `EventMerger` in the running application yet.
- **Layer 7** (pydub segment assembly + crossfade) and removal of the Python subprocess.
- **Segment file export** (`compute_event_merge_index_to_file`) — the C++ merger returns boundaries in
  memory; it never writes audio to disk.
- **Reproducing Python's exact 9-stream input set.** That needs a librosa-`agglomerative` equivalent
  and `start_bpm`-seeded beat tracking, neither of which exists in C++. The merger accepts arbitrary
  streams instead; choosing them is the wiring epic's problem.

---

## 2. Decisions confirmed (grilling)

| # | Question | Decision | Consequence |
|---|----------|----------|-------------|
| D1 | How far does the port reach? | **Merger only, standalone.** | `AutoEdit.cpp` / `AnalysisProvider` untouched. No behaviour change in the app. Wiring is a separate, later epic. |
| D2 | The Python has several apparent bugs. Replicate or fix? | **Faithful by default, quirks behind `Parameters` flags.** | Default output matches the reference so it can be diffed; each quirk is individually switchable for later A/B on real material. |
| D3 | What feeds the merge? | **Generic N labelled streams.** | `merge(streams, duration, params)`. Merger stays pure and testable with synthetic input; caller picks streams. |
| D4 | Frames or seconds at the API? | **Seconds in/out, frame grid inside.** | Matches `SBicSegmenter`/`BeatSegmenter`/`AnalysisProvider`, which all speak seconds. Grid defaults to hop 512 @ 22050 Hz for librosa parity. |

---

## 3. Architecture

### Mirror the existing segmenters
`EventMerger` follows the shape already established by the three segmenters — nested
`Parameters` / `Result` structs, a defaulted constructor, an `analyze`-style entry point, and
`JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`:

- [`SBicSegmenter.h:24-66`](../../../Audionaut/Source/Engine/Analysis/SBicSegmenter.h#L24) — `Parameters` struct with tuning defaults, header free of Essentia types.
- [`BeatSegmenter.h:38-52`](../../../Audionaut/Source/Engine/Analysis/BeatSegmenter.h#L38) — `Parameters` + `Result` pair; `Result` carries the vector plus a scalar.
- GPL/commercial licence header on every new file, per [`CLAUDE.md`](../../../CLAUDE.md) *Conventions*.
- Everything in `namespace audium`.

### One deliberate departure: no Essentia
The three segmenters each guard their body with `ESSENTIA_ENABLED`
([`SBicSegmenter.cpp:14-26`](../../../Audionaut/Source/Engine/Analysis/SBicSegmenter.cpp#L14)) and their
tests are `#if`-compiled out when Essentia is absent
([`SBicSegmenterTests.cpp:7-17`](../../../Audionaut/Catch2Tests/Tests/SBicSegmenterTests.cpp#L7)).

**`EventMerger` is pure arithmetic over `std::vector<float>` — it needs no Essentia and must not
include it.** Consequence: its tests run unconditionally, including on a CI machine with no Essentia
build. This is the first analysis component with unconditional test coverage; keep it that way.

### Build system
- **Tests (CMake):** no change needed. `Engine/Analysis/*.cpp` is already globbed at
  [`Catch2Tests/CMakeLists.txt:70`](../../../Audionaut/Catch2Tests/CMakeLists.txt#L70), and
  `Tests/*.cpp` at line 108. New files are picked up on reconfigure.
- **App (Projucer):** required. The `.jucer` lists sources individually
  ([`Audionaut.jucer:47-70`](../../../Audionaut/Audionaut.jucer#L47)), so `EventMerger.{h,cpp}` must be
  added to the `Analysis` `<GROUP>` and the project re-saved. Per `CLAUDE.md`, never hand-edit
  `Builds/` — edit the `.jucer` and re-save.

### Algorithm specification

Public surface:

```cpp
struct EventStream {
    std::string        label;      ///< diagnostic only, e.g. "sbic", "beats_120"
    Kind               kind;       ///< Segmentation | Beat — selects Stage A treatment
    std::vector<float> times;      ///< event times in seconds, ascending
};

struct Parameters {
    // --- grid ---
    int   hopSize  = 512;          ///< librosa default
    float gridRate = 22050.0f;     ///< librosa analysis rate; frame = round(t * gridRate / hopSize)

    // --- algorithm ---
    int   numSegments        = 20; ///< `numsegs`: how many peaks to keep
    int   maxKernelPoints    = 250;///< `numpoints = min(250, numframes)`
    float kernelWidthDivisor = 10.0f; ///< ricker `a = interval / 10.0`
    int   minSegmentFrames   = 43; ///< `seglen_min`; ~0.998 s at the default grid

    // --- quirk flags (see §Quirks); defaults reproduce the Python ---
    bool  applyKernelWeights  = false;
    int   peakIndexOffset     = -2;
    bool  dropLastSegBoundary = true;
    bool  strictStreamMapping = false;
};

struct Result {
    std::vector<float> boundaries;   ///< merged cut points, seconds, ascending
    std::vector<float> activation;   ///< `final_sum`, per frame — for debugging/visualisation
};

Result merge (const std::vector<EventStream>& streams,
              float durationSeconds,
              const Parameters& params);
```

**Stage A — intervals and activation matrix** (`segments.py:22`)

```
segsInterval  = mean(diff(s.frames))            for every Segmentation stream
beatsInterval = mean(diff(b.frames))            for every Beat stream with >1 event
beatsValid    = indices of those Beat streams
intervals     = segsInterval ++ beatsInterval
numInputs     = |intervals|

final = zeros(numFrames, numInputs)
for i in [0, numSegs):     final[segs[i].frames[:-1], i] = 1      // note the [:-1]
for i in [0, |beatsValid|): final[beats[i].frames, numSegs + i] = 1  // note: beats[i], not beats[beatsValid[i]]
```

**Stage B — Ricker kernels, convolution, peak picking** (`segments.py:75`)

```
numPoints  = min(maxKernelPoints, numFrames)
kernel[i]  = ricker(numPoints, a = intervals[i] / kernelWidthDivisor)
weights[i] = 1 + i                                  // computed; never applied in the Python
column[i]  = convolve(final[:,i], kernel[i], mode='same')
finalSum   = rowwise sum of columns
ind        = indices of the numSegments largest finalSum values, + peakIndexOffset
```

`ricker` is `scipy.signal.ricker`, closed form:

```
A     = 2 / (sqrt(3a) * pi^(1/4))
vec   = arange(points) - (points - 1) / 2
xsq   = vec^2 ;  wsq = a^2
out   = A * (1 - xsq/wsq) * exp(-xsq / (2*wsq))
```

**Stage C — minimum-length heuristic** (`segments.py:149`)

```
sort(ind)
indFull = [0] ++ ind ++ [numFrames - 1]
keep    = diff(indFull) > minSegmentFrames
out     = [0] ++ indFull[1:][keep]
boundaries = out * hopSize / gridRate        // frames -> seconds
```

### Quirks — what the flags control

| Flag | Python behaviour | Where |
|------|------------------|-------|
| `applyKernelWeights = false` | `kernel_weights` is computed as `[1+i …]` and then **never used** — the convolution result is summed unweighted. Setting `true` multiplies column `i` by `1+i`, which is what the code appears to intend. | `segments.py:126,134` |
| `peakIndexOffset = -2` | Peak indices are shifted by an unexplained `- 2` before the heuristic. No comment justifies it. Can produce negative indices. Set `0` to disable. | `segments.py:138` |
| `dropLastSegBoundary = true` | Segmentation streams are indexed `segs[i][:-1]`, silently discarding each stream's final boundary. Beat streams are not truncated. | `segments.py:66` |
| `strictStreamMapping = false` | The beat loop writes `final[…, i + len(segs)] = beats[i]` but the matching interval came from `beats[beatsValid[i]]`. When every beat stream has >1 event these coincide; otherwise column `i` gets a kernel sized for a different stream. `true` uses `beats[beatsValid[i]]`. | `segments.py:70` |

Two further reference behaviours are **not** flagged, because they are unambiguous defects with no
sane "faithful" reading — they are handled defensively and documented in code:

- `segs_interval` has no `len > 1` guard (unlike `beats_interval`), so a single-boundary segmentation
  stream yields `mean(diff(...))` over an empty array → `NaN`, poisoning that column's kernel.
  **C++: skip such streams, as the beat path already does.** (`segments.py:53`)
- `seglen_min` is hardcoded to `43` inside `compute_event_merge_heuristics`, so the CLI's
  `--seglen-min` (default 2 seconds, `config.py:75`) never reaches the merge. **C++: exposed as
  `minSegmentFrames`, defaulted to 43 to preserve output.** (`segments.py:157`)

---

## 4. Phases (one PR each)

Single track — the work is one self-contained class. Four PRs, each off the epic base
`feature/event-merger`.

### Phase 1 — API contract + build plumbing
**Scope:** land the header contract and register the files with both build systems; `merge()` compiles
and returns an empty `Result`.

- **Files:** `Source/Engine/Analysis/EventMerger.h` (new), `EventMerger.cpp` (new, stub),
  `Catch2Tests/Tests/EventMergerTests.cpp` (new), `Audionaut/Audionaut.jucer` (+ regenerated `Builds/`).
- **Depends on:** nothing.
- **Reviewed in isolation:** the header *is* the review — read it as the spec for Phases 2–4. Tests
  pin seconds↔frame quantisation and the degenerate guards (empty streams, zero duration, `numSegments <= 0`).
- **Why this is its own PR:** the Projucer re-save regenerates `Audionaut/Builds/`, producing a large
  mechanical diff. Isolating it here keeps Phases 2–4 as pure algorithm diffs. Review the `.jucer`
  hunk and skim the rest.

### Phase 2 — Stage A: intervals + activation matrix
**Scope:** `compute_event_mean_intervals` equivalent, plus the `dropLastSegBoundary` and
`strictStreamMapping` flags and the `NaN`-interval guard.

- **Files:** `EventMerger.cpp`, `EventMerger.h` (flags), `EventMergerTests.cpp`.
- **Depends on:** Phase 1.
- **Reviewed in isolation:** synthetic streams with hand-computable intervals; assert column count
  (`|segs| + |valid beats|`), activation placement, and each flag's on/off behaviour.

### Phase 3 — Stage B: Ricker, convolution, peak picking
**Scope:** `ricker()`, `convolveSame()`, top-N selection; the `applyKernelWeights` and
`peakIndexOffset` flags. `merge()` becomes end-to-end functional (minus Stage C pruning).

- **Files:** `EventMerger.cpp`, `EventMerger.h` (flags), `EventMergerTests.cpp`.
- **Depends on:** Phase 2.
- **Reviewed in isolation:** the three helpers are independently testable — Ricker against a
  closed-form value table, `convolveSame` against a hand-worked example that **pins the centring
  convention**, top-N against a vector with a deliberate tie.

### Phase 4 — Stage C: heuristic, output, parity
**Scope:** minimum-length pruning, frames→seconds conversion, and the end-to-end parity fixture.

- **Files:** `EventMerger.cpp`, `EventMergerTests.cpp`,
  `Catch2Tests/TestFiles/EventMerger/reference-layer6.json` (new fixture).
- **Depends on:** Phase 3.
- **Reviewed in isolation:** pruning tests (gaps ≤ `minSegmentFrames` dropped, first boundary always
  `0`), output invariants (ascending, within `[0, duration]`), and the golden-fixture comparison.
- **Note:** the parity test is **skipped when the fixture is absent** — see Risk R1.

---

## 5. Contracts

Single track; no cross-track contract to pin. The Phase-1 header serves as the internal contract for
Phases 2–4.

---

## 6. Files to ADD / MODIFY

| Path | Add/Modify | Phase |
|------|-----------|-------|
| `Audionaut/Source/Engine/Analysis/EventMerger.h` | ADD | 1 (API), 2–3 (flags) |
| `Audionaut/Source/Engine/Analysis/EventMerger.cpp` | ADD | 1 (stub), 2, 3, 4 |
| `Audionaut/Catch2Tests/Tests/EventMergerTests.cpp` | ADD | 1, 2, 3, 4 |
| `Audionaut/Catch2Tests/TestFiles/EventMerger/reference-layer6.json` | ADD | 4 |
| `Audionaut/Audionaut.jucer` | MODIFY (Projucer) | 1 |
| `Audionaut/Builds/**` | REGENERATED — do not hand-edit | 1 |
| `Audionaut/Catch2Tests/CMakeLists.txt` | **no change** — already globs `Engine/Analysis/*.cpp` | — |

---

## 7. Tests

All in `EventMergerTests.cpp`, tagged `[engine][analysis][merge]`, running unconditionally (no
`ESSENTIA_ENABLED` guard).

**Phase 1**
1. A time in seconds quantises to the expected frame and back at the default grid.
2. Empty stream list → empty `Result`, no crash.
3. Zero/negative duration, and `numSegments <= 0` → empty `Result`.

**Phase 2**
4. Two segmentation + two beat streams → activation matrix has 4 columns; ones land on the expected frames.
5. A beat stream with a single event is excluded from the interval list and its column.
6. A segmentation stream with a single boundary is skipped, not turned into a `NaN` column.
7. `dropLastSegBoundary` on/off changes only the last activation of each segmentation stream.
8. `strictStreamMapping` diverges from the default **only** when a beat stream is invalid — with all-valid streams both settings give identical output.

**Phase 3**
9. Ricker output matches a closed-form value table; is symmetric; has its maximum at the centre.
10. `convolveSame` returns `max(M,N)` samples and matches a hand-worked example — **this test is the
    contract for the centring convention**.
11. `applyKernelWeights = true` scales column `i` by `1+i`; default leaves columns unweighted.
12. Top-N selection with tied values returns a deterministic, index-ordered set.
13. `numSegments` larger than the available frame count is clamped rather than out-of-bounds.

**Phase 4**
14. Boundaries are strictly ascending and lie within `[0, durationSeconds]`.
15. Two peaks closer than `minSegmentFrames` collapse to one boundary; the first boundary is always `0`.
16. A negative index produced by `peakIndexOffset` is discarded, not read out of bounds.
17. **Parity:** merging the fixture's recorded input streams reproduces the fixture's recorded Python
    boundaries within a one-frame tolerance. Skipped if the fixture file is absent.

---

## 8. Tracking

**Not applicable.** Audionaut has no analytics or telemetry subsystem — there are no existing events
to reuse, and this epic introduces none. Diagnostics are the `Result::activation` vector, which a
future visualisation can render.

---

## 9. QA

**No manual test plan — this epic has no user-visible surface.** Per decision D1 nothing in the
running app calls `EventMerger`, so there is nothing to click. Validation is entirely the test binary:

```
cmake -B build -S Audionaut/Catch2Tests
cmake --build build
./build/AudionautTests_artefacts/AudionautTests "[merge]"
```

Additionally, per phase, confirm the app target still builds after the Phase-1 Projucer re-save
(open `Audionaut/Builds/MacOSX/Audionaut.xcodeproj` and build once).

Manual QA arrives with the wiring epic, not this one.

---

## 10. Risks / verify during implementation

- **R1 — The reference Python cannot currently run on this machine.** `python3 -c "import numpy"`
  fails: numpy 2.0.2 is installed under `python3.9/site-packages` but resolved by Python 3.14
  (`ModuleNotFoundError: numpy._core._multiarray_umath`). Separately, `scipy.signal.ricker` was
  deprecated in SciPy 1.12 and **removed in 1.15**, so `segments.py:114` will raise on any modern
  SciPy even after the numpy mismatch is fixed.
  *Mitigation:* generate `reference-layer6.json` once from a pinned throwaway venv
  (`numpy<2, scipy<1.15, librosa`), commit it, and gate test 17 on its presence. Do not make the test
  suite depend on a working Python.
- **R2 — `np.argpartition` tie-breaking is unspecified.** It uses introselect; equal values may be
  returned in any order. Exact index parity with Python is therefore not guaranteed when
  `final_sum` has ties. *Mitigation:* deterministic index-ordered tie-break in C++ (test 12), and a
  one-frame tolerance in the parity test.
- **R3 — `np.convolve(mode='same')` centring is the classic off-by-one.** Verify the exact offset
  against a real numpy run before trusting the implementation; test 10 pins whatever is chosen.
- **R4 — `peakIndexOffset = -2` can yield negative frame indices.** In Python these survive into
  `ind_full`, where the subsequent `diff > seglen_min` filter happens to discard them. Replicate the
  *outcome*, not the out-of-bounds read (test 16).
- **R5 — Grid mismatch.** Audionaut's segmenters analyse at 44100 Hz
  ([`SBicSegmenter.h:33`](../../../Audionaut/Source/Engine/Analysis/SBicSegmenter.h#L33)) while the
  parity grid is 22050 Hz. Harmless here because the API takes seconds and quantises internally — but
  the wiring epic must not assume the two agree.

---

## 11. Verification (end-to-end)

The epic is done when, on the `feature/event-merger` base:

1. `cmake -B build -S Audionaut/Catch2Tests && cmake --build build` succeeds from a clean `build/`.
2. `./build/AudionautTests_artefacts/AudionautTests "[merge]"` passes, all 17 scenarios, with the
   parity fixture present.
3. The full suite `./build/AudionautTests_artefacts/AudionautTests` is green — confirming Phase 1's
   Projucer re-save broke nothing.
4. The macOS app target builds from the regenerated Xcode project.
5. `grep -rn "#include.*essentia" Source/Engine/Analysis/EventMerger.*` returns nothing. (Match the
   include, not the word — the class doc comment explains *why* there is no Essentia dependency, so a
   bare `grep essentia` matches prose and always "fails".)
6. `AutoEdit.cpp` and `AnalysisProvider.{h,cpp}` are unchanged versus `develop` — the standalone
   guarantee of decision D1.

---

## Branch structure

- **Epic base:** `feature/event-merger`, forked off `develop` with `--no-track`.
  `develop` is the integration branch — it carries `Engine/Analysis/`, and CI runs on it
  (`.github/workflows/`). `main` does not have the Analysis subsystem and is promoted separately.
- **Planning branch:** `plan/event-merger` off the epic base, containing only `docs/plans/eventmerger-layer6-port/`.
- **Draft PR:** `plan/event-merger` → `feature/event-merger`, so the reviewed diff is exactly this plan.
- **Each phase:** its own branch off `feature/event-merger`, its own PR back into it.
- `feature/event-merger` → `develop` once all four phases have merged.

> **History note.** The epic was briefly based on `feature/degara-analysis`, which has since merged to
> `develop` via [#23](https://github.com/kvoltmer/Audionaut/pull/23). The epic base was fast-forwarded
> to `develop` and the phase branches rebased onto it, so no degara-specific sequencing constraint
> remains.
