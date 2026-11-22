/-
  Several tracks. Ripple on one track alone moves its material against the
  other tracks. Opening the same hole on every track keeps what sounded
  together sounding together.
-/
import Timeline.Views

namespace Timeline

/-- Open the hole without putting anything in it: what the other tracks get
    when one track receives a rippling drop. -/
def openSilence (t L : Nat) (tr : Track) : Track := openGap [] t L tr

theorem openSilence_seq {tr : Track} (t L : Nat) (h : Seq tr) : Seq (openSilence t L tr) :=
  (openGap_seq (block_nil t L) h).1

/-- Two ticks that sounded at the same time on two tracks still sound at the
    same time after a ripple drop on one and the same hole on the other. -/
theorem ripple_all_keeps_sync {n : Clip} {a b : Track} (ha : Seq a) (hb : Seq b)
    {ida ka idb kb t : Nat} (hpa : Plays a ida ka t) (hpb : Plays b idb kb t) :
    ∃ t', Plays (ripple n a) ida ka t' ∧ Plays (openSilence n.start n.len b) idb kb t' := by
  by_cases ht : t < n.start
  · exact ⟨t, (plays_openGap ha _ _ _).mpr (Or.inr ⟨t, hpa, Or.inl ⟨ht, rfl⟩⟩),
      (plays_openGap hb _ _ _).mpr (Or.inr ⟨t, hpb, Or.inl ⟨ht, rfl⟩⟩)⟩
  · exact ⟨t + n.len, (plays_openGap ha _ _ _).mpr (Or.inr ⟨t, hpa, Or.inr ⟨by omega, rfl⟩⟩),
      (plays_openGap hb _ _ _).mpr (Or.inr ⟨t, hpb, Or.inr ⟨by omega, rfl⟩⟩)⟩

/-- Ripple on one track alone: whatever sounded together with the other
    track at or after the drop position is now a drop length late. -/
theorem ripple_one_breaks_sync {n : Clip} {a : Track} (ha : Seq a)
    (hfun : Functional (ripple n a)) {id k t : Nat} (ht : n.start ≤ t) (hn : 0 < n.len)
    (hp : Plays a id k t) : ¬ Plays (ripple n a) id k t := by
  intro hstay
  have := gapsKept_moves_all (ripple_gapsKept ha) hfun ht hp hstay
  omega

end Timeline
