# Timeline drop rules: evaluation

**Question.** Dropping a region where clips already sit is ambiguous: should the neighbours move or not, and what happens to gaps? Can common rules be established?

**Answer.** Yes. The ambiguity is not open-ended. It consists of two separate choices, and the gesture itself already answers one of them:

1. A drop that lands on existing material cannot keep four things at once. Exactly one has to be given up, and each choice is a known edit mode.
2. If clips are allowed to move, a gap is either content or free space. Each reading has exactly one rule.

Both statements, and the rest of this document, are machine-checked in Lean 4 (`Formal/Timeline`, about 1500 lines, no Mathlib). Nothing in the application was changed.

## The model

Time is counted in ticks. A clip is `(id, start, src, len)`: a window of `len` ticks into its source, beginning at source tick `src`, placed at timeline tick `start`. A track is a list of clips.

What a track *means* is the relation `Plays tr id k t`: source tick `k` of clip `id` sounds at timeline tick `t`. Every rule is judged by what it does to this relation. Cutting a clip in two without moving anything therefore changes nothing, and a cut is not counted as a loss.

The same track has two views that carry the same information (`ofSeq_toSeq`, `toSeq_ofSeq` in `Views.lean`):

- **timeline view**: every clip has a start tick;
- **sequence view**: the clips in order, each with the gap in front of it. A positive gap is silence, zero is a butt joint, a negative gap is an overlap.

## The properties

For a drop of clip `n` at tick `n.start`, turning track `tr` into `tr'`:

| | Property | Meaning |
|---|---|---|
| P1 | Material | everything that sounded still sounds somewhere |
| P2 | Position | whatever still sounds, sounds when it did before |
| P3 | Separation | no two clips sound at once |
| P4 | Fidelity | the dropped clip sits where it was dropped, whole |
| P6 | Gaps kept | everything from the drop position on sounds exactly one drop length later |
| P7 | Past kept | everything before the drop position sounds when it did |
| P8 | Least displacement | no clip moves further than it must |
| P9 | Reversible | a remove rule restores the track |

## What was proved

### 1. The four-way conflict

If the dropped clip overlaps an existing clip, P1, P2, P3 and P4 cannot all hold (`conflict`, `Conflict.lean`). Any three can, and the property given up names the rule:

| Given up | Rule | What it does | Proved to keep |
|---|---|---|---|
| P3 Separation | **Layer** | nothing else changes, the overlap stays | P1, P2, P4 (`layer_material`, `layer_position`, `layer_fidelity`) |
| P1 Material | **Overwrite** | the covered part of each clip is cut away | P2, P3, P4, and all material outside the dropped range (`overwrite_position`, `overwrite_separation`, `overwrite_keeps_uncovered`) |
| P2 Position | **Shift** (Ripple or Push) | later clips move | P1, P3, P4, P7 (`ripple_*`, `push_*` in `Shift.lean`) |
| P4 Fidelity | **Relocate** | the drop goes to the first later place where its whole length fits | P1, P2, P3; never earlier; unchanged when it fits (`relocate_*`) |

There is no fifth option that avoids the choice.

### 2. Gaps decide between Ripple and Push

Within Shift:

- **Ripple** opens a hole of the dropped length and moves everything behind it by that length. It is the *only* rule that keeps the past and the gaps (`ripple_unique`).
- **Push** moves a clip only as far as needed to clear the one in front. It keeps the order and is the *least* such move, clip by clip (`shiftedRight_pushRight`, `pushRight_least`), so it never moves a clip further than Ripple (`pushRight_le_shift`).
- With no gaps behind the drop position the two are the same rule (`push_eq_ripple_of_tight`).
- With a gap they exclude each other: a rule that keeps gaps cannot leave a single downstream tick where it was (`gapsKept_moves_all`), while Push leaves everything behind a gap with room untouched (`pushRight_stops_at_gap`).

So "what is a gap?" is one bit. Gap is content: Ripple. Gap is free space: Push.

### 3. A drop position inside a clip

If the past must stay and the dropped clip must land where dropped, the clip under the drop position cannot stay whole (`straddle`, `Straddle.lean`). A shift rule has to cut it, snap the drop to a clip edge, or move the clip as a whole and change the past. Ripple and Push in the model cut.

### 4. Reversibility

| Rule | Undone by | Proof |
|---|---|---|
| Layer | remove, leave the place empty | `leaveGap_layer` |
| Ripple | remove, close the hole | `closeGap_ripple` (clip for clip when nothing was cut), `plays_closeGap_ripple` (always, as sound) |
| Push | nothing | `push_no_undo`: two different tracks give the same result |
| Overwrite | nothing | `overwrite_no_undo` |

Push forgets how large the gaps were. That is a property of the rule, independent of the undo history.

### 5. Addressing

A drop is addressed either by time (on a track lane) or by position in the sequence (in the Playlist panel: "before this row", "at the end"). A drop addressed by sequence position never lands inside a clip (`seq_no_straddle`), so Ripple cuts nothing there and is undone exactly (`closeGap_dropAtIndex`). In the sequence view it is plain list insertion: the new item takes the gap that was in front of the row, the row follows directly, the rest is untouched (`toSeq_ripple_before`).

### 6. Several tracks

Ripple on one track moves its material against all other tracks (`ripple_one_breaks_sync`). Opening the same hole on every track keeps simultaneous material simultaneous (`ripple_all_keeps_sync`, `MultiTrack.lean`).

## Today's behaviour against the rules

`Current.lean` writes the engine's present functions in the model. Every finding is a concrete track checked by the build.

| Gesture | Engine code | In the model | Finding |
|---|---|---|---|
| Lane drop, `place-clip`, `move-clip` | `createPlayListItemAtPositionUI` | Layer | consistent |
| Delete | `deletePlayListItem` | leave-gap | consistent, and the right inverse for Layer |
| Region on a Playlist row | `createPlayListItemUI` + `movePlayListItemsPosition` | `panelDrop` | neither Ripple nor Push, see below |
| Reorder Playlist rows | `movePlayListItemBefore` | Ripple + leave-gap | mismatched pair |
| Paste, duplicate | `findNextFreePosition` | `paste` | a Relocate that checks only the start |

**Region on a Playlist row.** Each clip that touches the one in front moves by the full new length; the first that does not touch ends the walk.

- It can leave an overlap and swap clips (`panelDrop_overlaps`): a 4-tick clip dropped in front of two 1-tick clips sends the first behind the new clip, and the second stays underneath it. Short clips after a longer drop are enough to trigger it.
- It jumps: with a 3-tick gap the following clip moves 4 ticks and the gap survives; with a 4-tick gap the clip does not move and the gap is gone. A clip that starts one tick later ends up three ticks earlier. Ripple and Push are both continuous here.
- The engine sorts with an unstable sort while the new clip and the row share a start. If the row sorts first, nothing moves and the two lie on top of each other.

**Reorder.** The drop is a Ripple but the pick-up leaves its place empty. Three clips edge to edge, first dragged in front of third: the track then starts with a hole and is one clip longer. With the matching pick-up (close the hole) the clips stay edge to edge.

**Paste.** The start is moved past occupied time but the length is not checked: four ticks pasted at tick 1 between clips at `[0,2)` and `[3,5)` go to tick 2 and overlap the second clip (`paste_overlaps`). Relocate goes to tick 5.

## Recommended rules

This part is a proposal built on the proofs, not a proved result.

**A. The view decides what is kept.** A gesture on a track lane is addressed by time: positions are the truth, so the dropped clip lands where dropped (P4) and nothing else moves unless asked. A gesture in the Playlist panel is addressed by sequence: order and gaps are the truth, so the drop is a Ripple.

**B. On a lane, a conflict means choosing what to give up.** Layer stays the default: it is the only position-keeping choice that loses nothing and can be taken back by removing the clip. The alternatives are explicit: a modifier for Overwrite, one for Ripple. The existing Shift-drag crossfade is Layer plus fades on the overlap and fits unchanged.

**C. In the Playlist panel a gap is content.** In the sequence view the gap is a field of the item behind it, so inserting a row cannot change it. Push would be a separate, explicit command ("use up gaps"), not a default, because it cannot be undone by a remove.

**D. Pick-up and drop of one move come from the same family.** Lane move: leave-gap + Layer. Playlist reorder and Playlist delete: close-gap + Ripple.

**E. Ripple by time inside a clip cuts or snaps.** If Ripple becomes available on a lane, a drop position inside a clip must either cut the clip there or snap to its edge. There is no third way that keeps the past.

**F. A Ripple that should keep tracks aligned opens the hole on all tracks.**

| Gesture | Rule | Change from today |
|---|---|---|
| Lane drop, `place-clip` | Layer | none |
| Lane move, `move-clip` | leave-gap + Layer | none |
| Lane drop with modifier | Overwrite or Ripple | new |
| Region on Playlist row / at end | Ripple at the row's start | replaces `movePlayListItemsPosition` |
| Reorder Playlist rows | close-gap + Ripple | pick-up closes its hole |
| Delete on a lane | leave-gap | none |
| Delete in the Playlist panel | close-gap | new, if the panel has its own delete |
| Paste, duplicate | Relocate | check the whole length |

## What the model leaves out

- Positions are `double` clocks in the engine and whole ticks here.
- Speed, gain and fades are reduced to the clip's extent. Fade extensions beyond the clip edge are not modelled.
- Several clips can share one region, and trimming a clip trims the region. Overwrite and the cut in Ripple would have to detach shared regions first.
- A tempo change moves clip starts but not their lengths in seconds, so it can create overlaps by itself. Not modelled.
- Not done: a weaker separation that admits crossfades ("at most two clips at once, none inside another").

## Checking it

```
brew install elan-init && elan toolchain install stable
cd Formal/Timeline
lake build                 # all theorems and all counterexamples
lake env lean Check.lean   # axioms of the main results; no sorryAx
```

Last run: clean build without warnings, no `sorry`, the main results rest only on Lean's standard axioms (`propext`, `Quot.sound`, `Classical.choice`).

| File | Content |
|---|---|
| `Basic.lean` | clips, tracks, `Plays`, overlap, `Seq` |
| `Properties.lean` | the properties as predicates |
| `Conflict.lean` | the conflict; Layer, Overwrite, Relocate |
| `Shift.lean` | Ripple, Push, uniqueness, least displacement, gaps |
| `Straddle.lean` | drop position inside a clip |
| `Reversibility.lean` | leave-gap, close-gap, what cannot be undone |
| `Views.lean` | sequence view, drops addressed by index |
| `MultiTrack.lean` | alignment across tracks |
| `Current.lean` | the engine's present behaviour and its counterexamples |
