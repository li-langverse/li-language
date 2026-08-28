import Init

/-!
# Li Core semantics (Phase 2f partial)

Formal typing and contract rules for Li Core. See `docs/verification/provability-gaps.md` (**G-trust**).

`LiArray α n` matches the compiler's fixed-size `array[n, α]` surface in `AutoVC.lean` (not
Lean's builtin `Array α`).

## Typing rules (closed slice)

### T-GetElem

If `a : LiArray α n` and `i : Nat` with `i < n`, then `a[i]` is well-typed with type `α`.
This is the semantic counterpart of Li's release rule: dynamic indices need a bounds proof.
-/

namespace Li

/-- Build gate: Lake must compile before `lic build` accepts release. -/
theorem core_build_ok : True := trivial

/-- Fixed-size array surface (`array[n, α]` in Li). Proof layer uses `Fin` indexing. -/
abbrev LiArray (α : Type) (n : Nat) := Fin n → α

instance {α : Type} {n : Nat} : GetElem (LiArray α n) Nat α fun _ i => i < n where
  getElem a i h := a ⟨i, h⟩

/-- **T-GetElem:** `⊢ a[i] : α` when `a : LiArray α n` and `i < n`. -/
theorem typing_getElem {α : Type} {n : Nat} (a : LiArray α n) (i : Nat) (hi : i < n) :
    ∃ (v : α), a ⟨i, hi⟩ = v := ⟨a ⟨i, hi⟩, rfl⟩

end Li

open Li

-- `build/generated/AutoVC.lean` (namespace `AutoVC`) imports this file on every `lic build`.
