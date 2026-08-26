import Core
import Discharge
namespace Li.ProofDB
def dot4_eval (a b : LiArray Int 4) : Int :=
  (((((a[0]!) * (b[0]!)) + ((a[1]!) * (b[1]!))) + ((a[2]!) * (b[2]!))) + ((a[3]!) * (b[3]!)))
def add4_int (a b : LiArray Int 4) : LiArray Int 4 := fun i => a[i]! + b[i]!
theorem std_add_comm_int (a b : Int) : a + b = b + a := Int.add_comm a b
theorem std_mul_assoc_int (a b c : Int) : (a * b) * c = a * (b * c) := Int.mul_assoc a b c
theorem std_triangle_ineq_float_scalar (a b : Float) :
    Float.abs (a + b) ≤ Float.abs a + Float.abs b := by sorry
theorem std_dot4_bilinear_right (a b c : LiArray Int 4) :
    dot4_eval a (add4_int b c) = dot4_eval a b + dot4_eval a c := by unfold dot4_eval add4_int; rfl
theorem std_dot4_comm (a b : LiArray Int 4) : dot4_eval a b = dot4_eval b a := by unfold dot4_eval; rfl
theorem std_dot4_agrees_discharge (a b : LiArray Int 4) :
    dot4_eval a b = Li.Discharge.dot4_loop_eval a b := rfl
end Li.ProofDB
