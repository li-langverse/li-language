import Core
import Discharge
set_option maxHeartbeats 1000000
namespace Li.ProofDB
def dot4_eval (a b : LiArray Int 4) : Int :=
  (((((a[0]!) * (b[0]!)) + ((a[1]!) * (b[1]!))) + ((a[2]!) * (b[2]!))) + ((a[3]!) * (b[3]!)))
def add4_int (a b : LiArray Int 4) : LiArray Int 4 := fun i => a[i]! + b[i]!
theorem std_add_comm_int (a b : Int) : a + b = b + a := Int.add_comm a b
theorem std_mul_assoc_int (a b c : Int) : (a * b) * c = a * (b * c) := Int.mul_assoc a b c
theorem std_triangle_ineq_float_scalar (a b : Float) :
    Float.abs (a + b) ≤ Float.abs a + Float.abs b := by sorry
theorem std_dot4_bilinear_right (a b c : LiArray Int 4) :
    dot4_eval a (add4_int b c) = dot4_eval a b + dot4_eval a c := by
  simp only [dot4_eval, add4_int]
  have hb0 : (add4_int b c)[0]! = b[0]! + c[0]! := rfl
  have hb1 : (add4_int b c)[1]! = b[1]! + c[1]! := rfl
  have hb2 : (add4_int b c)[2]! = b[2]! + c[2]! := rfl
  have hb3 : (add4_int b c)[3]! = b[3]! + c[3]! := rfl
  rw [hb0, hb1, hb2, hb3]
  have h0 : a[0]! * (b[0]! + c[0]!) = a[0]! * b[0]! + a[0]! * c[0]! := by simp [Int.mul_add]
  have h1 : a[1]! * (b[1]! + c[1]!) = a[1]! * b[1]! + a[1]! * c[1]! := by simp [Int.mul_add]
  have h2 : a[2]! * (b[2]! + c[2]!) = a[2]! * b[2]! + a[2]! * c[2]! := by simp [Int.mul_add]
  have h3 : a[3]! * (b[3]! + c[3]!) = a[3]! * b[3]! + a[3]! * c[3]! := by simp [Int.mul_add]
  rw [h0, h1, h2, h3]
  ac_rfl
theorem std_dot4_comm (a b : LiArray Int 4) : dot4_eval a b = dot4_eval b a := by
  simp only [dot4_eval]
  rw [Int.mul_comm a[0]! b[0]!, Int.mul_comm a[1]! b[1]!,
      Int.mul_comm a[2]! b[2]!, Int.mul_comm a[3]! b[3]!]
theorem std_dot4_agrees_discharge (a b : LiArray Int 4) :
    dot4_eval a b = Li.Discharge.dot4_loop_eval a b := rfl
end Li.ProofDB
