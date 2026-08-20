#pragma once

#include "li/ast.hpp"
#include "li/mir.hpp"

namespace li {

struct CallerProofFacts;

struct VcWitnessStats {
  std::size_t ensures_total = 0;
  std::size_t ensures_witnessed = 0;
  std::size_t mir_return_linked = 0;
};

const Expr* single_return_expr(const ProcDecl& proc);
bool contract_witnessed_trivial(const ProcDecl& proc, const Contract& c,
                                const Module* module = nullptr,
                                const CallerProofFacts* caller_facts = nullptr);
bool mir_return_links_proc(const MirFn& fn, const ProcDecl& proc);
bool witness_dot4_int_loop(const ProcDecl& proc, const Expr& ensures_rhs);
bool witness_dot4_prelude_call(const Expr& ret, const Expr& ensures_rhs);
bool witness_dot4_matmul(const Expr& ret, const Expr& ensures_rhs);
bool witness_mat2_int_at2_spec(const ProcDecl& proc, const Expr& ensures_expr);
bool witness_sqrt_open_bound_spec(const ProcDecl& proc, const Expr& ensures_expr);

VcWitnessStats compute_vc_witness_stats(const Module& module, const MirModule* mir);

}  // namespace li
