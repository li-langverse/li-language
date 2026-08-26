import Mathlib
import MathAxioms

namespace Li.ProofDb.Math

theorem m_lm_add_comm (a b : ℝ) : a + b = b + a := real_add_comm a b
theorem m_lm_add_assoc (a b c : ℝ) : a + b + c = a + (b + c) := real_add_assoc a b c
theorem m_lm_mul_one (a : ℝ) : a * 1 = a := real_mul_one a
theorem m_lm_nat_add_zero (n : Nat) : n + 0 = n := Nat.add_zero n
theorem m_lm_nat_add_comm (a b : Nat) : a + b = b + a := Nat.add_comm a b

end Li.ProofDb.Math
