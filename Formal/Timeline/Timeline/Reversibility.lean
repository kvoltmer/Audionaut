/-
  Theorem 3. Which drops can be taken back by a remove rule.

  Layer is undone by removing the clip and leaving its place empty. Ripple is
  undone by removing the clip and closing the hole. Push and Overwrite forget
  what the track looked like, so no remove rule can undo them.
-/
import Timeline.Straddle

namespace Timeline

/-- Remove the clip, leave everything else where it is. -/
def leaveGap (id : Nat) (tr : Track) : Track := tr.filter fun c => c.id != id

/-- Remove the clip and move everything behind it earlier by its length. -/
def closeGap (id : Nat) : Track → Track
  | [] => []
  | c :: rest => if c.id = id then rest.map (Clip.unshift c.len) else c :: closeGap id rest

theorem leaveGap_layer {n : Clip} {tr : Track} (hf : Fresh n tr) :
    leaveGap n.id (layer n tr) = tr := by
  have : tr.filter (fun c => c.id != n.id) = tr :=
    List.filter_eq_self.mpr fun c hc => by simpa using hf c hc
  simp [leaveGap, layer, this]

theorem map_unshift_shift (l : Track) (d : Nat) :
    (l.map (Clip.shift d)).map (Clip.unshift d) = l := by
  induction l with
  | nil => rfl
  | cons c rest ih => simp [ih]

/-- Close-gap undoes Ripple, clip for clip, when the drop position was not
    inside a clip. -/
theorem closeGap_ripple {n : Clip} {tr : Track} (hf : Fresh n tr)
    (hns : ∀ c ∈ tr, c.stop ≤ n.start ∨ n.start ≤ c.start) :
    closeGap n.id (ripple n tr) = tr := by
  unfold ripple
  induction tr with
  | nil => simp [openGap, closeGap]
  | cons c rest ih =>
    have hcid : c.id ≠ n.id := hf c List.mem_cons_self
    have hf' : Fresh n rest := fun x hx => hf x (List.mem_cons_of_mem _ hx)
    have hns' : ∀ x ∈ rest, x.stop ≤ n.start ∨ n.start ≤ x.start :=
      fun x hx => hns x (List.mem_cons_of_mem _ hx)
    by_cases h1 : c.stop ≤ n.start
    · rw [openGap_before h1]
      simp [closeGap, hcid, ih hf' hns']
    · by_cases h2 : n.start ≤ c.start
      · rw [openGap_at h1 h2]
        simp only [List.singleton_append, closeGap, ite_true]
        exact map_unshift_shift _ _
      · rcases hns c List.mem_cons_self with h | h
        · exact absurd h h1
        · exact absurd h h2

/-- Close-gap undoes Ripple in every case as far as the sound goes: where a
    clip was cut, the two pieces come back together edge to edge. -/
theorem plays_closeGap_ripple {n : Clip} {tr : Track} (hf : Fresh n tr) (id k t : Nat) :
    Plays (closeGap n.id (ripple n tr)) id k t ↔ Plays tr id k t := by
  unfold ripple
  induction tr with
  | nil => simp [openGap, closeGap]
  | cons c rest ih =>
    have hcid : c.id ≠ n.id := hf c List.mem_cons_self
    have hf' : Fresh n rest := fun x hx => hf x (List.mem_cons_of_mem _ hx)
    by_cases h1 : c.stop ≤ n.start
    · rw [openGap_before h1]
      simp [closeGap, hcid, ih hf']
    · by_cases h2 : n.start ≤ c.start
      · rw [openGap_at h1 h2]
        simp only [List.singleton_append, closeGap, ite_true]
        rw [map_unshift_shift]
      · rw [openGap_split h1 h2]
        have hk : (c.keepUntil n.start).id ≠ n.id := hcid
        simp only [List.singleton_append, closeGap, hk, ite_false, ite_true]
        rw [map_unshift_shift]
        simp only [plays_cons, Clip.plays_keepUntil, Clip.plays_keepFrom]
        constructor
        · rintro (⟨h, _⟩ | ⟨h, _⟩ | h)
          · exact Or.inl h
          · exact Or.inl h
          · exact Or.inr h
        · rintro (h | h)
          · by_cases ht : t < n.start
            · exact Or.inl ⟨h, ht⟩
            · exact Or.inr (Or.inl ⟨h, by omega⟩)
          · exact Or.inr (Or.inr h)

/-- So Ripple at a clip edge loses no information: different tracks give
    different results. -/
theorem ripple_injective {n : Clip} {tr₁ tr₂ : Track}
    (hf₁ : Fresh n tr₁) (hf₂ : Fresh n tr₂)
    (hns₁ : ∀ c ∈ tr₁, c.stop ≤ n.start ∨ n.start ≤ c.start)
    (hns₂ : ∀ c ∈ tr₂, c.stop ≤ n.start ∨ n.start ≤ c.start)
    (h : ripple n tr₁ = ripple n tr₂) : tr₁ = tr₂ := by
  rw [← closeGap_ripple hf₁ hns₁, ← closeGap_ripple hf₂ hns₂, h]

theorem layer_injective {n : Clip} {tr₁ tr₂ : Track} (h : layer n tr₁ = layer n tr₂) :
    tr₁ = tr₂ := by
  simpa [layer] using h

/-! ### Push and Overwrite cannot be undone -/

/-- Two different tracks that Push turns into the same track: a clip that
    was already clear of the drop by one tick, and one that was not. -/
theorem push_not_injective :
    ∃ (n : Clip) (tr₁ tr₂ : Track), Seq tr₁ ∧ Seq tr₂ ∧ tr₁ ≠ tr₂ ∧ push n tr₁ = push n tr₂ :=
  ⟨⟨9, 0, 0, 2⟩, [⟨1, 0, 0, 1⟩], [⟨1, 1, 0, 1⟩], by simp, by simp, by decide, by decide⟩

theorem push_no_undo : ¬ ∃ undo : Clip → Track → Track, ∀ n tr, undo n (push n tr) = tr := by
  rintro ⟨undo, h⟩
  obtain ⟨n, tr₁, tr₂, _, _, hne, heq⟩ := push_not_injective
  exact hne (by rw [← h n tr₁, ← h n tr₂, heq])

/-- Two different tracks that Overwrite turns into the same track: what was
    under the dropped clip is gone. -/
theorem overwrite_not_injective :
    ∃ (n : Clip) (tr₁ tr₂ : Track), tr₁ ≠ tr₂ ∧ overwrite n tr₁ = overwrite n tr₂ :=
  ⟨⟨9, 0, 0, 2⟩, [⟨1, 0, 0, 1⟩], [⟨1, 1, 0, 1⟩], by decide, by decide⟩

theorem overwrite_no_undo :
    ¬ ∃ undo : Clip → Track → Track, ∀ n tr, undo n (overwrite n tr) = tr := by
  rintro ⟨undo, h⟩
  obtain ⟨n, tr₁, tr₂, hne, heq⟩ := overwrite_not_injective
  exact hne (by rw [← h n tr₁, ← h n tr₂, heq])

end Timeline
