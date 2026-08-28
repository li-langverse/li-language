import Mathlib
namespace Li.ProofDb.Math
axiom peano_zero_not_succ : Prop
axiom peano_succ_injective : ∀ a b : Nat, Nat.succ a = Nat.succ b → a = b
axiom peano_induction (P : Nat → Prop) : P 0 → (∀ n, P n → P (Nat.succ n)) → ∀ n, P n
axiom order_trichotomy_nat : ∀ a b : Nat, a < b ∨ a = b ∨ b < a
axiom order_antisym : ∀ a b : Nat, a ≤ b → b ≤ a → a = b
axiom real_add_comm : ∀ a b : ℝ, a + b = b + a
axiom real_add_assoc : ∀ a b c : ℝ, a + b + c = a + (b + c)
axiom real_mul_distrib : ∀ a b c : ℝ, a * (b + c) = a * b + a * c
axiom real_mul_one : ∀ a : ℝ, a * 1 = a
end Li.ProofDb.Math
