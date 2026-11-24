/-
  Two views of the same track, and what each kind of drop addresses.

  The timeline view gives every clip a start tick. The sequence view lists
  the clips in order, each with the gap in front of it: positive is silence,
  zero is a butt joint, negative is an overlap. The two views carry the same
  information.

  A drop on a track lane is addressed by time. A drop into the playlist list
  is addressed by position in the sequence: "before this item" or "at the
  end". The second kind never lands inside a clip, so Ripple needs no cut
  there and can always be taken back clip for clip.
-/
import Timeline.Reversibility

namespace Timeline

/-- One entry of the sequence view: the gap in front of the clip, then the
    clip without its position. -/
structure Item where
  gap : Int
  id : Nat
  src : Nat
  len : Nat
deriving DecidableEq, Repr

/-- Timeline view to sequence view, measuring the first gap from `cur`. -/
def toSeq (cur : Nat) : Track → List Item
  | [] => []
  | c :: rest => ⟨(c.start : Int) - cur, c.id, c.src, c.len⟩ :: toSeq (c.start + c.len) rest

/-- Sequence view to timeline view, laying the items out from `cur`. -/
def ofSeq (cur : Nat) : List Item → Track
  | [] => []
  | i :: rest =>
    ⟨i.id, (cur + i.gap).toNat, i.src, i.len⟩ :: ofSeq ((cur + i.gap).toNat + i.len) rest

/-- No overlap may reach back before tick zero. -/
def Valid (cur : Nat) : List Item → Prop
  | [] => True
  | i :: rest => 0 ≤ cur + i.gap ∧ Valid ((cur + i.gap).toNat + i.len) rest

theorem ofSeq_toSeq (cur : Nat) (tr : Track) : ofSeq cur (toSeq cur tr) = tr := by
  induction tr generalizing cur with
  | nil => rfl
  | cons c rest ih =>
    have e : ((cur : Int) + ((c.start : Int) - cur)).toNat = c.start := by omega
    simp only [toSeq, ofSeq, e, ih]

theorem toSeq_ofSeq (cur : Nat) (s : List Item) (h : Valid cur s) :
    toSeq cur (ofSeq cur s) = s := by
  induction s generalizing cur with
  | nil => rfl
  | cons i rest ih =>
    obtain ⟨h0, hrest⟩ := h
    have e : (((cur + i.gap).toNat : Nat) : Int) - cur = i.gap := by omega
    simp only [toSeq, ofSeq, e, ih _ hrest]

/-- A sequence without negative gaps is a track in order without overlap. -/
theorem seq_ofSeq (cur : Nat) (s : List Item) (h : ∀ i ∈ s, 0 ≤ i.gap) :
    Seq (ofSeq cur s) ∧ ∀ c ∈ ofSeq cur s, cur ≤ c.start := by
  induction s generalizing cur with
  | nil => simp [ofSeq]
  | cons i rest ih =>
    have hi := h i List.mem_cons_self
    obtain ⟨ihS, ihB⟩ := ih ((cur + i.gap).toNat + i.len)
      (fun j hj => h j (List.mem_cons_of_mem _ hj))
    simp only [ofSeq]
    refine ⟨(seq_cons _ _).mpr ⟨?_, ihS⟩, ?_⟩
    · intro d hd
      have := ihB d hd
      simp only [Clip.stop]; omega
    · intro c hc
      rcases List.mem_cons.mp hc with rfl | hc
      · simp only; omega
      · have := ihB c hc; omega

/-- The sequence view does not see a shift of the whole track. -/
theorem toSeq_shift (cur d : Nat) (l : Track) :
    toSeq (cur + d) (l.map (Clip.shift d)) = toSeq cur l := by
  induction l generalizing cur with
  | nil => rfl
  | cons c rest ih =>
    have e : (((c.start + d : Nat) : Int) - ((cur + d : Nat) : Int)) = (c.start : Int) - cur := by
      omega
    have e' : c.start + d + c.len = c.start + c.len + d := by omega
    simp only [List.map_cons, toSeq, Clip.shift, e, e', ih]

/-- Ripple in front of a clip is plain list insertion in the sequence view:
    the new item takes over the gap that was in front of the clip, the clip
    follows it directly, and the rest of the sequence is untouched. -/
theorem toSeq_ripple_before {n c : Clip} {rest : Track} (cur : Nat)
    (hat : n.start = c.start) (hc : 0 < c.len) :
    toSeq cur (ripple n (c :: rest)) =
      ⟨(c.start : Int) - cur, n.id, n.src, n.len⟩ :: ⟨0, c.id, c.src, c.len⟩ ::
        toSeq (c.start + c.len) rest := by
  have h1 : ¬ c.stop ≤ n.start := by simp only [Clip.stop]; omega
  have h2 : n.start ≤ c.start := by omega
  unfold ripple
  rw [openGap_at h1 h2]
  have e' : c.start + n.len + c.len = c.start + c.len + n.len := by omega
  simp only [List.singleton_append, List.map_cons, toSeq, Clip.shift, e', toSeq_shift, hat,
    Int.sub_self]

/-! ### Drops addressed by position in the sequence -/

/-- Drop before the item at index `i`, or at the end of the track when there
    is no such item. The drop position is that item's start. -/
def dropAtIndex (n : Clip) (i : Nat) (tr : Track) : Track :=
  ripple { n with start := (tr[i]?.map Clip.start).getD (trackEnd tr) } tr

/-- A drop addressed by index cuts nothing and is undone exactly by removing
    the clip and closing the hole. -/
theorem closeGap_dropAtIndex {n : Clip} {tr : Track} (i : Nat) (h : Seq tr) (hf : Fresh n tr) :
    closeGap n.id (dropAtIndex n i tr) = tr := by
  unfold dropAtIndex
  refine closeGap_ripple
    (n := { n with start := (tr[i]?.map Clip.start).getD (trackEnd tr) }) hf ?_
  intro c hc
  cases hi : tr[i]? with
  | none =>
    left
    simpa using le_trackEnd hc
  | some x =>
    have hx : x ∈ tr := List.mem_of_getElem? hi
    simpa using seq_no_straddle h hx c hc

end Timeline
