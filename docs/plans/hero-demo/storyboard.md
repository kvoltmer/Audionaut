# README hero demo — storyboard

## 1. Purpose

A short autoplaying GIF at the top of the README (and on the website) that shows what
Audionaut does before the visitor reads a word. It is the first thing Show HN / Reddit /
JUCE-forum traffic sees when it clicks through to the repo. Model: the
[charmbracelet/vhs](https://github.com/charmbracelet/vhs) README — logo, one sentence, GIF,
then everything else.

Decisions:

- **Scope: full mix only, no stem separation** (2026-09-22, see §5). The arc is
  agent loads → app segments and assembles → agent smooths.
- **Format:** GIF for the README covering beats 2–5 (~35 s). The full take (beats 1–6,
  ~44 s) is exported as MP4 and linked underneath (GitHub comment-box upload →
  `user-images.githubusercontent.com` URL, renders inline).
- **Budget:** GIF ≤ 8 MB, 1000 px wide, 12–15 fps. Record at 1440–1600 px wide so text
  survives the downscale.
- **Layout:** Audionaut window left ~65 %, terminal running Claude Code right ~35 %.
  One continuous take, no cuts. The agent side is driven live so the tool calls on screen
  are real.

## 2. Storyboard

| t | beat | on screen | in GIF |
|---|---|---|---|
| 0–3 s | **Empty project** | Audionaut open on an empty project, terminal idle beside it. | – |
| 3–10 s | **Agent loads the project** | Typed into the terminal: *"open my demo project"* → the agent runs `open "/Users/klausvoltmer/Music/Audionaut/Demo-Project.audium"` and the app loads it: one track, the full 3:13 song, tempo **131**. | ✓ |
| 10–22 s | **Auto edit ▸ Create Segments** | Select the clip → Edit ▸ Auto Edit ▸ Create Segments. Cut lines appear, Less/More overlay visible. Click **More** until the segments are dense and even (**2 measures → 29 segments**, see §4 — the default is too coarse). Hold ~4 s on the dense state, then **Apply**. | ✓ |
| 22–27 s | **Auto edit ▸ Assemble Sequence** | Edit ▸ Auto Edit ▸ Assemble Sequence…, Random, 0:30 → the playlist rebuilds into a new 30 s arrangement (4 clips, butt-joined, **no fades**). | ✓ |
| 27–38 s | **Agent increases the fades** | Terminal: *"the cuts are hard — give every clip a half-second crossfade"* → the agent calls `get_project_info` (to learn the region names), then `clip_fades` on each clip; the fade ramps appear on every seam in the arrangement as the calls scroll by. | ✓ |
| 38–44 s | **Play & export** | Playhead runs across the new arrangement ~2 s, then Export → file appears in Finder. | – |

## 3. How the agent reaches the app

Two mechanisms, both verified 2026-09-23:

- **Loading** — there is no "open project" CLI/MCP verb, so the agent opens it through
  macOS: `open "<project>.audium"`. `.audium` is registered as a document package
  (`documentExtensions="audium"` in the .jucer, `LSTypeIsPackage`, role Editor) and
  LaunchServices has it claimed and bound, so it lands in Audionaut.
- **Agent → app** — the app watches the package and reloads foreign writes (undoably), so
  the agent's fades appear live. Verified on the running v1.6.0 app 2026-09-23.
- **App → agent** — the app only writes `Project.json` on save, but snapshots unsaved work
  to `Autosave.json`; a CLI/MCP run reads whichever is newer. Beat 5 therefore needs **no
  ⌘S** after the GUI beats. This needed a code change (2026-09-23) — before it, the agent
  read the last-saved project and would have saved the pre-Auto-Edit state back over the
  take.

## 4. Demo project — dry-run (2026-09-23)

`/Users/klausvoltmer/Music/Audionaut/Demo-Project.audium` — one track, `Epy - golden
person v3 -stimme lauter-.wav`, 192.9 s at position 0.458 s, **project tempo already 131**,
analysis already cached (`beat_degara` 131.04 BPM, `sbic` 37 boundaries). So no tempo
fiddling and no analyse step are needed on camera.

**Use 2 measures for Create Segments.** The default segmentation is too coarse on a song
this long — the merge collapses 36 boundaries into 12 wildly uneven ones. Measured on
copies of the project:

| setting | clips | min | max | median |
|---|---|---|---|---|
| default | 12 | 2.3 s | **49.9 s** | 6.4 s |
| 8 measures | 5 | 12.4 s | **62.3 s** | 49.9 s |
| 4 measures | 12 | 4.1 s | 35.7 s | 10.1 s |
| **2 measures** | **29** | 2.3 s | 12.8 s | 6.4 s |
| 1 measure | 36 | 1.4 s | 11.5 s | 5.5 s |

`measures` sets a *minimum* segment length (half the target), so bigger measures merge
harder. **More** halves the measures, so a few clicks from the default land on 2 — which
is the take to Apply.

Assemble (random, 30 s, seed 1234) on the 2-measure segmentation gives **4 clips**:
6.41 s + 12.82 s + 3.66 s + remainder. A longer target (0:45–1:00) would put more clips on
screen if 4 looks thin in rehearsal.

**Assemble drops all fades** — the arrangement comes out butt-joined at 0 ms (it rebuilds
the playlist from regions; auto-edit's crossfades do not survive). That is what makes
beat 5 land: the agent is not tweaking fades, it is smoothing seams that are genuinely
hard. Rehearsed: `clip_fades` with `fade_in`/`fade_out` 0.25 s and 0.5 s, unit seconds,
addressed by region name (e.g. `Epy - golden person v3 -stimme lauter--seg-20`), both
applied with `otherValuesAdjusted: false` — no clamping even at 0.5 s on a 3.66 s clip.

## 5. Why no stem separation

Separation was dry-run (Demucs weights installed 2026-09-22, sha256 verified). Three
findings ruled it out of the hero demo:

1. **`assemble` is per-track** (`AutoEdit.cpp:430` takes a single `trackId`). Rearranging
   one stem leaves the other three on the original timeline and the mix falls apart.
2. **Timing:** 1 min 47 s to separate a 60 s excerpt (10-core Mac17,4) — cannot be shown
   in real time.
3. **Licence:** the htdemucs weights are research-only, not MIT. A launch GIF is a poor
   place to foreground them.

Per-stem analysis, kept in case separation is demoed separately later (measured on a 60 s
excerpt): full mix 130.83 BPM / 11 segments; **Other** 130.96 / 11; Drums 130.93 / 5;
Bass 130.36 / 5; Vocals **97.75** (unusable grid) / 11. Drums and bass segment poorly —
SBic reacts to spectral change and a steady groove barely changes. **"Other"** is the stem
to auto-edit if it ever gets its own demo.

## 6. Prerequisites

- [x] **Demo project** — exists, tempo 131, analysis cached, dry-run green (§4).
- [x] **Agent beats rehearsed** — `open` for loading, `clip_fades` for the fades (§3, §4).
- [ ] **Listen to the result** and confirm the rearrangement plus crossfades sound good.
      Earlier excerpt-based renders for reference: `~/Music/audionaut-hero-assemble-test.wav`,
      `~/Music/audionaut-hero-agent-test.wav`.
- [ ] **Reset between takes with `ditto` (or `cp -Rp`), never plain `cp -R`.** The analysis
      cache is keyed on **mtime + size**; a plain copy changes mtime, invalidates the cache
      and auto edit then fails with *"Still analysing … (sbic, beat_degara)"*. Keep a
      pristine `ditto` copy of Demo-Project.audium aside and restore it between takes.
- [ ] **Window setup** — Audionaut + terminal side by side at 1440–1600 px total width,
      readable terminal font (≥ 14 pt), clean desktop, notifications off.
- [ ] **Prefs preset:** Assemble dialog defaults to Random / 0:30 (persisted in prefs) so
      there is no fiddling on camera.

## 7. Tooling (macOS)

- **Record:** QuickTime screen recording (free) or Screen Studio (auto zoom, cursor
  smoothing). Record the whole take once; trim afterwards.
- **GIF conversion:** `docs/plans/hero-demo/make-gif.sh recording.mov <start> <duration>`
  writes `hero.gif` + `hero.mp4` next to the recording (`brew install gifski` for better
  quality; the script prefers Homebrew ffmpeg over the stale one in `/usr/local/bin`).
- **Host:** GIF committed under `docs/media/`; MP4 uploaded via a GitHub comment box and
  linked from the README.

## 8. README placement (target)

```markdown
# Audionaut

Open-source multitrack audio editor that AI agents can drive over MCP.

<img alt="Audionaut demo: auto edit, assemble, agent-driven crossfades" src="docs/media/hero.gif" width="1000" />

▶ Full take with sound: <mp4 link>

[Download](https://audionaut.app) · [Discussions](…) · [Sponsor](…)
```

Badges and build instructions move below the fold.

## 9. Recording notes (first full take, 2026-09-23)

What the first take taught, for the next one:

- **Ask the agent for crossfades, and let it use the overlap geometry.** Setting `fade_in` and
  `fade_out` on butt-joined clips does *not* crossfade them — the ramps are sequential and the
  level dips ~2.5 dB at every seam. A real crossfade of length L is: outgoing clip
  `fade_out = L, fade_out_end = 0`; incoming clip `fade_in = 0, fade_in_start = -L`. Assemble
  leaves the seams at 0 ms (issue #108), so the agent beat has real work to do.
- **The payoff lands late.** The fade curves only appear in the arrangement ~4 s *after* the
  terminal reports completion, because the app has to reload. A GIF window that ends on the
  message misses the whole point — end it on the curves.
- **Beat 5 is slow.** Nine clips took 1 m 12 s of tool calls. The GIF has to compress it
  (10× worked) or the demo needs fewer clips: assemble to 0:30 rather than 0:45.
- **A region can repeat.** Assemble may place the same region twice, and then `clip_fades`
  cannot address it by name — the agent has to fall back to `at` positions. Harmless, but it
  shows up as an extra step on camera.
- **Timings actually used** (source seconds → GIF): 12–18 at 1×, 18–30 at 1× (Create Segments,
  the money shot), 30–42 at 2×, 42–150 at 10× (typing + agent work), 150–165 at 2× (the
  curves). 42 s total, 5.0 MB at 1000 px / 12 fps.
- **Cosmetics:** the terminal was translucent, so desktop icons showed through on the right.
  Use a solid background. The window layout was stacked (app above terminal), which read fine.
- **No audio** was captured; the full-take MP4 is silent. Enable audio capture if the linked
  video should have sound.
