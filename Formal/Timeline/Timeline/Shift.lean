/-
  The Shift family: keep material, separation and fidelity, let clips move.

  Two rules live here. Ripple opens a hole of the dropped length at the drop
  position and moves everything behind it by that length. Push moves a clip
  only as far as it must to get out of the way. Both cut a clip that the drop
  position falls inside; `Straddle.lean` shows that this cannot be avoided.
-/
import Timeline.Conflict

namespace Timeline

/-! ### Clips: shifting and cutting -/

namespace Clip

theorem Plays.bounds {c : Clip} {id k t : Nat} (h : c.Plays id k t) :
    c.start ≤ t ∧ t < c.stop := by
  simp only [Plays, stop] at *; omega

theorem plays_shift {c : Clip} {id k t : Nat} (L : Nat) (h : c.Plays id k t) :
    (c.shift L).Plays id k (t + L) := by
  simp only [Plays, shift] at *; omega

theorem plays_shift_inv {c : Clip} {id k t' L : Nat} (h : (c.shift L).Plays id k t') :
    ∃ t, c.Plays id k t ∧ t' = t + L :=
  ⟨t' - L, by simp only [Plays, shift] at *; omega⟩

/-- The part before `t` plays what the clip played before `t`. -/
theorem plays_keepUntil {c : Clip} {t id k u : Nat} :
    (c.keepUntil t).Plays id k u ↔ c.Plays id k u ∧ u < t := by
  simp only [Plays, keepUntil, stop]; omega

/-- The part from `t` on plays what the clip played from `t` on. -/
theorem plays_keepFrom {c : Clip} {t id k u : Nat} :
    (c.keepFrom t).Plays id k u ↔ c.Plays id k u ∧ t ≤ u := by
  simp only [Plays, keepFrom]; omega

end Clip

theorem plays_map_shift {l : Track} {L id k t' : Nat} :
    Plays (l.map (Clip.shift L)) id k t' ↔ ∃ t, Plays l id k t ∧ t' = t + L := by
  constructor
  · rintro ⟨c', hc', h⟩
    obtain ⟨c, hc, rfl⟩ := List.mem_map.mp hc'
    obtain ⟨t, ht, rfl⟩ := Clip.plays_shift_inv h
    exact ⟨t, ⟨c, hc, ht⟩, rfl⟩
  · rintro ⟨t, ⟨c, hc, h⟩, rfl⟩
    exact ⟨c.shift L, List.mem_map.mpr ⟨c, hc, rfl⟩, Clip.plays_shift L h⟩

/-- Nothing sounds before the earliest clip starts. -/
theorem plays_ge {l : Track} {b id k t : Nat} (hb : ∀ c ∈ l, b ≤ c.start)
    (h : Plays l id k t) : b ≤ t := by
  obtain ⟨c, hc, h⟩ := h
  have := hb c hc
  have := h.bounds
  omega

theorem seq_append {a b : Track} :
    Seq (a ++ b) ↔ Seq a ∧ Seq b ∧ ∀ x ∈ a, ∀ y ∈ b, x.stop ≤ y.start :=
  List.pairwise_append

theorem seq_map_shift {l : Track} (L : Nat) (h : Seq l) : Seq (l.map (Clip.shift L)) := by
  induction l with
  | nil => simp
  | cons c rest ih =>
    rw [seq_cons] at h
    rw [List.map_cons, seq_cons]
    refine ⟨?_, ih h.2⟩
    intro d hd
    obtain ⟨x, hx, rfl⟩ := List.mem_map.mp hd
    have := h.1 x hx
    simp only [Clip.shift, Clip.stop] at *; omega

/-! ### Ripple -/

/-- What gets dropped: clips in order, without overlap, inside `[t, t + L)`.
    One clip is the usual case; several regions laid end to end and an empty
    block (silence) fit as well. -/
def Block (ins : Track) (t L : Nat) : Prop :=
  Seq ins ∧ ∀ i ∈ ins, t ≤ i.start ∧ i.stop ≤ t + L

theorem block_single (n : Clip) : Block [n] n.start n.len := by
  refine ⟨by simp, ?_⟩
  intro i hi
  simp only [List.mem_singleton] at hi
  subst hi
  simp only [Clip.stop]; omega

theorem block_nil (t L : Nat) : Block [] t L := ⟨by simp, by simp⟩

/-- Open a hole of `L` ticks at tick `t` and put `ins` into it. Clips that
    end by `t` stay, clips that start at or after `t` move by `L`, and a clip
    that `t` falls inside is cut there. The track is expected in timeline
    order. -/
def openGap (ins : Track) (t L : Nat) : Track → Track
  | [] => ins
  | c :: rest =>
    if c.stop ≤ t then c :: openGap ins t L rest
    else if t ≤ c.start then ins ++ (c :: rest).map (Clip.shift L)
    else c.keepUntil t :: (ins ++ (c.keepFrom t :: rest).map (Clip.shift L))

/-- Ripple: insert the clip, moving everything from the drop position on. -/
def ripple (n : Clip) (tr : Track) : Track := openGap [n] n.start n.len tr

theorem openGap_before {ins : Track} {t L : Nat} {c : Clip} {rest : Track} (h : c.stop ≤ t) :
    openGap ins t L (c :: rest) = c :: openGap ins t L rest := by
  simp [openGap, h]

theorem openGap_at {ins : Track} {t L : Nat} {c : Clip} {rest : Track}
    (h1 : ¬ c.stop ≤ t) (h2 : t ≤ c.start) :
    openGap ins t L (c :: rest) = ins ++ (c :: rest).map (Clip.shift L) := by
  simp [openGap, h1, h2]

theorem openGap_split {ins : Track} {t L : Nat} {c : Clip} {rest : Track}
    (h1 : ¬ c.stop ≤ t) (h2 : ¬ t ≤ c.start) :
    openGap ins t L (c :: rest) =
      c.keepUntil t :: (ins ++ (c.keepFrom t :: rest).map (Clip.shift L)) := by
  simp [openGap, h1, h2]

theorem mem_openGap {ins : Track} {t L : Nat} {i : Clip} (tr : Track) (hi : i ∈ ins) :
    i ∈ openGap ins t L tr := by
  induction tr with
  | nil => simpa [openGap] using hi
  | cons c rest ih =>
    by_cases h1 : c.stop ≤ t
    · rw [openGap_before h1]; exact List.mem_cons_of_mem _ ih
    · by_cases h2 : t ≤ c.start
      · rw [openGap_at h1 h2]; exact List.mem_append_left _ hi
      · rw [openGap_split h1 h2]; exact List.mem_cons_of_mem _ (List.mem_append_left _ hi)

theorem seq_block_tail {ins l : Track} {t L : Nat} (hb : Block ins t L) (hl : Seq l)
    (hge : ∀ c ∈ l, t ≤ c.start) : Seq (ins ++ l.map (Clip.shift L)) := by
  rw [seq_append]
  refine ⟨hb.1, seq_map_shift L hl, ?_⟩
  intro x hx y hy
  obtain ⟨c, hc, rfl⟩ := List.mem_map.mp hy
  have := (hb.2 x hx).2
  have := hge c hc
  simp only [Clip.shift]; omega

theorem block_tail_ge {ins l : Track} {t L : Nat} (hb : Block ins t L)
    (hge : ∀ c ∈ l, t ≤ c.start) : ∀ d ∈ ins ++ l.map (Clip.shift L), t ≤ d.start := by
  intro d hd
  rcases List.mem_append.mp hd with hd | hd
  · exact (hb.2 d hd).1
  · obtain ⟨c, hc, rfl⟩ := List.mem_map.mp hd
    have := hge c hc
    simp only [Clip.shift]; omega

/-- Opening a gap keeps the track in order and overlap free. The second part
    is the induction's bookkeeping: nothing ends up earlier than before. -/
theorem openGap_seq {ins tr : Track} {t L : Nat} (hb : Block ins t L) (h : Seq tr) :
    Seq (openGap ins t L tr) ∧
    ∀ b, b ≤ t → (∀ c ∈ tr, b ≤ c.start) → ∀ d ∈ openGap ins t L tr, b ≤ d.start := by
  induction tr with
  | nil =>
    refine ⟨by simpa [openGap] using hb.1, ?_⟩
    intro b hbt _ d hd
    simp only [openGap] at hd
    have := (hb.2 d hd).1
    omega
  | cons c rest ih =>
    rw [seq_cons] at h
    obtain ⟨ihS, ihB⟩ := ih h.2
    by_cases h1 : c.stop ≤ t
    · rw [openGap_before h1]
      refine ⟨(seq_cons _ _).mpr ⟨ihB c.stop h1 h.1, ihS⟩, ?_⟩
      intro b hbt hbc d hd
      rcases List.mem_cons.mp hd with rfl | hd
      · exact hbc _ List.mem_cons_self
      · exact ihB b hbt (fun x hx => hbc x (List.mem_cons_of_mem _ hx)) d hd
    · by_cases h2 : t ≤ c.start
      · rw [openGap_at h1 h2]
        have hge : ∀ x ∈ c :: rest, t ≤ x.start := by
          intro x hx
          rcases List.mem_cons.mp hx with rfl | hx
          · exact h2
          · have := h.1 x hx; simp only [Clip.stop] at this; omega
        refine ⟨seq_block_tail hb ((seq_cons _ _).mpr h) hge, ?_⟩
        intro b hbt _ d hd
        have := block_tail_ge hb hge d hd
        omega
      · rw [openGap_split h1 h2]
        simp only [Clip.stop] at h1
        have hseq : Seq (c.keepFrom t :: rest) := by
          refine (seq_cons _ _).mpr ⟨?_, h.2⟩
          intro d hd
          have := h.1 d hd
          simp only [Clip.stop, Clip.keepFrom] at *; omega
        have hge : ∀ x ∈ c.keepFrom t :: rest, t ≤ x.start := by
          intro x hx
          rcases List.mem_cons.mp hx with rfl | hx
          · simp only [Clip.keepFrom]; omega
          · have := h.1 x hx; simp only [Clip.stop] at this; omega
        have hall := block_tail_ge hb hge
        refine ⟨(seq_cons _ _).mpr ⟨?_, seq_block_tail hb hseq hge⟩, ?_⟩
        · intro d hd
          have := hall d hd
          simp only [Clip.stop, Clip.keepUntil]; omega
        · intro b hbt hbc d hd
          rcases List.mem_cons.mp hd with rfl | hd
          · have := hbc c List.mem_cons_self
            simp only [Clip.keepUntil]; omega
          · have := hall d hd; omega

/-- When nothing of `l` sounds before `t`, "later by `L`" is the whole story. -/
theorem warp_of_ge {l : Track} {t L id k t' : Nat} (hge : ∀ u, Plays l id k u → t ≤ u) :
    (∃ u, Plays l id k u ∧ t' = u + L) ↔
    ∃ u, Plays l id k u ∧ ((u < t ∧ t' = u) ∨ (t ≤ u ∧ t' = u + L)) := by
  constructor
  · rintro ⟨u, hp, rfl⟩
    exact ⟨u, hp, Or.inr ⟨hge u hp, rfl⟩⟩
  · rintro ⟨u, hp, ⟨hlt, _⟩ | ⟨_, rfl⟩⟩
    · exact absurd (hge u hp) (by omega)
    · exact ⟨u, hp, rfl⟩

/-- What the track plays after a gap was opened: the inserted block, and
    everything it played before, unmoved before `t` and `L` later from `t`
    on. This one statement carries material, the kept past and the kept gaps. -/
theorem plays_openGap {ins tr : Track} {t L : Nat} (h : Seq tr) (id k t' : Nat) :
    Plays (openGap ins t L tr) id k t' ↔
    Plays ins id k t' ∨
      ∃ u, Plays tr id k u ∧ ((u < t ∧ t' = u) ∨ (t ≤ u ∧ t' = u + L)) := by
  induction tr with
  | nil => simp [openGap]
  | cons c rest ih =>
    rw [seq_cons] at h
    by_cases h1 : c.stop ≤ t
    · rw [openGap_before h1, plays_cons, ih h.2]
      constructor
      · rintro (hc | hi | ⟨u, hr, hu⟩)
        · have := hc.bounds
          exact Or.inr ⟨t', (plays_cons _ _ _ _ _).mpr (Or.inl hc), Or.inl ⟨by omega, rfl⟩⟩
        · exact Or.inl hi
        · exact Or.inr ⟨u, (plays_cons _ _ _ _ _).mpr (Or.inr hr), hu⟩
      · rintro (hi | ⟨u, hp, hu⟩)
        · exact Or.inr (Or.inl hi)
        · rcases (plays_cons _ _ _ _ _).mp hp with hc | hr
          · have := hc.bounds
            rcases hu with ⟨_, rfl⟩ | ⟨_, rfl⟩
            · exact Or.inl hc
            · omega
          · exact Or.inr (Or.inr ⟨u, hr, hu⟩)
    · have hrest : ∀ u, Plays rest id k u → c.stop ≤ u := fun u hu => plays_ge h.1 hu
      by_cases h2 : t ≤ c.start
      · rw [openGap_at h1 h2, plays_append, plays_map_shift]
        have hge : ∀ u, Plays (c :: rest) id k u → t ≤ u := by
          intro u hu
          rcases (plays_cons _ _ _ _ _).mp hu with hc | hr
          · have := hc.bounds; omega
          · have := hrest u hr; simp only [Clip.stop] at this; omega
        rw [warp_of_ge hge]
      · rw [openGap_split h1 h2, plays_cons, plays_append, plays_map_shift]
        simp only [Clip.stop] at h1 hrest
        constructor
        · rintro (hc | hi | ⟨u, hp, rfl⟩)
          · rw [Clip.plays_keepUntil] at hc
            exact Or.inr ⟨t', (plays_cons _ _ _ _ _).mpr (Or.inl hc.1), Or.inl ⟨hc.2, rfl⟩⟩
          · exact Or.inl hi
          · rcases (plays_cons _ _ _ _ _).mp hp with hc | hr
            · rw [Clip.plays_keepFrom] at hc
              exact Or.inr ⟨u, (plays_cons _ _ _ _ _).mpr (Or.inl hc.1), Or.inr ⟨hc.2, rfl⟩⟩
            · have := hrest u hr
              exact Or.inr ⟨u, (plays_cons _ _ _ _ _).mpr (Or.inr hr), Or.inr ⟨by omega, rfl⟩⟩
        · rintro (hi | ⟨u, hp, hu⟩)
          · exact Or.inr (Or.inl hi)
          · rcases (plays_cons _ _ _ _ _).mp hp with hc | hr
            · rcases hu with ⟨hlt, rfl⟩ | ⟨hle, rfl⟩
              · exact Or.inl (Clip.plays_keepUntil.mpr ⟨hc, hlt⟩)
              · exact Or.inr (Or.inr ⟨u, (plays_cons _ _ _ _ _).mpr
                  (Or.inl (Clip.plays_keepFrom.mpr ⟨hc, hle⟩)), rfl⟩)
            · have := hrest u hr
              rcases hu with ⟨hlt, rfl⟩ | ⟨hle, rfl⟩
              · omega
              · exact Or.inr (Or.inr ⟨u, (plays_cons _ _ _ _ _).mpr (Or.inr hr), rfl⟩)

/-! Ripple keeps everything except position. -/

theorem ripple_seq {n : Clip} {tr : Track} (h : Seq tr) : Seq (ripple n tr) :=
  (openGap_seq (block_single n) h).1

theorem ripple_separation {n : Clip} {tr : Track} (h : Seq tr) : Separation (ripple n tr) :=
  (ripple_seq h).overlapFree

theorem ripple_fidelity (n : Clip) (tr : Track) : Fidelity n (ripple n tr) :=
  mem_openGap tr List.mem_cons_self

theorem ripple_pastKept {n : Clip} {tr : Track} (h : Seq tr) : PastKept n tr (ripple n tr) := by
  intro id k t ht hp
  exact (plays_openGap h id k t).mpr (Or.inr ⟨t, hp, Or.inl ⟨ht, rfl⟩⟩)

theorem ripple_gapsKept {n : Clip} {tr : Track} (h : Seq tr) : GapsKept n tr (ripple n tr) := by
  intro id k t ht hp
  exact (plays_openGap h id k _).mpr (Or.inr ⟨t, hp, Or.inr ⟨ht, rfl⟩⟩)

theorem ripple_material {n : Clip} {tr : Track} (h : Seq tr) : Material tr (ripple n tr) := by
  intro id k t hp
  by_cases ht : t < n.start
  · exact ⟨t, ripple_pastKept h id k t ht hp⟩
  · exact ⟨t + n.len, ripple_gapsKept h id k t (by omega) hp⟩

/-- Ripple is the only rule that keeps the past and the gaps: any outcome
    that does so, holds the dropped clip and nothing new besides, and plays
    nothing twice, plays exactly what Ripple plays. -/
theorem ripple_unique {n : Clip} {tr tr' : Track} (h : Seq tr)
    (hpast : PastKept n tr tr') (hgaps : GapsKept n tr tr') (hfid : Fidelity n tr')
    (hfun : Functional tr')
    (honly : ∀ id k t', Plays tr' id k t' → n.Plays id k t' ∨ ∃ u, Plays tr id k u)
    (id k t' : Nat) : Plays tr' id k t' ↔ Plays (ripple n tr) id k t' := by
  unfold ripple
  rw [plays_openGap h]
  constructor
  · intro hp
    rcases honly id k t' hp with hn | ⟨u, hu⟩
    · exact Or.inl ⟨n, List.mem_cons_self, hn⟩
    · by_cases hlt : u < n.start
      · exact Or.inr ⟨u, hu, Or.inl ⟨hlt, hfun _ _ _ _ hp (hpast id k u hlt hu)⟩⟩
      · exact Or.inr ⟨u, hu, Or.inr ⟨by omega, hfun _ _ _ _ hp (hgaps id k u (by omega) hu)⟩⟩
  · rintro (⟨c, hc, hn⟩ | ⟨u, hu, ⟨hlt, rfl⟩ | ⟨hle, rfl⟩⟩)
    · simp only [List.mem_singleton] at hc
      subst hc
      exact ⟨c, hfid, hn⟩
    · exact hpast id k _ hlt hu
    · exact hgaps id k u hle hu

/-! ### Push -/

/-- Make room from tick `edge` on: a clip that starts too early moves up to
    `edge`, and the clip behind it then has to clear the moved one. The first
    clip that is already clear ends the cascade. -/
def pushRight (edge : Nat) : Track → Track
  | [] => []
  | c :: rest =>
    if edge ≤ c.start then c :: rest
    else { c with start := edge } :: pushRight (edge + c.len) rest

/-- Push: insert the clip, moving later clips only as far as needed. -/
def push (n : Clip) : Track → Track
  | [] => [n]
  | c :: rest =>
    if c.stop ≤ n.start then c :: push n rest
    else if n.start ≤ c.start then n :: pushRight n.stop (c :: rest)
    else c.keepUntil n.start :: n :: pushRight n.stop (c.keepFrom n.start :: rest)

theorem pushRight_keep {e : Nat} {c : Clip} {rest : Track} (h : e ≤ c.start) :
    pushRight e (c :: rest) = c :: rest := by
  simp [pushRight, h]

theorem pushRight_move {e : Nat} {c : Clip} {rest : Track} (h : ¬ e ≤ c.start) :
    pushRight e (c :: rest) = { c with start := e } :: pushRight (e + c.len) rest := by
  simp [pushRight, h]

theorem push_before {n c : Clip} {rest : Track} (h : c.stop ≤ n.start) :
    push n (c :: rest) = c :: push n rest := by
  simp [push, h]

theorem push_at {n c : Clip} {rest : Track} (h1 : ¬ c.stop ≤ n.start) (h2 : n.start ≤ c.start) :
    push n (c :: rest) = n :: pushRight n.stop (c :: rest) := by
  simp [push, h1, h2]

theorem push_split {n c : Clip} {rest : Track} (h1 : ¬ c.stop ≤ n.start)
    (h2 : ¬ n.start ≤ c.start) :
    push n (c :: rest) =
      c.keepUntil n.start :: n :: pushRight n.stop (c.keepFrom n.start :: rest) := by
  simp [push, h1, h2]

/-- After pushing, the track is in order, overlap free, and clear of `e`. -/
theorem pushRight_seq {l : Track} (e : Nat) (h : Seq l) :
    Seq (pushRight e l) ∧ ∀ d ∈ pushRight e l, e ≤ d.start := by
  induction l generalizing e with
  | nil => simp [pushRight]
  | cons c rest ih =>
    rw [seq_cons] at h
    by_cases he : e ≤ c.start
    · rw [pushRight_keep he]
      refine ⟨(seq_cons _ _).mpr h, ?_⟩
      intro d hd
      rcases List.mem_cons.mp hd with rfl | hd
      · exact he
      · have := h.1 d hd; simp only [Clip.stop] at this; omega
    · rw [pushRight_move he]
      obtain ⟨ihS, ihB⟩ := ih (e + c.len) h.2
      refine ⟨(seq_cons _ _).mpr ⟨?_, ihS⟩, ?_⟩
      · intro d hd
        have := ihB d hd
        simp only [Clip.stop]; omega
      · intro d hd
        rcases List.mem_cons.mp hd with rfl | hd
        · simp
        · have := ihB d hd; omega

/-- Pushing loses nothing and moves nothing earlier. -/
theorem plays_pushRight {l : Track} (e : Nat) {id k t : Nat} (h : Plays l id k t) :
    ∃ t', t ≤ t' ∧ Plays (pushRight e l) id k t' := by
  induction l generalizing e with
  | nil => exact absurd h (plays_nil _ _ _)
  | cons c rest ih =>
    by_cases he : e ≤ c.start
    · rw [pushRight_keep he]; exact ⟨t, Nat.le_refl _, h⟩
    · rw [pushRight_move he]
      rcases (plays_cons _ _ _ _ _).mp h with hc | hr
      · refine ⟨t + (e - c.start), by omega, (plays_cons _ _ _ _ _).mpr (Or.inl ?_)⟩
        simp only [Clip.Plays] at *; omega
      · obtain ⟨t', ht', hp⟩ := ih (e + c.len) hr
        exact ⟨t', ht', (plays_cons _ _ _ _ _).mpr (Or.inr hp)⟩

theorem push_seq {n : Clip} {tr : Track} (h : Seq tr) :
    Seq (push n tr) ∧
    ∀ b, b ≤ n.start → (∀ c ∈ tr, b ≤ c.start) → ∀ d ∈ push n tr, b ≤ d.start := by
  induction tr with
  | nil =>
    refine ⟨by simp [push], ?_⟩
    intro b hb _ d hd
    simp only [push, List.mem_singleton] at hd
    subst hd; exact hb
  | cons c rest ih =>
    rw [seq_cons] at h
    obtain ⟨ihS, ihB⟩ := ih h.2
    by_cases h1 : c.stop ≤ n.start
    · rw [push_before h1]
      refine ⟨(seq_cons _ _).mpr ⟨ihB c.stop h1 h.1, ihS⟩, ?_⟩
      intro b hbt hbc d hd
      rcases List.mem_cons.mp hd with rfl | hd
      · exact hbc _ List.mem_cons_self
      · exact ihB b hbt (fun x hx => hbc x (List.mem_cons_of_mem _ hx)) d hd
    · by_cases h2 : n.start ≤ c.start
      · rw [push_at h1 h2]
        obtain ⟨pS, pB⟩ := pushRight_seq n.stop ((seq_cons _ _).mpr h)
        refine ⟨(seq_cons _ _).mpr ⟨pB, pS⟩, ?_⟩
        intro b hbt _ d hd
        rcases List.mem_cons.mp hd with rfl | hd
        · exact hbt
        · have := pB d hd; simp only [Clip.stop] at this; omega
      · rw [push_split h1 h2]
        simp only [Clip.stop] at h1
        have hseq : Seq (c.keepFrom n.start :: rest) := by
          refine (seq_cons _ _).mpr ⟨?_, h.2⟩
          intro d hd
          have := h.1 d hd
          simp only [Clip.stop, Clip.keepFrom] at *; omega
        obtain ⟨pS, pB⟩ := pushRight_seq n.stop hseq
        refine ⟨(seq_cons _ _).mpr ⟨?_, (seq_cons _ _).mpr ⟨pB, pS⟩⟩, ?_⟩
        · intro d hd
          rcases List.mem_cons.mp hd with rfl | hd
          · simp only [Clip.stop, Clip.keepUntil]; omega
          · have := pB d hd; simp only [Clip.stop, Clip.keepUntil] at *; omega
        · intro b hbt hbc d hd
          have := hbc c List.mem_cons_self
          rcases List.mem_cons.mp hd with rfl | hd
          · simp only [Clip.keepUntil]; omega
          · rcases List.mem_cons.mp hd with rfl | hd
            · exact hbt
            · have := pB d hd; simp only [Clip.stop] at this; omega

theorem push_separation {n : Clip} {tr : Track} (h : Seq tr) : Separation (push n tr) :=
  (push_seq h).1.overlapFree

theorem push_fidelity (n : Clip) (tr : Track) : Fidelity n (push n tr) := by
  unfold Fidelity
  induction tr with
  | nil => simp [push]
  | cons c rest ih =>
    by_cases h1 : c.stop ≤ n.start
    · rw [push_before h1]; exact List.mem_cons_of_mem _ ih
    · by_cases h2 : n.start ≤ c.start
      · rw [push_at h1 h2]; exact List.mem_cons_self
      · rw [push_split h1 h2]; exact List.mem_cons_of_mem _ List.mem_cons_self

/-- Push loses nothing and moves nothing earlier. -/
theorem plays_push {n : Clip} {tr : Track} {id k t : Nat} (h : Plays tr id k t) :
    ∃ t', t ≤ t' ∧ Plays (push n tr) id k t' := by
  induction tr with
  | nil => exact absurd h (plays_nil _ _ _)
  | cons c rest ih =>
    by_cases h1 : c.stop ≤ n.start
    · rw [push_before h1]
      rcases (plays_cons _ _ _ _ _).mp h with hc | hr
      · exact ⟨t, Nat.le_refl _, (plays_cons _ _ _ _ _).mpr (Or.inl hc)⟩
      · obtain ⟨t', ht', hp⟩ := ih hr
        exact ⟨t', ht', (plays_cons _ _ _ _ _).mpr (Or.inr hp)⟩
    · by_cases h2 : n.start ≤ c.start
      · rw [push_at h1 h2]
        obtain ⟨t', ht', hp⟩ := plays_pushRight n.stop h
        exact ⟨t', ht', (plays_cons _ _ _ _ _).mpr (Or.inr hp)⟩
      · rw [push_split h1 h2]
        by_cases hlt : t < n.start
        · rcases (plays_cons _ _ _ _ _).mp h with hc | hr
          · exact ⟨t, Nat.le_refl _, (plays_cons _ _ _ _ _).mpr
              (Or.inl (Clip.plays_keepUntil.mpr ⟨hc, hlt⟩))⟩
          · have hp : Plays (c.keepFrom n.start :: rest) id k t :=
              (plays_cons _ _ _ _ _).mpr (Or.inr hr)
            obtain ⟨t', ht', hp⟩ := plays_pushRight n.stop hp
            exact ⟨t', ht', (plays_cons _ _ _ _ _).mpr (Or.inr ((plays_cons _ _ _ _ _).mpr (Or.inr hp)))⟩
        · have hp : Plays (c.keepFrom n.start :: rest) id k t := by
            rcases (plays_cons _ _ _ _ _).mp h with hc | hr
            · exact (plays_cons _ _ _ _ _).mpr (Or.inl (Clip.plays_keepFrom.mpr ⟨hc, by omega⟩))
            · exact (plays_cons _ _ _ _ _).mpr (Or.inr hr)
          obtain ⟨t', ht', hp⟩ := plays_pushRight n.stop hp
          exact ⟨t', ht', (plays_cons _ _ _ _ _).mpr (Or.inr ((plays_cons _ _ _ _ _).mpr (Or.inr hp)))⟩

theorem push_material (n : Clip) (tr : Track) : Material tr (push n tr) := by
  intro id k t h
  obtain ⟨t', _, hp⟩ := plays_push (n := n) h
  exact ⟨t', hp⟩

theorem push_pastKept {n : Clip} {tr : Track} (h : Seq tr) : PastKept n tr (push n tr) := by
  intro id k t ht hp
  induction tr with
  | nil => exact absurd hp (plays_nil _ _ _)
  | cons c rest ih =>
    rw [seq_cons] at h
    have hrest : ∀ u, Plays rest id k u → c.stop ≤ u := fun u hu => plays_ge h.1 hu
    by_cases h1 : c.stop ≤ n.start
    · rw [push_before h1]
      rcases (plays_cons _ _ _ _ _).mp hp with hc | hr
      · exact (plays_cons _ _ _ _ _).mpr (Or.inl hc)
      · exact (plays_cons _ _ _ _ _).mpr (Or.inr (ih h.2 hr))
    · simp only [Clip.stop] at h1 hrest
      rcases (plays_cons _ _ _ _ _).mp hp with hc | hr
      · have := hc.bounds
        by_cases h2 : n.start ≤ c.start
        · omega
        · rw [push_split (by simp only [Clip.stop]; omega) h2]
          exact (plays_cons _ _ _ _ _).mpr (Or.inl (Clip.plays_keepUntil.mpr ⟨hc, ht⟩))
      · have := hrest t hr; omega

/-! ### Push moves as little as possible -/

/-- `l'` holds the clips of `l`, in the same order, none of them earlier. -/
inductive ShiftedRight : Track → Track → Prop
  | nil : ShiftedRight [] []
  | cons {c c' : Clip} {l l' : Track} :
      c'.id = c.id → c'.src = c.src → c'.len = c.len → c.start ≤ c'.start →
      ShiftedRight l l' → ShiftedRight (c :: l) (c' :: l')

/-- Clip by clip, `l` starts no later than `l'`. -/
inductive StartsLE : Track → Track → Prop
  | nil : StartsLE [] []
  | cons {c c' : Clip} {l l' : Track} :
      c.start ≤ c'.start → StartsLE l l' → StartsLE (c :: l) (c' :: l')

theorem ShiftedRight.startsLE {l l' : Track} (h : ShiftedRight l l') : StartsLE l l' := by
  induction h with
  | nil => exact StartsLE.nil
  | cons _ _ _ hle _ ih => exact StartsLE.cons hle ih

/-- Least displacement: among all ways to move the clips right, keeping their
    order, so that they clear `e` and do not overlap, `pushRight` puts every
    single clip earliest. -/
theorem pushRight_least {l l' : Track} {e : Nat} (hs : ShiftedRight l l') (hseq : Seq l')
    (he : ∀ d ∈ l', e ≤ d.start) : StartsLE (pushRight e l) l' := by
  induction hs generalizing e with
  | nil => simpa [pushRight] using StartsLE.nil
  | @cons c c' l l' hid hsrc hlen hle hrest ih =>
    rw [seq_cons] at hseq
    by_cases hc : e ≤ c.start
    · rw [pushRight_keep hc]; exact StartsLE.cons hle hrest.startsLE
    · rw [pushRight_move hc]
      refine StartsLE.cons (he c' List.mem_cons_self) (ih hseq.2 ?_)
      intro d hd
      have h1 := hseq.1 d hd
      have h2 := he c' List.mem_cons_self
      simp only [Clip.stop] at h1; omega

theorem ShiftedRight.refl (l : Track) : ShiftedRight l l := by
  induction l with
  | nil => exact ShiftedRight.nil
  | cons c rest ih => exact ShiftedRight.cons rfl rfl rfl (Nat.le_refl _) ih

/-- `pushRight` is itself such a move: same clips, same order, none earlier.
    Together with `pushRight_least` it is the least one. -/
theorem shiftedRight_pushRight (e : Nat) (l : Track) : ShiftedRight l (pushRight e l) := by
  induction l generalizing e with
  | nil => exact ShiftedRight.nil
  | cons c rest ih =>
    by_cases he : e ≤ c.start
    · rw [pushRight_keep he]; exact ShiftedRight.refl _
    · rw [pushRight_move he]
      exact ShiftedRight.cons rfl rfl rfl (by simp only; omega) (ih _)

theorem shiftedRight_map_shift (l : Track) (d : Nat) : ShiftedRight l (l.map (Clip.shift d)) := by
  induction l with
  | nil => exact ShiftedRight.nil
  | cons c rest ih =>
    exact ShiftedRight.cons rfl rfl rfl (by simp [Clip.shift]) ih

/-- In particular Push never moves a clip further than Ripple does. -/
theorem pushRight_le_shift {l : Track} {e d : Nat} (h : Seq l)
    (he : ∀ c ∈ l, e ≤ c.start + d) : StartsLE (pushRight e l) (l.map (Clip.shift d)) := by
  refine pushRight_least (shiftedRight_map_shift l d) (seq_map_shift d h) ?_
  intro x hx
  obtain ⟨c, hc, rfl⟩ := List.mem_map.mp hx
  exact he c hc

/-! ### Where Push and Ripple part: gaps -/

/-- No gaps: every clip starts where the one before it ends. -/
def Tight : Track → Prop
  | [] => True
  | [_] => True
  | c :: d :: rest => d.start = c.stop ∧ Tight (d :: rest)

/-- Without gaps, pushing is shifting: Push and Ripple agree. -/
theorem pushRight_tight (rest : Track) (c : Clip) (d : Nat) (hd : 0 < d)
    (h : Tight (c :: rest)) :
    pushRight (c.start + d) (c :: rest) = (c :: rest).map (Clip.shift d) := by
  induction rest generalizing c with
  | nil => rw [pushRight_move (by omega)]; simp [pushRight, Clip.shift]
  | cons x rest ih =>
    obtain ⟨hx, ht⟩ := h
    rw [pushRight_move (by omega)]
    have e : c.start + d + c.len = x.start + d := by simp only [Clip.stop] at hx; omega
    rw [e, ih x ht]
    simp [Clip.shift]

theorem push_eq_ripple_of_tight {n c : Clip} {rest : Track} (hn : 0 < n.len) (hc : 0 < c.len)
    (hat : n.start = c.start) (h : Tight (c :: rest)) :
    push n (c :: rest) = ripple n (c :: rest) := by
  have h1 : ¬ c.stop ≤ n.start := by simp only [Clip.stop]; omega
  have h2 : n.start ≤ c.start := by omega
  unfold ripple
  rw [push_at h1 h2, openGap_at h1 h2]
  have : n.stop = c.start + n.len := by simp only [Clip.stop]; omega
  rw [this, pushRight_tight rest c n.len hn h]
  rfl

/-- With a gap that has room, Push stops there: what lies behind the gap
    stays put. -/
theorem pushRight_stops_at_gap {e : Nat} {c : Clip} {rest : Track} (h : e ≤ c.start) :
    pushRight e (c :: rest) = c :: rest := pushRight_keep h

/-- A rule that keeps the gaps cannot leave anything downstream where it is:
    if a tick at or after the drop position still sounds at its old time,
    the dropped length is zero. So "keep gaps" and "move only what must move"
    exclude each other as soon as one clip had room to stay. -/
theorem gapsKept_moves_all {n : Clip} {tr tr' : Track} (hg : GapsKept n tr tr')
    (hfun : Functional tr') {id k t : Nat} (ht : n.start ≤ t) (hp : Plays tr id k t)
    (hstay : Plays tr' id k t) : n.len = 0 := by
  have := hfun _ _ _ _ hstay (hg id k t ht hp)
  omega

end Timeline
