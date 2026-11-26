/-
  What the engine does today, written in the model, and where it departs from
  the rules. Each function follows the C++ named in its comment
  (`Source/Engine/PlayList/PlayListContainer.cpp`). Every claim is a concrete
  track that the build checks.
-/
import Timeline.MultiTrack

namespace Timeline

instance (tr : Track) : Decidable (OverlapFree tr) := by
  unfold OverlapFree; infer_instance

/-! ### Lane drop, `place-clip`, `move-clip`: `createPlayListItemAtPositionUI`

The clip is put at the position and nothing else changes. That is `layer`. -/

/-! ### Delete: `deletePlayListItem`

The clip is removed and nothing else changes. That is `leaveGap`. -/

/-! ### Region dropped on a playlist row: `createPlayListItemUI` -/

/-- `juce::Range::intersects`. -/
def touches (a b : Clip) : Bool := decide (a.start < b.stop ∧ b.start < a.stop)

/-- `movePlayListItemsPosition`: walk the clips behind the new one; each one
    that touches the clip in front of it moves by the full new length, and
    the first one that does not touch ends the walk. -/
def makeWay (L : Nat) (prev : Clip) : Track → Track
  | [] => []
  | x :: rest => if touches x prev then x.shift L :: makeWay L (x.shift L) rest else x :: rest

/-- `createPlayListItem`: the new clip takes the start of the item at the
    index, or the end of the last item. At index 0 it goes in front of the
    first item when there is room. -/
def panelPos (len i : Nat) (tr : Track) : Nat :=
  let pos := match tr[i]? with
    | some c => c.start
    | none => (tr.getLast?.map Clip.stop).getD 0
  if i = 0 ∧ len ≤ pos then pos - len else pos

/-- The whole gesture. The engine sorts by start with an unstable sort while
    the new clip and the item at the index share a start; this model takes
    the case where the new clip comes first. In the other case nothing moves
    at all and the two clips lie on top of each other. -/
def panelDrop (n : Clip) (i : Nat) (tr : Track) : Track :=
  let n' := { n with start := panelPos n.len i tr }
  tr.take i ++ n' :: makeWay n.len n' (tr.drop i)

/-- It can leave an overlap. A four tick clip dropped in front of two one
    tick clips: the first jumps behind the new clip, the second no longer
    touches anything and stays underneath it. -/
example :
    panelDrop ⟨9, 0, 0, 4⟩ 0 [⟨1, 0, 0, 1⟩, ⟨2, 1, 0, 1⟩] =
      [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 1⟩, ⟨2, 1, 0, 1⟩] := by decide

theorem panelDrop_overlaps :
    ¬ OverlapFree (panelDrop ⟨9, 0, 0, 4⟩ 0 [⟨1, 0, 0, 1⟩, ⟨2, 1, 0, 1⟩]) := by decide

/-- It also swapped the two old clips. Push on the same track keeps their
    order and leaves no overlap. -/
example : push ⟨9, 0, 0, 4⟩ [⟨1, 0, 0, 1⟩, ⟨2, 1, 0, 1⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 1⟩, ⟨2, 5, 0, 1⟩] := by decide

/-- It jumps. With a gap of three ticks behind the first clip the second clip
    moves four ticks and the gap survives; with a gap of four ticks it does
    not move and the gap is gone. Starting one tick later, the clip ends up
    three ticks earlier. -/
example : panelDrop ⟨9, 0, 0, 4⟩ 0 [⟨1, 0, 0, 4⟩, ⟨2, 7, 0, 4⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 4⟩, ⟨2, 11, 0, 4⟩] := by decide

example : panelDrop ⟨9, 0, 0, 4⟩ 0 [⟨1, 0, 0, 4⟩, ⟨2, 8, 0, 4⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 4⟩, ⟨2, 8, 0, 4⟩] := by decide

/-- Neither rule jumps. Ripple keeps the gap in both cases, Push uses it up
    in both. -/
example : ripple ⟨9, 0, 0, 4⟩ [⟨1, 0, 0, 4⟩, ⟨2, 7, 0, 4⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 4⟩, ⟨2, 11, 0, 4⟩] := by decide

example : ripple ⟨9, 0, 0, 4⟩ [⟨1, 0, 0, 4⟩, ⟨2, 8, 0, 4⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 4⟩, ⟨2, 12, 0, 4⟩] := by decide

example : push ⟨9, 0, 0, 4⟩ [⟨1, 0, 0, 4⟩, ⟨2, 7, 0, 4⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 4⟩, ⟨2, 8, 0, 4⟩] := by decide

example : push ⟨9, 0, 0, 4⟩ [⟨1, 0, 0, 4⟩, ⟨2, 8, 0, 4⟩] =
    [⟨9, 0, 0, 4⟩, ⟨1, 4, 0, 4⟩, ⟨2, 8, 0, 4⟩] := by decide

/-! ### Reordering playlist rows: `movePlayListItemBefore`

The dragged clip takes the start of the item it is dropped in front of, and
that item and everything behind it move by the dragged clip's length. The
place the clip came from stays empty. In the model: Ripple for the drop,
leave-gap for the pick-up. -/

def reorderBefore (x : Clip) (targetStart : Nat) (tr : Track) : Track :=
  ripple { x with start := targetStart } (leaveGap x.id tr)

/-- Three clips edge to edge, the first dragged in front of the third. The
    track now starts with a hole and is two ticks longer. -/
example : reorderBefore ⟨1, 0, 0, 2⟩ 4 [⟨1, 0, 0, 2⟩, ⟨2, 2, 0, 2⟩, ⟨3, 4, 0, 2⟩] =
    [⟨2, 2, 0, 2⟩, ⟨1, 4, 0, 2⟩, ⟨3, 6, 0, 2⟩] := by decide

/-- With the matching pick-up (close the hole, then Ripple in front of the
    third clip, which now starts at tick 2) the clips stay edge to edge. -/
example :
    ripple ⟨1, 2, 0, 2⟩ (closeGap 1 [⟨1, 0, 0, 2⟩, ⟨2, 2, 0, 2⟩, ⟨3, 4, 0, 2⟩]) =
      [⟨2, 0, 0, 2⟩, ⟨1, 2, 0, 2⟩, ⟨3, 4, 0, 2⟩] := by decide

/-! ### Paste and duplicate: `findNextFreePosition` -/

/-- While some clip contains the position, jump to that clip's end. -/
def nextFree : Nat → Nat → Track → Nat
  | 0, p, _ => p
  | fuel + 1, p, tr =>
    match tr.find? fun c => decide (c.start ≤ p ∧ p < c.stop) with
    | some c => nextFree fuel c.stop tr
    | none => p

def paste (n : Clip) (tr : Track) : Track :=
  layer { n with start := nextFree (tr.length + 1) n.start tr } tr

/-- The start is free but the clip does not fit: four ticks pasted at tick 1
    go to tick 2 and run into the clip at tick 3. -/
theorem paste_overlaps :
    ¬ OverlapFree (paste ⟨9, 1, 0, 4⟩ [⟨1, 0, 0, 2⟩, ⟨2, 3, 0, 2⟩]) := by decide

/-- Relocate looks at the whole length and goes to tick 5. -/
example : relocate ⟨9, 1, 0, 4⟩ [⟨1, 0, 0, 2⟩, ⟨2, 3, 0, 2⟩] =
    [⟨9, 5, 0, 4⟩, ⟨1, 0, 0, 2⟩, ⟨2, 3, 0, 2⟩] := by decide

end Timeline
