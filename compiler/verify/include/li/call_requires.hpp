#pragma once

#include "li/ast.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace li {

enum class RequiresCheckResult {
  Satisfied,
  Violated,
  Unknown,
};

/// Facts available for lightweight proof discharge (const locals, if-guard assumptions).
/// `const_float_locals` is optional (nullptr when no float facts are tracked).
struct ProofFacts {
  const std::map<std::string, std::int64_t>& const_int_locals;
  const std::set<std::string>& assum_nonneg_ints;
  const std::map<std::string, double>* const_float_locals = nullptr;
};

const ProcDecl* find_proc_by_name(const Module& module, const std::string& name);

/// Build `[receiver, ...method_args]` for requires substitution on desugared methods.
std::vector<std::unique_ptr<Expr>> method_call_arg_list(
    const Expr& receiver, const std::vector<std::unique_ptr<Expr>>& method_args);

std::unique_ptr<Expr> substitute_call_params(
    const Expr& expr, const std::vector<std::string>& param_names,
    const std::vector<std::unique_ptr<Expr>>& args);

/// Replace a single refinement binder (`x` in `{x: int | …}`) with `arg_value`.
std::unique_ptr<Expr> substitute_refinement_binding(const Expr& predicate,
                                                    const std::string& bind_var,
                                                    const Expr& arg_value);

std::unique_ptr<Expr> fold_const_int_locals(
    const Expr& expr, const std::map<std::string, std::int64_t>& const_int_locals);

/// Fold const locals (int + float) and collapse the resulting literal
/// arithmetic via `fold_const` — e.g. `a[0] * b[0] + ...` with `a[0] = 1.0`,
/// `b[0] = 2.0` folds to `8.0`. Float array-element stores are keyed like
/// ints (`"a[0]"`) in the float map.
std::unique_ptr<Expr> fold_const_locals(
    const Expr& expr, const std::map<std::string, std::int64_t>& const_int_locals,
    const std::map<std::string, double>& const_float_locals);

/// `fold_const_locals` against a `ProofFacts` (float facts are optional).
std::unique_ptr<Expr> fold_facts_expr(const Expr& expr, const ProofFacts& facts);

bool expr_statically_true(const Expr& expr);
bool expr_statically_false(const Expr& expr);
bool folded_discharged_by_proof_facts(const Expr& folded, const ProofFacts& facts);

RequiresCheckResult check_requires_at_call(const ProcDecl& callee, const Expr& call,
                                           const ProofFacts& facts);

/// Method call: substitutes `self` from receiver, then remaining params from `method_args`.
RequiresCheckResult check_requires_at_method_call(
    const ProcDecl& callee, const Expr& receiver,
    const std::vector<std::unique_ptr<Expr>>& method_args, const ProofFacts& facts);

/// When a call provably breaks a callee `requires`, plain-language text for diagnostics.
struct RequiresViolationExplanation {
  std::string message;
  std::string hint;
};

std::optional<RequiresViolationExplanation> explain_requires_violation(
    const ProcDecl& callee, const Expr& call, const ProofFacts& facts);

std::optional<RequiresViolationExplanation> explain_requires_violation_method(
    const ProcDecl& callee, const Expr& receiver, const std::string& method_name,
    const std::vector<std::unique_ptr<Expr>>& method_args, const ProofFacts& facts);

std::string method_call_to_user_string(const Expr& receiver, const std::string& method,
                                       const std::vector<std::unique_ptr<Expr>>& method_args);

std::string expr_to_user_string(const Expr& expr);
std::string call_to_user_string(const Expr& call);

void collect_calls_in_stmts(const std::vector<Stmt>& stmts,
                            std::vector<const Expr*>& out);

void collect_method_calls_in_stmts(const std::vector<Stmt>& stmts,
                                 std::vector<const Expr*>& out);

/// Identifiers referenced in an expression (for Lean VC formals on call-site requires).
void collect_idents_in_expr(const Expr& expr, std::set<std::string>& out);

/// Resolved refinement on a parameter or variable type (`{x: int | …}` or alias).
struct ResolvedRefinement {
  std::string bind_var;
  std::string type_label;
  const Expr* predicate = nullptr;
};

using AliasTypeLookup = std::function<const TypeExpr*(const std::string&)>;

std::optional<ResolvedRefinement> resolve_refinement_on_type(const TypeExpr& param_type,
                                                             AliasTypeLookup lookup);

RequiresCheckResult check_refinement_argument(const ResolvedRefinement& refinement,
                                              const Expr& arg, const ProofFacts& facts);

std::optional<RequiresViolationExplanation> explain_refinement_violation(
    const ResolvedRefinement& refinement, const Expr& arg, const ProofFacts& facts);

/// Record `name` as non-negative inside a guarded branch (`if name >= 0`, etc.).
void note_nonneg_assumption_from_cond(const Expr& cond, std::set<std::string>& out);

/// `w.balance` after `w.balance = 5` for method `requires self.balance >= n` folding.
std::optional<std::string> object_field_const_key(const Expr& e);

/// Canonical key for const array-element stores: `a[0]` and nested `a[0][1]`
/// (2D tiles), requiring every index to be a literal.
std::optional<std::string> array_index_const_key(const Expr& e);

/// Replace an ident with an int literal throughout an expression (loop
/// unrolling), cloning the tree.
std::unique_ptr<Expr> subst_ident_lit(const Expr& e, const std::string& from,
                                      std::int64_t to);

/// Recognize `while i < N: ...; i = i + 1` counter loops with a known start,
/// no other writes to `i`, no break/continue/return, and at most 64
/// iterations — safe to unroll for per-element const-store facts.
bool simple_counter_loop(const Stmt& loop,
                         const std::map<std::string, std::int64_t>& const_int_locals,
                         std::string* idx, std::int64_t* start, std::int64_t* end);

/// Fold a call-assignment (`s = f(args)` or `C = f(args)` for tile-returning
/// procs) against a proc lookup instead of a full Module — the typechecker's
/// requires path uses this.
bool try_fold_call_to_const(const std::string& name, const Expr& call,
                            std::map<std::string, std::int64_t>& const_int_locals,
                            std::map<std::string, double>& const_float_locals,
                            const std::function<const ProcDecl*(const std::string&)>& lookup);

/// Owned facts for VC emit (const `var` inits + `if n >= 0` guards in procedure body).
/// `const_float_locals` tracks float const locals and float array-element
/// stores (`a[0] = 1.0`) plus call-assignments whose callee `ensures` folds to
/// a constant (`s = dot4_float(a, b)` when the arrays are constant-filled).
struct CallerProofFacts {
  std::map<std::string, std::int64_t> const_int_locals;
  std::set<std::string> assum_nonneg_ints;
  std::map<std::string, double> const_float_locals;
  ProofFacts view() const {
    return ProofFacts{const_int_locals, assum_nonneg_ints, &const_float_locals};
  }
};

CallerProofFacts collect_caller_proof_facts(const ProcDecl& caller,
                                            const Module* module = nullptr);

}  // namespace li
