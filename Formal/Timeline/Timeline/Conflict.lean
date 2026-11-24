/-
  Theorem 1. A drop that lands on existing material cannot keep material,
  position, separation and fidelity all at once. Dropping any one of the four
  makes the other three satisfiable; the witnesses are Layer, Overwrite and
  Relocate here, and the Shift rules in `Shift.lean`.
-/
import Timeline.Properties

namespace Timeline

/-- The four-way conflict. -/
theorem conflict {tr tr' : Track} {n c : Clip}
    (hc : c ∈ tr) (hid : c.id ≠ n.id) (hov : c.Overlaps n)
    (hM : Material tr tr') (hP : Position tr tr')
    (hS : Separation tr') (hF : Fidelity n tr') : False := by
  unfold Clip.Overlaps Clip.stop at hov
  -- a tick `t` that both `c` and the dropped clip cover, playing source tick `k` of `c`
  obtain ⟨t, ht⟩ : ∃ t, c.start ≤ t ∧ n.start ≤ t ∧ t < c.start + c.len ∧ t < n.start + n.len :=
    ⟨c.start + (n.start - c.start), by omega⟩
  obtain ⟨k, hk⟩ : ∃ k, k = c.src + (t - c.start) := ⟨_, rfl⟩
  have hplay : Plays tr c.id k t := ⟨c, hc, rfl, by omega, by omega, by omega⟩
  obtain ⟨t', ht'⟩ := hM _ _ _ hplay
  have : t' = t := hP _ _ _ _ hplay ht'
  subst this
  obtain ⟨c', hc', hid', h1, h2, h3⟩ := ht'
  have hov' : c'.Overlaps n := by
    unfold Clip.Overlaps Clip.stop; omega
  have := hS c' hc' n hF hov'
  subst this
  exact hid hid'.symm

/-! ### Layer: give up separation -/

/-- Put the clip where it was dropped and touch nothing else. -/
def layer (n : Clip) (tr : Track) : Track := n :: tr

theorem layer_material (n : Clip) (tr : Track) : Material tr (layer n tr) := by
  intro id k t h; exact ⟨t, by simp [layer, h]⟩

theorem layer_position {n : Clip} {tr : Track} (hu : UniqueIds tr) (hf : Fresh n tr) :
    Position tr (layer n tr) := by
  intro id k t t' ⟨c, hc, h⟩ h'
  simp only [layer, plays_cons] at h'
  unfold Clip.Plays at h
  rcases h' with h' | ⟨c', hc', h'⟩
  · unfold Clip.Plays at h'; exact absurd (h.1.trans h'.1.symm) (hf c hc)
  · unfold Clip.Plays at h'
    have := hu c hc c' hc' (h.1.trans h'.1.symm)
    subst this; omega

theorem layer_fidelity (n : Clip) (tr : Track) : Fidelity n (layer n tr) := by
  simp [Fidelity, layer]

/-! ### Overwrite: give up material -/

/-- The part of the clip before tick `t`, where it was. Written with
    truncated subtraction instead of `min`, which `omega` reads directly. -/
def Clip.keepUntil (x : Clip) (t : Nat) : Clip := { x with len := x.len - (x.stop - t) }

/-- The part of the clip from tick `t` on, where it was. -/
def Clip.keepFrom (x : Clip) (t : Nat) : Clip :=
  { x with start := x.start + (t - x.start), src := x.src + (t - x.start),
           len := x.len - (t - x.start) }

/-- What is left of `x` once the range of `n` is cut out of it: nothing, one
    piece, or a piece on either side. The pieces stay where they were. -/
def cut (n x : Clip) : List Clip :=
  (if x.start < n.start then [x.keepUntil n.start] else []) ++
  (if n.stop < x.stop then [x.keepFrom n.stop] else [])

/-- The dropped clip replaces whatever it covers. -/
def overwrite (n : Clip) (tr : Track) : Track := n :: tr.flatMap (cut n)

theorem mem_cut {n x p : Clip} (h : p ∈ cut n x) :
    (x.start < n.start ∧ p = x.keepUntil n.start) ∨
    (n.stop < x.stop ∧ p = x.keepFrom n.stop) := by
  unfold cut at h
  rw [List.mem_append] at h
  rcases h with h | h
  · left; split at h <;> simp_all
  · right; split at h <;> simp_all

/-- A piece is part of the clip it was cut from, at the same place. -/
theorem cut_sub {n x p : Clip} (h : p ∈ cut n x) :
    p.id = x.id ∧ x.start ≤ p.start ∧ p.stop ≤ x.stop ∧ p.start + x.src = x.start + p.src ∧
    (p.stop ≤ n.start ∨ n.stop ≤ p.start) := by
  rcases mem_cut h with ⟨h1, rfl⟩ | ⟨h1, rfl⟩ <;>
    simp only [Clip.stop, Clip.keepUntil, Clip.keepFrom, true_and] at h1 ⊢ <;> omega

theorem overwrite_position {n : Clip} {tr : Track} (hu : UniqueIds tr) (hf : Fresh n tr) :
    Position tr (overwrite n tr) := by
  intro id k t t' ⟨c, hc, h⟩ h'
  simp only [overwrite, plays_cons] at h'
  unfold Clip.Plays at h
  rcases h' with h' | ⟨p, hp, h'⟩
  · unfold Clip.Plays at h'; exact absurd (h.1.trans h'.1.symm) (hf c hc)
  · obtain ⟨x, hx, hpx⟩ := List.mem_flatMap.mp hp
    have hs := cut_sub hpx
    unfold Clip.Plays at h'
    have := hu c hc x hx (by omega)
    subst this
    simp only [Clip.stop] at hs; omega

theorem overwrite_separation {n : Clip} {tr : Track} (h : OverlapFree tr) :
    Separation (overwrite n tr) := by
  intro a ha b hb hab
  simp only [overwrite, List.mem_cons, List.mem_flatMap] at ha hb
  unfold Clip.Overlaps at hab
  rcases ha with rfl | ⟨x, hx, hax⟩ <;> rcases hb with rfl | ⟨y, hy, hby⟩
  · rfl
  · have := cut_sub hby; simp only [Clip.stop] at *; omega
  · have := cut_sub hax; simp only [Clip.stop] at *; omega
  · have ha' := cut_sub hax
    have hb' := cut_sub hby
    have hxy : x = y := h x hx y hy (by unfold Clip.Overlaps; simp only [Clip.stop] at *; omega)
    subst hxy
    rcases mem_cut hax with ⟨h1, rfl⟩ | ⟨h1, rfl⟩ <;> rcases mem_cut hby with ⟨h2, rfl⟩ | ⟨h2, rfl⟩
    · rfl
    · simp only [Clip.stop, Clip.keepUntil, Clip.keepFrom] at *; omega
    · simp only [Clip.stop, Clip.keepUntil, Clip.keepFrom] at *; omega
    · rfl

theorem overwrite_fidelity (n : Clip) (tr : Track) : Fidelity n (overwrite n tr) := by
  simp [Fidelity, overwrite]

/-- Overwrite loses only what the dropped clip covers. -/
theorem overwrite_keeps_uncovered {n : Clip} {tr : Track} {id k t : Nat}
    (h : Plays tr id k t) (ht : t < n.start ∨ n.stop ≤ t) : Plays (overwrite n tr) id k t := by
  obtain ⟨c, hc, h⟩ := h
  unfold Clip.Plays at h
  simp only [overwrite, plays_cons]
  right
  by_cases hl : t < n.start
  · refine ⟨c.keepUntil n.start, ?_, ?_⟩
    · refine List.mem_flatMap.mpr ⟨c, hc, ?_⟩
      unfold cut; rw [List.mem_append]; left
      simp [show c.start < n.start by omega]
    · unfold Clip.Plays; simp only [Clip.stop, Clip.keepUntil]; omega
  · have hr : n.stop ≤ t := by omega
    refine ⟨c.keepFrom n.stop, ?_, ?_⟩
    · refine List.mem_flatMap.mpr ⟨c, hc, ?_⟩
      unfold cut; rw [List.mem_append]; right
      simp [show n.stop < c.stop by simp only [Clip.stop] at *; omega]
    · unfold Clip.Plays; simp only [Clip.stop, Clip.keepFrom] at *; omega

/-! ### Relocate: give up fidelity -/

/-- Does the clip fit at tick `s` without touching anything? -/
def fits (n : Clip) (tr : Track) (s : Nat) : Bool :=
  tr.all fun c => !decide (c.Overlaps { n with start := s })

/-- Last tick used on the track. -/
def trackEnd : Track → Nat
  | [] => 0
  | c :: rest => c.stop + (trackEnd rest - c.stop)

theorem le_trackEnd {tr : Track} {c : Clip} (h : c ∈ tr) : c.stop ≤ trackEnd tr := by
  induction tr with
  | nil => cases h
  | cons d rest ih =>
    simp only [List.mem_cons] at h
    unfold trackEnd
    rcases h with rfl | h
    · omega
    · have := ih h; omega

/-- First tick at or after the drop position where the clip fits. Only the
    drop position itself and the ends of clips need trying. -/
def freeStart (n : Clip) (tr : Track) : Nat :=
  (((n.start :: tr.map Clip.stop).filter fun s => n.start ≤ s && fits n tr s).head?).getD
    (n.start + (trackEnd tr - n.start))

/-- The clip goes to the nearest later place where it fits; nothing else
    changes. -/
def relocate (n : Clip) (tr : Track) : Track := { n with start := freeStart n tr } :: tr

theorem fits_trackEnd (n : Clip) (tr : Track) : fits n tr (n.start + (trackEnd tr - n.start)) = true := by
  unfold fits
  rw [List.all_eq_true]
  intro c hc
  have h := le_trackEnd hc
  have : ¬ c.Overlaps { n with start := n.start + (trackEnd tr - n.start) } := by
    simp only [Clip.Overlaps, Clip.stop] at h ⊢; omega
  simpa using this

theorem freeStart_spec (n : Clip) (tr : Track) :
    n.start ≤ freeStart n tr ∧ fits n tr (freeStart n tr) = true := by
  unfold freeStart
  cases hf : ((n.start :: tr.map Clip.stop).filter fun s => n.start ≤ s && fits n tr s).head? with
  | none => simp only [Option.getD_none]; exact ⟨by omega, fits_trackEnd n tr⟩
  | some s =>
    have hm := List.mem_of_mem_head? hf
    rw [List.mem_filter] at hm
    simpa using hm.2

theorem relocate_material (n : Clip) (tr : Track) : Material tr (relocate n tr) := by
  intro id k t h; exact ⟨t, by simp [relocate, h]⟩

theorem relocate_position {n : Clip} {tr : Track} (hu : UniqueIds tr) (hf : Fresh n tr) :
    Position tr (relocate n tr) :=
  layer_position (n := { n with start := freeStart n tr }) hu hf

theorem relocate_separation {n : Clip} {tr : Track} (h : OverlapFree tr) :
    Separation (relocate n tr) := by
  have hfit := (freeStart_spec n tr).2
  unfold fits at hfit
  rw [List.all_eq_true] at hfit
  intro a ha b hb hab
  simp only [relocate, List.mem_cons] at ha hb
  rcases ha with rfl | ha <;> rcases hb with rfl | hb
  · rfl
  · have := hfit b hb
    simp only [Bool.not_eq_true', decide_eq_false_iff_not] at this
    exact absurd hab.symm this
  · have := hfit a ha
    simp only [Bool.not_eq_true', decide_eq_false_iff_not] at this
    exact absurd hab this
  · exact h a ha b hb hab

/-- Relocate never moves the clip earlier, and leaves it alone when it fits. -/
theorem relocate_not_earlier (n : Clip) (tr : Track) : n.start ≤ freeStart n tr :=
  (freeStart_spec n tr).1

theorem relocate_of_fits {n : Clip} {tr : Track} (h : fits n tr n.start = true) :
    relocate n tr = layer n tr := by
  have : freeStart n tr = n.start := by
    unfold freeStart
    simp [h]
  simp [relocate, layer, this]

end Timeline
