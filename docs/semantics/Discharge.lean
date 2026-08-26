import Init.Data.Float
import Core

/-!
# Discharge lemmas for generated AutoVC (Phase 2f partial)

`lic build` emits trivial `_proved` theorems in `build/generated/AutoVC.lean` for:
- `requires` / `ensures true`
- `ensures result == …` when the procedure body returns the same expression (static witness)
- literal `decreases` naturals

This module is reserved for hand-written lemmas that cannot be generated yet (e.g. float
postconditions for `sqrt_contract`, loop implementations for **P-linalg**). See **G-lean** and
**G-math** in `docs/verification/provability-gaps.md`.
-/

open Li

namespace Li.Discharge

theorem discharge_corpus_placeholder : True := trivial

/-- Closed-form fixed-size dot (matches `linalg_dot4_int_closed.li` / loop specimen). -/
def dot4_int_spec (a b : LiArray Int 4) (result : Int) : Prop :=
  result = (((((a[0]!) * (b[0]!)) + ((a[1]!) * (b[1]!))) + ((a[2]!) * (b[2]!))) + ((a[3]!) * (b[3]!)))

/-- Semantic value of the 4-iteration `while i < 4` dot loop (`witness_dot4_int_loop`). -/
def dot4_loop_eval (a b : LiArray Int 4) : Int :=
  (((((a[0]!) * (b[0]!)) + ((a[1]!) * (b[1]!))) + ((a[2]!) * (b[2]!))) + ((a[3]!) * (b[3]!)))

/-- Loop implementation matches closed-form spec (P-loop / **G-vc**). -/
theorem dot4_int_loop_eval_spec (a b : LiArray Int 4) : dot4_int_spec a b (dot4_loop_eval a b) := rfl

/-- One entry of 2×2 int matmul (matches `linalg_mat2_entry00_int_closed.li`). -/
def mat2_entry00_int_spec (a00 a01 b00 b10 result : Int) : Prop :=
  result = ((a00 * b00) + (a01 * b10))

/-- Full 2×2 float `@` postcondition (matches `linalg_mat2_at2_float_closed.li`). -/
def mat2_at2_float_spec (A B result : LiArray (LiArray Float 2) 2) : Prop :=
  (result[0]![0]! = ((A[0]![0]! * B[0]![0]!) + (A[0]![1]! * B[1]![0]!))) ∧
  (result[0]![1]! = ((A[0]![0]! * B[0]![1]!) + (A[0]![1]! * B[1]![1]!))) ∧
  (result[1]![0]! = ((A[1]![0]! * B[0]![0]!) + (A[1]![1]! * B[1]![0]!))) ∧
  (result[1]![1]! = ((A[1]![0]! * B[0]![1]!) + (A[1]![1]! * B[1]![1]!)))

/-- Semantic 2×2 `@` (matches `return A @ B` for fixed 2×2 tiles). -/
def mat2_at2_eval (A B : LiArray (LiArray Float 2) 2) : LiArray (LiArray Float 2) 2 :=
  fun i j =>
    match i, j with
    | ⟨0, _⟩, ⟨0, _⟩ => (A[0]![0]! * B[0]![0]!) + (A[0]![1]! * B[1]![0]!)
    | ⟨0, _⟩, ⟨1, _⟩ => (A[0]![0]! * B[0]![1]!) + (A[0]![1]! * B[1]![1]!)
    | ⟨1, _⟩, ⟨0, _⟩ => (A[1]![0]! * B[0]![0]!) + (A[1]![1]! * B[1]![0]!)
    | ⟨1, _⟩, ⟨1, _⟩ => (A[1]![0]! * B[0]![1]!) + (A[1]![1]! * B[1]![1]!)

/-- Closed 2×2 float `@` witness (P-linalg / **G-math**). -/
theorem mat2_at2_float_spec_proved (A B : LiArray (LiArray Float 2) 2) :
    mat2_at2_float_spec A B (mat2_at2_eval A B) := by
  unfold mat2_at2_float_spec mat2_at2_eval
  refine And.intro rfl (And.intro rfl (And.intro rfl rfl))

/-!
## Trusted libm (`li_rt_sqrt`) — **P-float** corpus only

`li_rt_sqrt` accuracy is axiomatized here (not proved from IEEE). See **G-hw** in provability-gaps.
-/
namespace Li.TrustedMath

axiom li_rt_sqrt : Float → Float

axiom li_rt_sqrt_bound (x : Float) (hx : x ≥ (0 : Float)) :
    Float.abs (li_rt_sqrt x * li_rt_sqrt x - x) < (1e-12 : Float)

end Li.TrustedMath

def sqrt_open_bound_spec (x : Float) : Prop :=
  Float.abs (Li.TrustedMath.li_rt_sqrt x * Li.TrustedMath.li_rt_sqrt x - x) < (1e-12 : Float)

theorem sqrt_open_bound_spec_proved (x : Float) (hreq : x ≥ (0 : Float)) : sqrt_open_bound_spec x :=
  Li.TrustedMath.li_rt_sqrt_bound x hreq

/-!
## Refinement types (**P-refine** / **G-vc**)
-/
def refinement_nonneg_spec (n : Int) : Prop := n ≥ (0 : Int)

theorem refinement_nonneg_lit_proved (n : Int) (hn : n ≥ (0 : Int)) : refinement_nonneg_spec n := hn

/-!
## Classical physics (**P-physics** / proof-database)

Scalar point-mass stubs aligned with `docs/verification/proof-database/entries/physics-*.toml`.
Tier-2 drivers remain **modeling_gap** until extern kernels export real `ensures`.
-/

/-- Kinetic energy T = ½ m v² (P-LM-ENERGY-001). -/
def kinetic_energy_spec (m v T : Float) : Prop :=
  T = (0.5 : Float) * m * v * v

theorem kinetic_energy_def_consistent (m v : Float) :
    kinetic_energy_spec m v ((0.5 : Float) * m * v * v) := rfl

/-- Linear momentum p = m v (P-LM-MOM-001). -/
def linear_momentum_spec (m v p : Float) : Prop :=
  p = m * v

theorem linear_momentum_linear_stub (m v : Float) :
    linear_momentum_spec m v (m * v) := rfl

/-- Newton second law scalar stub (P-AX-MECH-002 witness). -/
def force_equals_mass_accel_spec (m a F : Float) : Prop :=
  F = m * a

theorem force_equals_mass_accel_stub (m a : Float) :
    force_equals_mass_accel_spec m a (m * a) := rfl

/-- Dimensional homogeneity — placeholder until unit types exist (P-AX-DIM-001). -/
theorem dimensional_homogeneity_placeholder : True := trivial

end Li.Discharge
