/-
  The properties a drop might be asked to keep. `tr` is the track before the
  drop, `n` the clip being dropped (its `start` is the drop position), `tr'`
  the track afterwards.
-/
import Timeline.Basic

namespace Timeline

/-- P1 material: everything that sounded still sounds somewhere. -/
def Material (tr tr' : Track) : Prop :=
  ∀ id k t, Plays tr id k t → ∃ t', Plays tr' id k t'

/-- P2 position: whatever still sounds, sounds when it did before. -/
def Position (tr tr' : Track) : Prop :=
  ∀ id k t t', Plays tr id k t → Plays tr' id k t' → t' = t

/-- P3 separation: no two clips sound at once. -/
abbrev Separation (tr' : Track) : Prop := OverlapFree tr'

/-- P4 fidelity: the dropped clip sits where it was dropped, whole. -/
def Fidelity (n : Clip) (tr' : Track) : Prop := n ∈ tr'

/-- P7 the past is untouched: whatever sounded before the drop position
    sounds at the same tick afterwards. -/
def PastKept (n : Clip) (tr tr' : Track) : Prop :=
  ∀ id k t, t < n.start → Plays tr id k t → Plays tr' id k t

/-- P6 gaps kept: whatever sounded at or after the drop position sounds
    exactly one drop length later, so every distance downstream survives. -/
def GapsKept (n : Clip) (tr tr' : Track) : Prop :=
  ∀ id k t, n.start ≤ t → Plays tr id k t → Plays tr' id k (t + n.len)

/-- Every source tick sounds at most once. -/
def Functional (tr : Track) : Prop :=
  ∀ id k t₁ t₂, Plays tr id k t₁ → Plays tr id k t₂ → t₁ = t₂

end Timeline
