/-
  Timeline model: clips, tracks, and what a track plays.

  Time is counted in ticks (natural numbers). A clip is a window of `len`
  ticks into some source material, starting at source tick `src`, placed on
  the timeline at tick `start`. Speed, fades and gain are left out: only the
  extent on the timeline matters for placement conflicts.
-/

namespace Timeline

structure Clip where
  id : Nat
  start : Nat
  src : Nat
  len : Nat
deriving DecidableEq, Repr

namespace Clip

/-- First tick after the clip. The clip covers `[start, stop)`. -/
def stop (c : Clip) : Nat := c.start + c.len

/-- Two clips share at least one tick. An empty clip covers no tick, so it
    overlaps nothing. -/
def Overlaps (a b : Clip) : Prop :=
  0 < a.len ∧ 0 < b.len ∧ a.start < b.stop ∧ b.start < a.stop

theorem Overlaps.symm {a b : Clip} (h : a.Overlaps b) : b.Overlaps a := by
  unfold Overlaps at *; omega

instance (a b : Clip) : Decidable (a.Overlaps b) := by
  unfold Overlaps; infer_instance

/-- The clip plays source tick `k` of material `id` at timeline tick `t`. -/
def Plays (c : Clip) (id k t : Nat) : Prop :=
  c.id = id ∧ c.src ≤ k ∧ k < c.src + c.len ∧ t + c.src = c.start + k

/-- The same clip, `d` ticks later. -/
def shift (d : Nat) (c : Clip) : Clip := { c with start := c.start + d }

/-- The same clip, `d` ticks earlier. -/
def unshift (d : Nat) (c : Clip) : Clip := { c with start := c.start - d }

@[simp] theorem unshift_shift (d : Nat) (c : Clip) : (c.shift d).unshift d = c := by
  cases c; simp [shift, unshift]

end Clip

/-- A track is a list of clips. Where order matters it is the order on the
    timeline, see `Seq`. -/
abbrev Track := List Clip

/-- What a track means: which source tick of which material sounds at which
    timeline tick. Every rule below is judged by what it does to this
    relation, so cutting a clip in two without moving anything changes
    nothing. -/
def Plays (tr : Track) (id k t : Nat) : Prop := ∃ c ∈ tr, c.Plays id k t

@[simp] theorem plays_nil (id k t : Nat) : ¬ Plays [] id k t := by
  simp [Plays]

@[simp] theorem plays_cons (c : Clip) (tr : Track) (id k t : Nat) :
    Plays (c :: tr) id k t ↔ c.Plays id k t ∨ Plays tr id k t := by
  simp [Plays]

@[simp] theorem plays_append (a b : Track) (id k t : Nat) :
    Plays (a ++ b) id k t ↔ Plays a id k t ∨ Plays b id k t := by
  simp [Plays, or_and_right, exists_or]

/-- No two different clips of the track share a tick. -/
def OverlapFree (tr : Track) : Prop :=
  ∀ a ∈ tr, ∀ b ∈ tr, a.Overlaps b → a = b

/-- Every clip id occurs once. -/
def UniqueIds (tr : Track) : Prop :=
  ∀ a ∈ tr, ∀ b ∈ tr, a.id = b.id → a = b

/-- The id of `n` is not used on the track. -/
def Fresh (n : Clip) (tr : Track) : Prop := ∀ c ∈ tr, c.id ≠ n.id

/-- The track in timeline order with no overlap: every clip ends before any
    later one starts. Gaps are allowed. -/
def Seq (tr : Track) : Prop := List.Pairwise (fun a b => a.stop ≤ b.start) tr

@[simp] theorem seq_nil : Seq [] := List.Pairwise.nil

@[simp] theorem seq_cons (c : Clip) (tr : Track) :
    Seq (c :: tr) ↔ (∀ d ∈ tr, c.stop ≤ d.start) ∧ Seq tr := List.pairwise_cons

/-- A sequence is overlap free. -/
theorem Seq.overlapFree {tr : Track} (h : Seq tr) : OverlapFree tr := by
  induction tr with
  | nil => intro a ha; cases ha
  | cons c rest ih =>
    rw [seq_cons] at h
    intro a ha b hb hab
    simp only [List.mem_cons] at ha hb
    unfold Clip.Overlaps Clip.stop at hab
    rcases ha with rfl | ha <;> rcases hb with rfl | hb
    · rfl
    · have := h.1 b hb; unfold Clip.stop at this; omega
    · have := h.1 a ha; unfold Clip.stop at this; omega
    · exact ih h.2 a ha b hb hab

end Timeline
