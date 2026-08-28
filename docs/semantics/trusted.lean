-- Trusted axioms for Li (ONLY unproved surface)
-- Every symbol here must be reviewed in RFC; keep minimal.

import Mathlib

namespace Li.Trusted

/-- Abstract IO monad for OS/SDL — user code proves against this interface. -/
axiom IO : Type → Type

axiom IO.bind : {α β : Type} → IO α → (α → IO β) → IO β
axiom IO.pure : {α : Type} → α → IO α

/-- Present one frame; axiomatized, not implemented in Lean. -/
axiom present_frame : Unit → IO Unit

/-- Read keyboard event; axiomatized. -/
axiom poll_event : Unit → IO (Option UInt32)

end Li.Trusted
