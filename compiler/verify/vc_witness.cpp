#include "li/vc_witness.hpp"

#include "li/call_requires.hpp"
#include "li/vc_prove.hpp"

#include <cmath>
#include <memory>

namespace li {

void collect_return_exprs_in_stmts(const std::vector<Stmt>& stmts,
                                   std::vector<const Expr*>& out) {
  for (const auto& s : stmts) {
    if (s.kind == Stmt::Kind::Return && s.expr) {
      out.push_back(s.expr.get());
    }
    collect_return_exprs_in_stmts(s.then_body, out);
    if (s.else_body) {
      collect_return_exprs_in_stmts(*s.else_body, out);
    }
    collect_return_exprs_in_stmts(s.while_body, out);
    collect_return_exprs_in_stmts(s.for_body, out);
    collect_return_exprs_in_stmts(s.par_body, out);
  }
}

const Expr* single_return_expr(const ProcDecl& proc) {
  std::vector<const Expr*> returns;
  collect_return_exprs_in_stmts(proc.body, returns);
  if (returns.empty()) {
    return nullptr;
  }
  return returns.back();
}

namespace {

bool numeric_same_value(const Expr& a, const Expr& b) {
  if (a.kind == Expr::Kind::IntLit && b.kind == Expr::Kind::IntLit) {
    return a.int_value == b.int_value;
  }
  if (a.kind == Expr::Kind::FloatLit && b.kind == Expr::Kind::FloatLit) {
    return a.float_value == b.float_value;
  }
  if (a.kind == Expr::Kind::IntLit && b.kind == Expr::Kind::FloatLit) {
    return static_cast<double>(a.int_value) == b.float_value;
  }
  if (a.kind == Expr::Kind::FloatLit && b.kind == Expr::Kind::IntLit) {
    return a.float_value == static_cast<double>(b.int_value);
  }
  return false;
}

bool expr_same_shape(const Expr& a, const Expr& b) {
  if (a.kind != b.kind) {
    return numeric_same_value(a, b);
  }
  switch (a.kind) {
    case Expr::Kind::IntLit:
      return a.int_value == b.int_value;
    case Expr::Kind::FloatLit:
      return a.float_value == b.float_value;
    case Expr::Kind::Ident:
      return a.ident == b.ident;
    case Expr::Kind::Index:
      return a.base && b.base && a.index && b.index && expr_same_shape(*a.base, *b.base) &&
             expr_same_shape(*a.index, *b.index);
    case Expr::Kind::BinOp:
      return a.bin_op == b.bin_op && a.lhs && b.lhs && a.rhs && b.rhs &&
             expr_same_shape(*a.lhs, *b.lhs) && expr_same_shape(*a.rhs, *b.rhs);
    default:
      return false;
  }
}

bool is_true_literal(const Expr& e) {
  return e.kind == Expr::Kind::Ident && e.ident == "true";
}

const Expr* ensures_rhs_eq_result(const Expr& e) {
  if (e.kind != Expr::Kind::BinOp || e.bin_op != BinOp::Eq || !e.lhs || !e.rhs) {
    return nullptr;
  }
  if (e.lhs->kind == Expr::Kind::Ident && e.lhs->ident == "result") {
    return e.rhs.get();
  }
  if (e.rhs->kind == Expr::Kind::Ident && e.rhs->ident == "result") {
    return e.lhs.get();
  }
  return nullptr;
}

std::unique_ptr<Expr> substitute_ident(const Expr& e, const std::string& from,
                                       const std::string& to) {
  auto out = std::make_unique<Expr>();
  out->span = e.span;
  out->kind = e.kind;
  out->int_value = e.int_value;
  out->float_value = e.float_value;
  out->str_value = e.str_value;
  out->ident = e.ident;
  out->bin_op = e.bin_op;
  out->field_name = e.field_name;
  if (e.kind == Expr::Kind::Ident && e.ident == from) {
    out->ident = to;
    return out;
  }
  if (e.lhs) {
    out->lhs = substitute_ident(*e.lhs, from, to);
  }
  if (e.rhs) {
    out->rhs = substitute_ident(*e.rhs, from, to);
  }
  if (e.operand) {
    out->operand = substitute_ident(*e.operand, from, to);
  }
  if (e.base) {
    out->base = substitute_ident(*e.base, from, to);
  }
  if (e.index) {
    out->index = substitute_ident(*e.index, from, to);
  }
  for (const auto& arg : e.args) {
    if (arg) {
      out->args.push_back(substitute_ident(*arg, from, to));
    }
  }
  return out;
}

bool ensures_subst_matches_requires(const ProcDecl& proc, const Contract& ens) {
  if (!ens.expr || ens.kind != ContractKind::Ensures) {
    return false;
  }
  const Expr* ret = single_return_expr(proc);
  if (ret == nullptr || ret->kind != Expr::Kind::Ident) {
    return false;
  }
  const auto sub = substitute_ident(*ens.expr, "result", ret->ident);
  if (!sub) {
    return false;
  }
  for (const auto& rc : proc.contracts) {
    if (rc.kind == ContractKind::Requires && rc.expr && expr_same_shape(*sub, *rc.expr)) {
      return true;
    }
  }
  return false;
}

bool comparison_mirror_requires_ensures(const Expr& req, const Expr& ens,
                                        const std::string& param) {
  if (req.kind != Expr::Kind::BinOp || ens.kind != Expr::Kind::BinOp ||
      req.bin_op != ens.bin_op || !req.lhs || !req.rhs || !ens.lhs || !ens.rhs) {
    return false;
  }
  if (!numeric_same_value(*req.rhs, *ens.rhs)) {
    return false;
  }
  if (req.lhs->kind == Expr::Kind::Ident && req.lhs->ident == param &&
      ens.lhs->kind == Expr::Kind::Ident && ens.lhs->ident == "result") {
    return true;
  }
  if (req.rhs->kind == Expr::Kind::Ident && req.rhs->ident == param &&
      ens.rhs->kind == Expr::Kind::Ident && ens.rhs->ident == "result") {
    return true;
  }
  return false;
}

const MirInsn* last_return_insn(const MirFn& fn) {
  for (auto it = fn.body.rbegin(); it != fn.body.rend(); ++it) {
    switch (it->op) {
      case MirOp::ReturnVoid:
      case MirOp::ReturnInt:
      case MirOp::ReturnFloat:
      case MirOp::ReturnIdent:
      case MirOp::ReturnObject:
        return &*it;
      default:
        break;
    }
  }
  return nullptr;
}

bool callee_ensures_result_equals_param(const ProcDecl& callee, std::size_t param_idx) {
  if (param_idx >= callee.params.size()) {
    return false;
  }
  const std::string& pname = callee.params[param_idx].name;
  for (const auto& rc : callee.contracts) {
    if (rc.kind != ContractKind::Ensures || !rc.expr) {
      continue;
    }
    const Expr* rhs = ensures_rhs_eq_result(*rc.expr);
    if (rhs && rhs->kind == Expr::Kind::Ident && rhs->ident == pname) {
      return true;
    }
  }
  return false;
}

bool call_ident_arg_matches_ensures(const Module& module, const Contract& c,
                                    const Expr& ret, const CallerProofFacts& facts) {
  if (c.kind != ContractKind::Ensures || !c.expr) {
    return false;
  }
  const Expr* rhs = ensures_rhs_eq_result(*c.expr);
  if (rhs == nullptr || ret.kind != Expr::Kind::Call) {
    return false;
  }
  const ProcDecl* callee = find_proc_by_name(module, ret.ident);
  if (callee == nullptr) {
    return false;
  }
  for (std::size_t i = 0; i < ret.args.size() && i < callee->params.size(); ++i) {
    if (!ret.args[i] || ret.args[i]->kind != Expr::Kind::Ident) {
      continue;
    }
    const std::string& arg_id = ret.args[i]->ident;
    if (rhs->kind == Expr::Kind::Ident) {
      if (arg_id != rhs->ident) {
        continue;
      }
    } else if (rhs->kind == Expr::Kind::IntLit) {
      const auto it = facts.const_int_locals.find(arg_id);
      if (it == facts.const_int_locals.end() || it->second != rhs->int_value) {
        continue;
      }
    } else {
      continue;
    }
    if (callee_ensures_result_equals_param(*callee, i)) {
      return true;
    }
  }
  return false;
}

bool call_literal_return_matches_ensures(const Module& module, const Contract& c,
                                         const Expr& ret) {
  if (c.kind != ContractKind::Ensures || !c.expr) {
    return false;
  }
  const Expr* rhs = ensures_rhs_eq_result(*c.expr);
  if (rhs == nullptr || ret.kind != Expr::Kind::Call) {
    return false;
  }
  const ProcDecl* callee = find_proc_by_name(module, ret.ident);
  if (callee == nullptr || ret.args.empty()) {
    return false;
  }
  for (std::size_t i = 0; i < ret.args.size() && i < callee->params.size(); ++i) {
    if (!ret.args[i] || !expr_same_shape(*ret.args[i], *rhs)) {
      continue;
    }
    if (callee_ensures_result_equals_param(*callee, i)) {
      return true;
    }
  }
  return false;
}

// `return read_at(a, 6)` satisfies `ensures result == 7` when `read_at`'s
// ensures is `result == a[i]`, the substitution `a[6]` is a constant array
// element store, and that store equals the caller's ensures constant.
bool call_return_folds_to_ensures(const Module& module, const Contract& c, const Expr& ret,
                                  const CallerProofFacts& facts) {
  if (c.kind != ContractKind::Ensures || !c.expr) {
    return false;
  }
  const Expr* rhs = ensures_rhs_eq_result(*c.expr);
  if (rhs == nullptr || ret.kind != Expr::Kind::Call) {
    return false;
  }
  const ProcDecl* callee = find_proc_by_name(module, ret.ident);
  if (callee == nullptr) {
    return false;
  }
  std::vector<std::string> param_names;
  for (const auto& p : callee->params) {
    param_names.push_back(p.name);
  }
  for (const auto& rc : callee->contracts) {
    if (rc.kind != ContractKind::Ensures || !rc.expr) {
      continue;
    }
    const Expr* crhs = ensures_rhs_eq_result(*rc.expr);
    if (crhs == nullptr) {
      continue;
    }
    std::unique_ptr<Expr> sub = substitute_call_params(*crhs, param_names, ret.args);
    if (!sub) {
      continue;
    }
    std::unique_ptr<Expr> folded =
        fold_const_locals(*sub, facts.const_int_locals, facts.const_float_locals);
    if (folded && (expr_same_shape(*folded, *rhs) || fold_numeric_equal(*folded, *rhs))) {
      return true;
    }
  }
  return false;
}

bool ident_return_matches_const_ensures(const Contract& c, const Expr& ret,
                                        const CallerProofFacts& facts) {
  if (c.kind != ContractKind::Ensures || !c.expr) {
    return false;
  }
  const Expr* rhs = ensures_rhs_eq_result(*c.expr);
  if (rhs == nullptr || ret.kind != Expr::Kind::Ident) {
    return false;
  }
  if (rhs->kind == Expr::Kind::IntLit) {
    const auto it = facts.const_int_locals.find(ret.ident);
    return it != facts.const_int_locals.end() && it->second == rhs->int_value;
  }
  if (rhs->kind == Expr::Kind::FloatLit) {
    const auto it = facts.const_float_locals.find(ret.ident);
    return it != facts.const_float_locals.end() && it->second == rhs->float_value;
  }
  return false;
}

bool expr_is_ident(const Expr* e, const std::string& name) {
  return e != nullptr && e->kind == Expr::Kind::Ident && e->ident == name;
}

bool expr_is_int_lit(const Expr* e, std::int64_t v) {
  return e != nullptr && e->kind == Expr::Kind::IntLit && e->int_value == v;
}

bool expr_is_index_lit_mul(const Expr* e, const std::string& a, const std::string& b,
                           std::int64_t idx) {
  if (e == nullptr || e->kind != Expr::Kind::BinOp || e->bin_op != BinOp::Mul || !e->lhs ||
      !e->rhs) {
    return false;
  }
  const auto lit_index_ok = [&](const Expr* side, const std::string& arr) -> bool {
    return side && side->kind == Expr::Kind::Index && expr_is_ident(side->base.get(), arr) &&
           expr_is_int_lit(side->index.get(), idx);
  };
  return (lit_index_ok(e->lhs.get(), a) && lit_index_ok(e->rhs.get(), b)) ||
         (lit_index_ok(e->lhs.get(), b) && lit_index_ok(e->rhs.get(), a));
}

bool expr_is_index_var_mul(const Expr* e, const std::string& a, const std::string& b,
                           const std::string& i) {
  if (e == nullptr || e->kind != Expr::Kind::BinOp || e->bin_op != BinOp::Mul || !e->lhs ||
      !e->rhs) {
    return false;
  }
  const auto var_index_ok = [&](const Expr* side, const std::string& arr) -> bool {
    return side && side->kind == Expr::Kind::Index && expr_is_ident(side->base.get(), arr) &&
           expr_is_ident(side->index.get(), i);
  };
  return (var_index_ok(e->lhs.get(), a) && var_index_ok(e->rhs.get(), b)) ||
         (var_index_ok(e->lhs.get(), b) && var_index_ok(e->rhs.get(), a));
}

void collect_add_chain_terms(const Expr* e, std::vector<const Expr*>& terms) {
  if (e == nullptr) {
    return;
  }
  if (e->kind == Expr::Kind::BinOp && e->bin_op == BinOp::Add && e->lhs && e->rhs) {
    collect_add_chain_terms(e->lhs.get(), terms);
    collect_add_chain_terms(e->rhs.get(), terms);
    return;
  }
  terms.push_back(e);
}

bool expr_is_dot4_int_spec(const Expr& e, const std::string& a, const std::string& b) {
  std::vector<const Expr*> terms;
  collect_add_chain_terms(&e, terms);
  if (terms.size() != 4) {
    return false;
  }
  for (std::int64_t idx = 0; idx < 4; ++idx) {
    if (!expr_is_index_lit_mul(terms[static_cast<std::size_t>(idx)], a, b, idx)) {
      return false;
    }
  }
  return true;
}

void collect_and_chain_terms(const Expr* e, std::vector<const Expr*>& terms) {
  if (e == nullptr) {
    return;
  }
  if (e->kind == Expr::Kind::BinOp && (e->bin_op == BinOp::And) && e->lhs && e->rhs) {
    collect_and_chain_terms(e->lhs.get(), terms);
    collect_and_chain_terms(e->rhs.get(), terms);
    return;
  }
  terms.push_back(e);
}

bool expr_is_index2d_lit_mul(const Expr* lhs_mul, const std::string& a, const std::string& b,
                             std::int64_t row, std::int64_t k, std::int64_t col) {
  if (lhs_mul == nullptr || lhs_mul->kind != Expr::Kind::BinOp || lhs_mul->bin_op != BinOp::Mul ||
      !lhs_mul->lhs || !lhs_mul->rhs) {
    return false;
  }
  const auto a_ok = [&](const Expr* side) -> bool {
    return side && side->kind == Expr::Kind::Index && side->base &&
           side->base->kind == Expr::Kind::Index && expr_is_ident(side->base->base.get(), a) &&
           expr_is_int_lit(side->base->index.get(), row) &&
           expr_is_int_lit(side->index.get(), k);
  };
  const auto b_ok = [&](const Expr* side) -> bool {
    return side && side->kind == Expr::Kind::Index && side->base &&
           side->base->kind == Expr::Kind::Index && expr_is_ident(side->base->base.get(), b) &&
           expr_is_int_lit(side->base->index.get(), k) &&
           expr_is_int_lit(side->index.get(), col);
  };
  return (a_ok(lhs_mul->lhs.get()) && b_ok(lhs_mul->rhs.get())) ||
         (a_ok(lhs_mul->rhs.get()) && b_ok(lhs_mul->lhs.get()));
}

bool expr_is_mat2_ij_entry_eq(const Expr* e, const std::string& c, const std::string& a,
                              const std::string& b, std::int64_t row, std::int64_t col) {
  if (e == nullptr || e->kind != Expr::Kind::BinOp || e->bin_op != BinOp::Eq || !e->lhs ||
      !e->rhs) {
    return false;
  }
  const Expr* lhs = e->lhs.get();
  const Expr* rhs = e->rhs.get();
  if (lhs->kind != Expr::Kind::Index || !lhs->base || lhs->base->kind != Expr::Kind::Index) {
    return false;
  }
  if (!expr_is_ident(lhs->base->base.get(), c) || !expr_is_int_lit(lhs->base->index.get(), row) ||
      !expr_is_int_lit(lhs->index.get(), col)) {
    return false;
  }
  std::vector<const Expr*> terms;
  collect_add_chain_terms(rhs, terms);
  if (terms.size() != 2) {
    return false;
  }
  return expr_is_index2d_lit_mul(terms[0], a, b, row, 0, col) &&
         expr_is_index2d_lit_mul(terms[1], a, b, row, 1, col);
}

bool expr_is_mat2_int_spec(const Expr& e, const std::string& a, const std::string& b,
                           const std::string& c) {
  std::vector<const Expr*> terms;
  collect_and_chain_terms(&e, terms);
  if (terms.size() != 4) {
    return false;
  }
  const std::pair<std::int64_t, std::int64_t> cells[] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
  for (std::size_t t = 0; t < 4; ++t) {
    if (!expr_is_mat2_ij_entry_eq(terms[t], c, a, b, cells[t].first, cells[t].second)) {
      return false;
    }
  }
  return true;
}


bool expr_is_float_lit(const Expr* e, double v) {
  return e && e->kind == Expr::Kind::FloatLit && std::fabs(e->float_value - v) < 1e-15;
}
bool expr_is_result_sq_minus_x(const Expr* e, const std::string& x) {
  if (!e || e->kind != Expr::Kind::BinOp || e->bin_op != BinOp::Sub || !e->lhs || !e->rhs) {
    return false;
  }
  const Expr& lhs = *e->lhs;
  if (lhs.kind != Expr::Kind::BinOp || lhs.bin_op != BinOp::Mul || !lhs.lhs || !lhs.rhs) {
    return false;
  }
  return expr_is_ident(lhs.lhs.get(), "result") && expr_is_ident(lhs.rhs.get(), "result") &&
         expr_is_ident(e->rhs.get(), x);
}
bool expr_is_sqrt_open_bound_ensures(const Expr& e, const std::string& x) {
  if (e.kind != Expr::Kind::BinOp || e.bin_op != BinOp::Lt || !e.lhs || !e.rhs) {
    return false;
  }
  if (!expr_is_float_lit(e.rhs.get(), 1e-12)) {
    return false;
  }
  const Expr& lhs = *e.lhs;
  if (lhs.kind != Expr::Kind::Call || lhs.ident != "abs" || lhs.args.size() != 1 || !lhs.args[0]) {
    return false;
  }
  return expr_is_result_sq_minus_x(lhs.args[0].get(), x);
}
bool witness_sqrt_open_bound_spec_impl(const ProcDecl& proc, const Expr& ensures_expr) {
  if (proc.params.empty()) {
    return false;
  }
  const std::string x = proc.params[0].name;
  const Expr* ret = single_return_expr(proc);
  if (!ret || ret->kind != Expr::Kind::Call || ret->ident != "li_rt_sqrt" || ret->args.size() != 1 ||
      !ret->args[0]) {
    return false;
  }
  if (!expr_is_ident(ret->args[0].get(), x)) {
    return false;
  }
  return expr_is_sqrt_open_bound_ensures(ensures_expr, x);
}
bool witness_mat2_int_at2_spec_impl(const ProcDecl& proc, const Expr& ensures_expr) {
  const Expr* ret = single_return_expr(proc);
  if (ret == nullptr || ret->kind != Expr::Kind::BinOp || ret->bin_op != BinOp::MatMul ||
      !ret->lhs || !ret->rhs) {
    return false;
  }
  if (ret->lhs->kind != Expr::Kind::Ident || ret->rhs->kind != Expr::Kind::Ident) {
    return false;
  }
  return expr_is_mat2_int_spec(ensures_expr, ret->lhs->ident, ret->rhs->ident, "result");
}

bool stmt_is_i_plus_one(const Stmt& s, const std::string& i) {
  if (s.kind != Stmt::Kind::Assign || !s.init || !s.expr || !expr_is_ident(s.init.get(), i)) {
    return false;
  }
  const Expr& rhs = *s.expr;
  return rhs.kind == Expr::Kind::BinOp && rhs.bin_op == BinOp::Add && rhs.lhs &&
         expr_is_ident(rhs.lhs.get(), i) && rhs.rhs && expr_is_int_lit(rhs.rhs.get(), 1);
}

// Extract the two array idents from a dot4 spec
// `a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]` (either operand
// order per term). Fails unless the shape is exactly that.
bool dot4_spec_array_names(const Expr& e, std::string* a, std::string* b) {
  std::vector<const Expr*> terms;
  collect_add_chain_terms(&e, terms);
  if (terms.size() != 4) {
    return false;
  }
  const Expr* t0 = terms[0];
  if (t0->kind != Expr::Kind::BinOp || t0->bin_op != BinOp::Mul || !t0->lhs || !t0->rhs) {
    return false;
  }
  const auto idx0 = [](const Expr* side) -> bool {
    return side && side->kind == Expr::Kind::Index && side->base &&
           side->base->kind == Expr::Kind::Ident && expr_is_int_lit(side->index.get(), 0);
  };
  if (!idx0(t0->lhs.get()) || !idx0(t0->rhs.get())) {
    return false;
  }
  const std::string n1 = t0->lhs->base->ident;
  const std::string n2 = t0->rhs->base->ident;
  if (n1 == n2) {
    return false;
  }
  for (std::size_t k = 0; k < 4; ++k) {
    if (!expr_is_index_lit_mul(terms[k], n1, n2, static_cast<std::int64_t>(k))) {
      return false;
    }
  }
  *a = n1;
  *b = n2;
  return true;
}

// `acc = acc + a[i] * b[i]` with any accumulator name.
bool stmt_is_accumulate(const Stmt& s, const std::string& a, const std::string& b,
                        const std::string& i, std::string* acc) {
  if (s.kind != Stmt::Kind::Assign || !s.init || !s.expr) {
    return false;
  }
  if (s.init->kind != Expr::Kind::Ident) {
    return false;
  }
  const Expr& rhs = *s.expr;
  if (rhs.kind != Expr::Kind::BinOp || rhs.bin_op != BinOp::Add || !rhs.lhs || !rhs.rhs) {
    return false;
  }
  if (!expr_is_ident(rhs.lhs.get(), s.init->ident)) {
    return false;
  }
  if (!expr_is_index_var_mul(rhs.rhs.get(), a, b, i)) {
    return false;
  }
  *acc = s.init->ident;
  return true;
}

// A handwritten 4-iteration dot loop
// (`while i < 4: acc = acc + a[i] * b[i]; i = i + 1`) satisfies the expanded
// sum spec with any parameter, accumulator, and index names.
bool witness_dot4_int_loop_impl(const ProcDecl& proc, const Expr& ensures_rhs) {
  std::string a;
  std::string b;
  if (!dot4_spec_array_names(ensures_rhs, &a, &b)) {
    return false;
  }
  const Stmt* loop = nullptr;
  for (const auto& st : proc.body) {
    if (st.kind == Stmt::Kind::While) {
      loop = &st;
      break;
    }
  }
  if (loop == nullptr || !loop->cond || loop->cond->kind != Expr::Kind::BinOp ||
      loop->cond->bin_op != BinOp::Lt || !loop->cond->lhs || !loop->cond->rhs) {
    return false;
  }
  if (loop->cond->lhs->kind != Expr::Kind::Ident ||
      loop->cond->rhs->kind != Expr::Kind::IntLit || loop->cond->rhs->int_value != 4) {
    return false;
  }
  const std::string i = loop->cond->lhs->ident;
  std::string acc;
  bool saw_i = false;
  for (const auto& st : loop->while_body) {
    std::string found_acc;
    if (stmt_is_accumulate(st, a, b, i, &found_acc)) {
      acc = found_acc;
    }
    if (stmt_is_i_plus_one(st, i)) {
      saw_i = true;
    }
  }
  if (acc.empty() || !saw_i) {
    return false;
  }
  const Expr* ret = single_return_expr(proc);
  return ret != nullptr && expr_is_ident(ret, acc);
}

// `return x @ y` on arrays satisfies the expanded dot4 sum on x, y.
bool witness_dot4_matmul_impl(const Expr& ret, const Expr& ensures_rhs) {
  if (ret.kind != Expr::Kind::BinOp || ret.bin_op != BinOp::MatMul || !ret.lhs || !ret.rhs) {
    return false;
  }
  if (ret.lhs->kind != Expr::Kind::Ident || ret.rhs->kind != Expr::Kind::Ident) {
    return false;
  }
  std::string a;
  std::string b;
  if (!dot4_spec_array_names(ensures_rhs, &a, &b)) {
    return false;
  }
  return (ret.lhs->ident == a && ret.rhs->ident == b) ||
         (ret.lhs->ident == b && ret.rhs->ident == a);
}

bool witness_dot4_prelude_call_impl(const Expr& ret, const Expr& ensures_rhs) {
  if (ret.kind != Expr::Kind::Call || ret.ident != "dot" || ret.args.size() != 2) {
    return false;
  }
  if (ret.args[0]->kind != Expr::Kind::Ident || ret.args[1]->kind != Expr::Kind::Ident) {
    return false;
  }
  return expr_is_dot4_int_spec(ensures_rhs, ret.args[0]->ident, ret.args[1]->ident);
}

bool witness_direct_call_inherits_callee_ensures(const ProcDecl& proc, const Contract& c,
                                                 const Module& module) {
  if (c.kind != ContractKind::Ensures || !c.expr) {
    return false;
  }
  const Expr* ret = single_return_expr(proc);
  if (ret == nullptr || ret->kind != Expr::Kind::Call) {
    return false;
  }
  const ProcDecl* callee = find_proc_by_name(module, ret->ident);
  if (callee == nullptr) {
    return false;
  }
  for (const auto& rc : callee->contracts) {
    if (rc.kind == ContractKind::Ensures && rc.expr && expr_same_shape(*c.expr, *rc.expr)) {
      return true;
    }
  }
  return false;
}

bool ensures_witnessed_for_return(const ProcDecl& proc, const Contract& c, const Expr& ret,
                                  const Module* module, const CallerProofFacts* caller_facts) {
  const Expr* rhs = ensures_rhs_eq_result(*c.expr);
  if (rhs != nullptr && witness_dot4_int_loop_impl(proc, *rhs)) {
    return true;
  }
  if (rhs != nullptr && witness_dot4_prelude_call_impl(ret, *rhs)) {
    return true;
  }
  if (rhs != nullptr && witness_dot4_matmul_impl(ret, *rhs)) {
    return true;
  }
  if (witness_mat2_int_at2_spec_impl(proc, *c.expr)) {
    return true;
  }
  if (rhs != nullptr && expr_same_shape(ret, *rhs)) {
    return true;
  }
  // `return 1.0 + 1.0 + 1.0` satisfies `ensures result == 3.0` once both
  // sides constant-fold to the same value.
  if (rhs != nullptr && fold_numeric_equal(ret, *rhs)) {
    return true;
  }
  if (module != nullptr && call_literal_return_matches_ensures(*module, c, ret)) {
    return true;
  }
  if (caller_facts != nullptr) {
    if (ident_return_matches_const_ensures(c, ret, *caller_facts)) {
      return true;
    }
    if (module != nullptr && call_ident_arg_matches_ensures(*module, c, ret, *caller_facts)) {
      return true;
    }
    if (module != nullptr && call_return_folds_to_ensures(*module, c, ret, *caller_facts)) {
      return true;
    }
  }
  if (ret.kind == Expr::Kind::Ident) {
    for (const auto& rc : proc.contracts) {
      if (rc.kind == ContractKind::Requires && rc.expr &&
          comparison_mirror_requires_ensures(*rc.expr, *c.expr, ret.ident)) {
        return true;
      }
    }
    if (ensures_subst_matches_requires(proc, c)) {
      return true;
    }
  }
  return false;
}

// ---- literal-range ensures witness ----
// `ensures result >= L and result < U` (or equivalent) closes statically when
// every return expression is an int literal inside [L, U) — e.g. an
// in_bounds() helper whose branches return 0/1. Only pure bound conjunctions
// are witnessed; any other conjunct leaves the obligation open.

bool ensures_conjunct_is_bound(const Expr& e, bool* has_lower, std::int64_t* lower,
                               bool* has_upper, std::int64_t* upper) {
  if (e.kind != Expr::Kind::BinOp || !e.lhs || !e.rhs) {
    return false;
  }
  const bool lhs_result = e.lhs->kind == Expr::Kind::Ident && e.lhs->ident == "result";
  const bool rhs_result = e.rhs->kind == Expr::Kind::Ident && e.rhs->ident == "result";
  if (e.bin_op == BinOp::Ge && lhs_result && e.rhs->kind == Expr::Kind::IntLit) {
    *has_lower = true;
    *lower = e.rhs->int_value;
    return true;
  }
  if (e.bin_op == BinOp::Le && rhs_result && e.lhs->kind == Expr::Kind::IntLit) {
    *has_lower = true;
    *lower = e.lhs->int_value;
    return true;
  }
  if (e.bin_op == BinOp::Lt && lhs_result && e.rhs->kind == Expr::Kind::IntLit) {
    *has_upper = true;
    *upper = e.rhs->int_value - 1;  // result < U  =>  result <= U - 1
    return true;
  }
  if (e.bin_op == BinOp::Le && lhs_result && e.rhs->kind == Expr::Kind::IntLit) {
    *has_upper = true;
    *upper = e.rhs->int_value;
    return true;
  }
  if (e.bin_op == BinOp::Gt && rhs_result && e.lhs->kind == Expr::Kind::IntLit) {
    *has_lower = true;
    *lower = e.lhs->int_value + 1;  // U > result  =>  result >= U + 1
    return true;
  }
  if (e.bin_op == BinOp::Ge && rhs_result && e.lhs->kind == Expr::Kind::IntLit) {
    *has_lower = true;
    *lower = e.lhs->int_value;
    return true;
  }
  return false;
}

bool ensures_is_pure_bound_conjunction(const Expr& e, bool* has_lower, std::int64_t* lower,
                                       bool* has_upper, std::int64_t* upper) {
  if (e.kind == Expr::Kind::BinOp && e.bin_op == BinOp::And && e.lhs && e.rhs) {
    return ensures_is_pure_bound_conjunction(*e.lhs, has_lower, lower, has_upper, upper) &&
           ensures_is_pure_bound_conjunction(*e.rhs, has_lower, lower, has_upper, upper);
  }
  return ensures_conjunct_is_bound(e, has_lower, lower, has_upper, upper);
}

bool witness_ensures_literal_range(const ProcDecl& proc, const Contract& c) {
  if (c.kind != ContractKind::Ensures || !c.expr) {
    return false;
  }
  bool has_lower = false;
  bool has_upper = false;
  std::int64_t lower = 0;
  std::int64_t upper = 0;
  if (!ensures_is_pure_bound_conjunction(*c.expr, &has_lower, &lower, &has_upper, &upper)) {
    return false;
  }
  if (!has_lower || !has_upper) {
    return false;
  }
  std::vector<const Expr*> returns;
  collect_return_exprs_in_stmts(proc.body, returns);
  if (returns.empty()) {
    return false;
  }
  for (const Expr* ret : returns) {
    if (ret->kind != Expr::Kind::IntLit) {
      return false;
    }
    if (ret->int_value < lower || ret->int_value > upper) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool contract_witnessed_trivial(const ProcDecl& proc, const Contract& c, const Module* module,
                                const CallerProofFacts* caller_facts) {
  if (!c.expr) {
    return false;
  }
  if (is_true_literal(*c.expr)) {
    return true;
  }
  if (module != nullptr && witness_direct_call_inherits_callee_ensures(proc, c, *module)) {
    return true;
  }
  if (c.kind != ContractKind::Ensures) {
    return false;
  }
  if (witness_ensures_literal_range(proc, c)) {
    return true;
  }
  std::vector<const Expr*> returns;
  collect_return_exprs_in_stmts(proc.body, returns);
  if (!returns.empty()) {
    for (const Expr* ret : returns) {
      if (ret != nullptr && ensures_witnessed_for_return(proc, c, *ret, module, caller_facts)) {
        return true;
      }
    }
    return false;
  }
  return false;
}

bool mir_return_links_proc(const MirFn& fn, const ProcDecl& proc) {
  const Expr* ast_ret = single_return_expr(proc);
  const MirInsn* mir_ret = last_return_insn(fn);
  if (ast_ret == nullptr || mir_ret == nullptr) {
    return false;
  }
  switch (ast_ret->kind) {
    case Expr::Kind::IntLit:
      return mir_ret->op == MirOp::ReturnInt && mir_ret->int_value == ast_ret->int_value;
    case Expr::Kind::FloatLit:
      return mir_ret->op == MirOp::ReturnFloat && mir_ret->float_value == ast_ret->float_value;
    case Expr::Kind::Ident:
      if (mir_ret->op == MirOp::ReturnIdent) {
        return mir_ret->ident == ast_ret->ident;
      }
      if (mir_ret->op == MirOp::ReturnObject) {
        return mir_ret->ident == (std::string("__li_o_") + ast_ret->ident);
      }
      return false;
    default:
      return false;
  }
}

VcWitnessStats compute_vc_witness_stats(const Module& module, const MirModule* mir) {
  VcWitnessStats stats;
  for (const auto& proc : module.procs) {
    if (proc.is_extern) {
      continue;
    }
    bool has_witnessed_ensure = false;
    for (const auto& c : proc.contracts) {
      if (c.kind == ContractKind::Ensures) {
        ++stats.ensures_total;
        if (contract_witnessed_trivial(proc, c)) {
          ++stats.ensures_witnessed;
          has_witnessed_ensure = true;
        }
      }
    }
    if (mir != nullptr && has_witnessed_ensure) {
      for (const auto& fn : mir->functions) {
        if (fn.name == proc.name && mir_return_links_proc(fn, proc)) {
          ++stats.mir_return_linked;
          break;
        }
      }
    }
  }
  return stats;
}

bool witness_dot4_int_loop(const ProcDecl& proc, const Expr& ensures_rhs) {
  return witness_dot4_int_loop_impl(proc, ensures_rhs);
}

bool witness_dot4_prelude_call(const Expr& ret, const Expr& ensures_rhs) {
  return witness_dot4_prelude_call_impl(ret, ensures_rhs);
}

bool witness_dot4_matmul(const Expr& ret, const Expr& ensures_rhs) {
  return witness_dot4_matmul_impl(ret, ensures_rhs);
}

bool witness_mat2_int_at2_spec(const ProcDecl& proc, const Expr& ensures_expr) {
  return witness_mat2_int_at2_spec_impl(proc, ensures_expr);
}

bool witness_sqrt_open_bound_spec(const ProcDecl& proc, const Expr& ensures_expr) {
  return witness_sqrt_open_bound_spec_impl(proc, ensures_expr);
}

}  // namespace li
