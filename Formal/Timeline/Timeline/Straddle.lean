/-
  A drop position inside a clip. If the past must stay as it was and the
  dropped clip must land where it was dropped, the clip under the drop
  position cannot stay in one piece. A shift rule therefore has three ways
  out: cut the clip (what `ripple` and `push` do), move the drop position to
  a clip edge, or move the clip as a whole and so change the past.
-/
import Timeline.Shift

namespace Timeline

theorem straddle {tr tr' : Track} {n c : Clip}
    (hc : c ∈ tr) (hid : c.id ≠ n.id)
    (hin : c.start < n.start ∧ n.start < c.stop) (hn : 0 < n.len)
    (hpast : PastKept n tr tr') (hS : Separation tr') (hF : Fidelity n tr')
    (hu : UniqueIds tr')
    (hwhole : ∃ c' ∈ tr', c'.id = c.id ∧ c'.src = c.src ∧ c'.len = c.len) : False := by
  simp only [Clip.stop] at hin
  obtain ⟨c', hc', hid', hsrc, hlen⟩ := hwhole
  -- the last tick of `c` before the drop position still sounds there
  have hplay : Plays tr c.id (c.src + (n.start - 1 - c.start)) (n.start - 1) :=
    ⟨c, hc, rfl, by omega, by omega, by omega⟩
  obtain ⟨c'', hc'', hp⟩ := hpast _ _ _ (by omega) hplay
  simp only [Clip.Plays] at hp
  have : c'' = c' := hu c'' hc'' c' hc' (by omega)
  subst this
  have hov : c''.Overlaps n := by simp only [Clip.Overlaps, Clip.stop]; omega
  have := hS c'' hc' n hF hov
  subst this
  exact hid hid'.symm

/-- A drop position that is the start of a clip on the track is inside no
    clip, so nothing has to be cut. This is the case for every drop that is
    addressed by list position rather than by time. -/
theorem seq_no_straddle {tr : Track} (h : Seq tr) {c : Clip} (hc : c ∈ tr) :
    ∀ d ∈ tr, d.stop ≤ c.start ∨ c.start ≤ d.start := by
  induction tr with
  | nil => cases hc
  | cons x rest ih =>
    rw [seq_cons] at h
    intro d hd
    rcases List.mem_cons.mp hc with rfl | hc' <;> rcases List.mem_cons.mp hd with rfl | hd'
    · exact Or.inr (Nat.le_refl _)
    · have := h.1 d hd'; simp only [Clip.stop] at this; right; omega
    · exact Or.inl (h.1 c hc')
    · exact ih h.2 hc' d hd'

end Timeline
