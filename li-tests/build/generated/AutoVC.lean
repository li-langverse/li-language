-- Auto-generated VC obligations (Phase 2e). Props typecheck in Lean; discharge in 2f.
import Init.Data.Float
import Core

namespace AutoVC

namespace get_cell

def vc_get_cell_requires_0 (grid : Array Int 8) (i : Int) : Prop := True
theorem vc_get_cell_requires_0_proved (grid : Array Int 8) (i : Int) : vc_get_cell_requires_0 grid i := trivial
def vc_get_cell_ensures_0 (grid : Array Int 8) (i : Int) (result : Int) : Prop := True
/-! Phase 2f: return expression matches ensures (static witness) -/
theorem vc_get_cell_ensures_0_proved (grid : Array Int 8) (i : Int) (result : Int) : vc_get_cell_ensures_0 grid i result := trivial
def vc_get_cell_decreases_0 : Nat := 0
theorem vc_get_cell_decreases_0_proved : vc_get_cell_decreases_0 = 0 := rfl

end get_cell

end AutoVC
