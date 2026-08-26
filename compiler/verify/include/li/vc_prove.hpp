#pragma once

#include "li/ast.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace li {

/// A constant-folded numeric value: `ok == false` means not foldable
/// (overflow, non-exact integer division, or division by zero). `is_float`
/// selects between exact int64 and IEEE double semantics.
struct FoldVal {
  bool ok = false;
  bool is_float = false;
  std::int64_t iv = 0;
  double fv = 0.0;

  double num() const { return is_float ? fv : static_cast<double>(iv); }
};

/// Constant-fold `+ - * /` over numeric literals. Pure-integer expressions
/// fold with exact int64 arithmetic; once a float literal appears, the fold
/// uses IEEE double semantics. Exported so the requires/witness layers can
/// collapse substituted expressions (e.g. `1.0 * 2.0 + ...` to `8.0`).
FoldVal fold_const(const Expr& e);

/// Native proof layer (Layer 2): discharges first-order propositions over the
/// arithmetic fragment with rewriting — AC normalization of + and *, constant
/// folding, distributivity expansion, identity rules — plus boolean
/// simplification of `and`/`or`/`not`/`implies`. Idents are treated as opaque
/// atoms; a theorem `∀ params, prop` closes iff `prop` is a tautology of its
/// atoms under these rewrites.
///
/// `true`  — the proposition is discharged natively.
/// `false` — the proposition is natively disprovable (a contradiction).
/// `null`  — not decided (stays an open obligation).
bool prove_prop_natively(const Expr& prop);

/// Returns true when both expressions constant-fold to the same numeric
/// value — e.g. `1.0 + 1.0 + 1.0` and `3.0`. Pure-integer expressions compare
/// exactly; expressions involving a float literal compare with IEEE double
/// semantics. Overflow and division by zero never fold (returning false).
bool fold_numeric_equal(const Expr& a, const Expr& b);

enum class NativeDischarge { Proved, Disproved, Open };

NativeDischarge discharge_prop_natively(const Expr& prop);

struct TheoremDischargeResult {
  std::size_t axiom_count = 0;
  std::size_t proved_count = 0;
  std::size_t open_count = 0;
  std::vector<std::string> open_names;
};

/// Discharges every non-axiom `theorem`/`lemma` in the module; axioms are
/// trusted and counted separately.
TheoremDischargeResult discharge_theorems_natively(const Module& module);

}  // namespace li
