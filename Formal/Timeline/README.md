# Timeline model

A Lean 4 model of clips on a track, used to decide which rules a drop onto occupied time can follow. The results and what they mean for the application are in [`docs/plans/timeline-drop-rules/evaluation.md`](../../docs/plans/timeline-drop-rules/evaluation.md).

This folder is not part of the application build.

```
brew install elan-init && elan toolchain install stable
lake build                 # checks every theorem and counterexample
lake env lean Check.lean   # lists the axioms the main results rest on
```

The toolchain version is pinned in `lean-toolchain`. There are no dependencies beyond core Lean.
