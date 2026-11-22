/- Not part of the library. `lake env lean Check.lean` lists the axioms the
   main results rest on; `sorryAx` must not appear. -/
import Timeline

open Timeline

#print axioms conflict
#print axioms layer_position
#print axioms overwrite_separation
#print axioms overwrite_keeps_uncovered
#print axioms relocate_separation
#print axioms straddle
#print axioms plays_openGap
#print axioms ripple_seq
#print axioms ripple_unique
#print axioms push_seq
#print axioms plays_push
#print axioms push_pastKept
#print axioms pushRight_least
#print axioms push_eq_ripple_of_tight
#print axioms gapsKept_moves_all
#print axioms closeGap_ripple
#print axioms plays_closeGap_ripple
#print axioms push_no_undo
#print axioms overwrite_no_undo
#print axioms ofSeq_toSeq
#print axioms toSeq_ofSeq
#print axioms toSeq_ripple_before
#print axioms closeGap_dropAtIndex
#print axioms ripple_all_keeps_sync
#print axioms panelDrop_overlaps
#print axioms paste_overlaps
