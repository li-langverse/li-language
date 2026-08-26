#include "li/call_requires.hpp"

#include "li/vc_prove.hpp"

#include <optional>
#include <sstream>

namespace li {
namespace {

const char* binop_spelling(BinOp op) {
  switch (op) {
    case BinOp::Eq:
      return "==";
    case BinOp::Ne:
      return "!=";
    case BinOp::Lt:
      return "<";
    case BinOp::Le:
      return "<=";
    case BinOp::Gt:
      return ">";
    case BinOp::Ge:
      return ">=";
    case BinOp::And:
      return "and";
    case BinOp::Or:
      return "or";
    default:
      return "?";
  }
}

std::string false_comparison_phrase(BinOp op, std::int64_t lhs, std::int64_t rhs) {
  std::ostringstream os;
  os << lhs << ' ' << binop_spelling(op) << ' ' << rhs << " is not true";
  return os.str();
}

std::string suggest_fix_for_int_comparison(BinOp op, std::int64_t lhs, std::int64_t rhs) {
  if (op == BinOp::Ge && lhs < 0 && rhs == 0) {
    return "The value " + std::to_string(lhs) + " is negative. Use zero or a positive number "
           "(for example `0` or `5`).";
  }
  if (op == BinOp::Gt && lhs <= rhs) {
    return "The value " + std::to_string(lhs) + " must be greater than " + std::to_string(rhs) +
           ".";
  }
  if (op == BinOp::Le && lhs > rhs) {
    return "The value " + std::to_string(lhs) + " must be less than or equal to " +
           std::to_string(rhs) + ".";
  }
  if (op == BinOp::Lt && lhs >= rhs) {
    return "The value " + std::to_string(lhs) + " must be less than " + std::to_string(rhs) + ".";
  }
  if (op == BinOp::Eq) {
    return "Use " + std::to_string(rhs) + " instead of " + std::to_string(lhs) + ", or change the "
           "callee's `requires` if the rule is wrong.";
  }
  return "Change the argument so the condition holds, or update the callee's `requires` clause "
         "if the rule is wrong.";
}

std::string suggest_fix_for_refinement(BinOp op, std::int64_t lhs, std::int64_t rhs) {
  if (op == BinOp::Ge && lhs < 0 && rhs == 0) {
    return "The value " + std::to_string(lhs) + " is negative. Use zero or a positive number, or "
           "widen the refinement type if the bound is wrong.";
  }
  if (op == BinOp::Gt && lhs <= rhs) {
    return "The value " + std::to_string(lhs) + " must be greater than " + std::to_string(rhs) +
           " for this refinement type.";
  }
  if (op == BinOp::Le && lhs > rhs) {
    return "The value " + std::to_string(lhs) + " must be at most " + std::to_string(rhs) +
           " for this refinement type.";
  }
  if (op == BinOp::Lt && lhs >= rhs) {
    return "The value " + std::to_string(lhs) + " must be less than " + std::to_string(rhs) +
           " for this refinement type.";
  }
  return "Use a value inside the declared range, or relax the refinement on the type alias.";
}

std::unique_ptr<Expr> clone_expr(const Expr& e) {
  auto out = std::make_unique<Expr>();
  out->span = e.span;
  out->kind = e.kind;
  out->int_value = e.int_value;
  out->float_value = e.float_value;
  out->str_value = e.str_value;
  out->ident = e.ident;
  out->bin_op = e.bin_op;
  out->field_name = e.field_name;
  if (e.lhs) {
    out->lhs = clone_expr(*e.lhs);
  }
  if (e.rhs) {
    out->rhs = clone_expr(*e.rhs);
  }
  if (e.operand) {
    out->operand = clone_expr(*e.operand);
  }
  if (e.base) {
    out->base = clone_expr(*e.base);
  }
  if (e.index) {
    out->index = clone_expr(*e.index);
  }
  for (const auto& arg : e.args) {
    if (arg) {
      out->args.push_back(clone_expr(*arg));
    }
  }
  return out;
}

std::optional<std::int64_t> int_lit_value(const Expr& e) {
  if (e.kind == Expr::Kind::IntLit) {
    return e.int_value;
  }
  return std::nullopt;
}

std::optional<double> float_lit_value(const Expr& e) {
  if (e.kind == Expr::Kind::FloatLit) {
    return e.float_value;
  }
  return std::nullopt;
}

bool compare_int_literals(BinOp op, std::int64_t lhs, std::int64_t rhs) {
  switch (op) {
    case BinOp::Eq:
      return lhs == rhs;
    case BinOp::Ne:
      return lhs != rhs;
    case BinOp::Lt:
      return lhs < rhs;
    case BinOp::Le:
      return lhs <= rhs;
    case BinOp::Gt:
      return lhs > rhs;
    case BinOp::Ge:
      return lhs >= rhs;
    default:
      return false;
  }
}

bool compare_num_literals(BinOp op, double lhs, double rhs) {
  switch (op) {
    case BinOp::Eq:
      return lhs == rhs;
    case BinOp::Ne:
      return lhs != rhs;
    case BinOp::Lt:
      return lhs < rhs;
    case BinOp::Le:
      return lhs <= rhs;
    case BinOp::Gt:
      return lhs > rhs;
    case BinOp::Ge:
      return lhs >= rhs;
    default:
      return false;
  }
}

// A numeric literal (int or float) as a double, for `8.0 == 8`-style checks.
std::optional<double> num_lit_value(const Expr& e) {
  if (const auto iv = int_lit_value(e)) {
    return static_cast<double>(*iv);
  }
  return float_lit_value(e);
}

}  // namespace

std::unique_ptr<Expr> substitute_refinement_binding(const Expr& predicate,
                                                    const std::string& bind_var,
                                                    const Expr& arg_value) {
  const std::vector<std::string> names{bind_var};
  std::vector<std::unique_ptr<Expr>> args;
  args.push_back(clone_expr(arg_value));
  return substitute_call_params(predicate, names, args);
}

const ProcDecl* find_proc_by_name(const Module& module, const std::string& name) {
  for (const auto& proc : module.procs) {
    if (proc.name == name) {
      return &proc;
    }
  }
  return nullptr;
}

std::unique_ptr<Expr> substitute_call_params(
    const Expr& expr, const std::vector<std::string>& param_names,
    const std::vector<std::unique_ptr<Expr>>& args) {
  auto out = clone_expr(expr);
  if (out->kind == Expr::Kind::Ident) {
    for (std::size_t i = 0; i < param_names.size() && i < args.size(); ++i) {
      if (out->ident == param_names[i] && args[i]) {
        return clone_expr(*args[i]);
      }
    }
    return out;
  }
  if (out->lhs) {
    out->lhs = substitute_call_params(*out->lhs, param_names, args);
  }
  if (out->rhs) {
    out->rhs = substitute_call_params(*out->rhs, param_names, args);
  }
  if (out->operand) {
    out->operand = substitute_call_params(*out->operand, param_names, args);
  }
  if (out->base) {
    out->base = substitute_call_params(*out->base, param_names, args);
  }
  if (out->index) {
    out->index = substitute_call_params(*out->index, param_names, args);
  }
  for (auto& arg : out->args) {
    if (arg) {
      arg = substitute_call_params(*arg, param_names, args);
    }
  }
  return out;
}

std::optional<std::string> object_field_const_key(const Expr& e) {
  if (e.kind != Expr::Kind::FieldAccess || !e.base) {
    return std::nullopt;
  }
  const Expr* root = e.base.get();
  while (root && root->kind == Expr::Kind::FieldAccess) {
    root = root->base.get();
  }
  if (!root || root->kind != Expr::Kind::Ident) {
    return std::nullopt;
  }
  return root->ident + "." + e.field_name;
}

std::optional<std::string> array_index_const_key(const Expr& e) {
  // Canonical key for `a[0]` and nested `a[0][1]` (2D tiles): walk the
  // index chain to the root ident, requiring every index to be a literal.
  std::string key;
  const Expr* cur = &e;
  while (cur != nullptr && cur->kind == Expr::Kind::Index && cur->base && cur->index) {
    if (cur->index->kind != Expr::Kind::IntLit) {
      return std::nullopt;
    }
    key = "[" + std::to_string(cur->index->int_value) + "]" + key;
    cur = cur->base.get();
  }
  if (cur == nullptr || cur->kind != Expr::Kind::Ident || key.empty()) {
    return std::nullopt;
  }
  return cur->ident + key;
}

void note_object_field_const_assign(const Expr& lhs, const Expr& rhs,
                                    std::map<std::string, std::int64_t>& const_int_locals) {
  const auto key = object_field_const_key(lhs);
  if (!key) {
    return;
  }
  if (rhs.kind == Expr::Kind::IntLit) {
    const_int_locals[*key] = rhs.int_value;
    return;
  }
  if (rhs.kind == Expr::Kind::Ident) {
    const auto it = const_int_locals.find(rhs.ident);
    if (it != const_int_locals.end()) {
      const_int_locals[*key] = it->second;
    }
    const auto fit = object_field_const_key(rhs);
    if (fit) {
      const auto it2 = const_int_locals.find(*fit);
      if (it2 != const_int_locals.end()) {
        const_int_locals[*key] = it2->second;
      }
    }
  }
}

void note_array_index_const_assign(const Expr& lhs, const Expr& rhs,
                                   std::map<std::string, std::int64_t>& const_int_locals) {
  const auto key = array_index_const_key(lhs);
  if (!key) {
    return;
  }
  if (rhs.kind == Expr::Kind::IntLit) {
    const_int_locals[*key] = rhs.int_value;
    return;
  }
  if (rhs.kind == Expr::Kind::Ident) {
    const auto it = const_int_locals.find(rhs.ident);
    if (it != const_int_locals.end()) {
      const_int_locals[*key] = it->second;
    }
  }
}

std::unique_ptr<Expr> fold_const_int_locals(
    const Expr& expr, const std::map<std::string, std::int64_t>& const_int_locals) {
  if (expr.kind == Expr::Kind::FieldAccess) {
    const auto key = object_field_const_key(expr);
    if (key) {
      const auto it = const_int_locals.find(*key);
      if (it != const_int_locals.end()) {
        auto lit = std::make_unique<Expr>();
        lit->kind = Expr::Kind::IntLit;
        lit->span = expr.span;
        lit->int_value = it->second;
        return lit;
      }
    }
  }
  if (expr.kind == Expr::Kind::Index) {
    const auto key = array_index_const_key(expr);
    if (key) {
      const auto it = const_int_locals.find(*key);
      if (it != const_int_locals.end()) {
        auto lit = std::make_unique<Expr>();
        lit->kind = Expr::Kind::IntLit;
        lit->span = expr.span;
        lit->int_value = it->second;
        return lit;
      }
    }
  }
  if (expr.kind == Expr::Kind::Ident) {
    const auto it = const_int_locals.find(expr.ident);
    if (it != const_int_locals.end()) {
      auto lit = std::make_unique<Expr>();
      lit->kind = Expr::Kind::IntLit;
      lit->span = expr.span;
      lit->int_value = it->second;
      return lit;
    }
    return clone_expr(expr);
  }
  auto out = clone_expr(expr);
  if (out->lhs) {
    out->lhs = fold_const_int_locals(*out->lhs, const_int_locals);
  }
  if (out->rhs) {
    out->rhs = fold_const_int_locals(*out->rhs, const_int_locals);
  }
  if (out->operand) {
    out->operand = fold_const_int_locals(*out->operand, const_int_locals);
  }
  if (out->base) {
    out->base = fold_const_int_locals(*out->base, const_int_locals);
  }
  if (out->index) {
    out->index = fold_const_int_locals(*out->index, const_int_locals);
  }
  for (auto& arg : out->args) {
    if (arg) {
      arg = fold_const_int_locals(*arg, const_int_locals);
    }
  }
  return out;
}

std::unique_ptr<Expr> fold_const_locals(
    const Expr& expr, const std::map<std::string, std::int64_t>& const_int_locals,
    const std::map<std::string, double>& const_float_locals) {
  auto fold_one = [&](const Expr& e) -> std::unique_ptr<Expr> {
    // Fold a leaf: const ident, `a[0]` int/float array store, `o.f` field.
    std::unique_ptr<Expr> lit;
    auto mk_lit = [&](Expr::Kind kind, std::int64_t iv, double fv) {
      lit = std::make_unique<Expr>();
      lit->kind = kind;
      lit->span = e.span;
      lit->int_value = iv;
      lit->float_value = fv;
    };
    if (e.kind == Expr::Kind::Ident) {
      const auto it = const_int_locals.find(e.ident);
      if (it != const_int_locals.end()) {
        mk_lit(Expr::Kind::IntLit, it->second, 0.0);
        return lit;
      }
      const auto ft = const_float_locals.find(e.ident);
      if (ft != const_float_locals.end()) {
        mk_lit(Expr::Kind::FloatLit, 0, ft->second);
        return lit;
      }
      return clone_expr(e);
    }
    if (e.kind == Expr::Kind::Index || e.kind == Expr::Kind::FieldAccess) {
      const auto key =
          e.kind == Expr::Kind::Index ? array_index_const_key(e) : object_field_const_key(e);
      if (key) {
        const auto it = const_int_locals.find(*key);
        if (it != const_int_locals.end()) {
          mk_lit(Expr::Kind::IntLit, it->second, 0.0);
          return lit;
        }
        const auto ft = const_float_locals.find(*key);
        if (ft != const_float_locals.end()) {
          mk_lit(Expr::Kind::FloatLit, 0, ft->second);
          return lit;
        }
      }
    }
    return nullptr;
  };
  auto out = clone_expr(expr);
  if (out->kind == Expr::Kind::Ident || out->kind == Expr::Kind::Index ||
      out->kind == Expr::Kind::FieldAccess) {
    if (auto lit = fold_one(*out)) {
      return lit;
    }
  }
  if (out->lhs) {
    out->lhs = fold_const_locals(*out->lhs, const_int_locals, const_float_locals);
  }
  if (out->rhs) {
    out->rhs = fold_const_locals(*out->rhs, const_int_locals, const_float_locals);
  }
  if (out->operand) {
    out->operand = fold_const_locals(*out->operand, const_int_locals, const_float_locals);
  }
  if (out->base) {
    out->base = fold_const_locals(*out->base, const_int_locals, const_float_locals);
  }
  if (out->index) {
    out->index = fold_const_locals(*out->index, const_int_locals, const_float_locals);
  }
  for (auto& arg : out->args) {
    if (arg) {
      arg = fold_const_locals(*arg, const_int_locals, const_float_locals);
    }
  }
  // Collapse the substituted literal arithmetic (e.g. `1.0 * 2.0 + ...`).
  const FoldVal folded = fold_const(*out);
  if (folded.ok) {
    auto lit = std::make_unique<Expr>();
    lit->kind = folded.is_float ? Expr::Kind::FloatLit : Expr::Kind::IntLit;
    lit->span = out->span;
    lit->int_value = folded.iv;
    lit->float_value = folded.fv;
    return lit;
  }
  return out;
}

std::unique_ptr<Expr> fold_facts_expr(const Expr& e, const ProofFacts& facts) {
  static const std::map<std::string, double> kNoFloatFacts;
  return fold_const_locals(e, facts.const_int_locals,
                           facts.const_float_locals ? *facts.const_float_locals : kNoFloatFacts);
}

bool expr_statically_true(const Expr& e) {
  if (e.kind == Expr::Kind::Ident && e.ident == "true") {
    return true;
  }
  if (e.kind != Expr::Kind::BinOp || !e.lhs || !e.rhs) {
    return false;
  }
  const auto li = num_lit_value(*e.lhs);
  const auto ri = num_lit_value(*e.rhs);
  if (!li || !ri) {
    return false;
  }
  return compare_num_literals(e.bin_op, *li, *ri);
}

bool expr_statically_false(const Expr& e) {
  if (e.kind == Expr::Kind::Ident && e.ident == "false") {
    return true;
  }
  if (e.kind != Expr::Kind::BinOp || !e.lhs || !e.rhs) {
    return false;
  }
  const auto li = num_lit_value(*e.lhs);
  const auto ri = num_lit_value(*e.rhs);
  if (!li || !ri) {
    return false;
  }
  return !compare_num_literals(e.bin_op, *li, *ri);
}

std::string call_to_user_string(const Expr& call);

std::string expr_to_user_string(const Expr& e) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      return std::to_string(e.int_value);
    case Expr::Kind::FloatLit: {
      std::ostringstream os;
      os << e.float_value;
      return os.str();
    }
    case Expr::Kind::Ident:
      if (e.ident == "true" || e.ident == "false") {
        return e.ident;
      }
      return e.ident;
    case Expr::Kind::UnaryNot:
      if (e.operand) {
        return "not " + expr_to_user_string(*e.operand);
      }
      return "not ?";
    case Expr::Kind::UnaryMinus:
      if (e.operand) {
        return "-" + expr_to_user_string(*e.operand);
      }
      return "-?";
    case Expr::Kind::BinOp:
      if (e.lhs && e.rhs) {
        return expr_to_user_string(*e.lhs) + std::string(" ") + binop_spelling(e.bin_op) +
               std::string(" ") + expr_to_user_string(*e.rhs);
      }
      return "?";
    case Expr::Kind::Call:
      return call_to_user_string(e);
    default:
      return "?";
  }
}

std::string call_to_user_string(const Expr& call) {
  if (call.kind != Expr::Kind::Call) {
    return "?";
  }
  std::ostringstream os;
  os << call.ident << '(';
  for (std::size_t i = 0; i < call.args.size(); ++i) {
    if (i > 0) {
      os << ", ";
    }
    if (call.args[i]) {
      os << expr_to_user_string(*call.args[i]);
    } else {
      os << '?';
    }
  }
  os << ')';
  return os.str();
}

bool is_ge_zero_for_ident(const Expr& e, const std::string& ident) {
  if (e.kind != Expr::Kind::BinOp || e.bin_op != BinOp::Ge || !e.lhs || !e.rhs) {
    return false;
  }
  if (e.lhs->kind != Expr::Kind::Ident || e.lhs->ident != ident) {
    return false;
  }
  const auto rhs = int_lit_value(*e.rhs);
  return rhs && *rhs == 0;
}

bool folded_discharged_by_proof_facts(const Expr& folded, const ProofFacts& facts) {
  if (folded.kind != Expr::Kind::BinOp || !folded.lhs || folded.lhs->kind != Expr::Kind::Ident) {
    return false;
  }
  const std::string& id = folded.lhs->ident;
  if (!is_ge_zero_for_ident(folded, id)) {
    return false;
  }
  if (facts.assum_nonneg_ints.count(id) > 0) {
    return true;
  }
  const auto it = facts.const_int_locals.find(id);
  return it != facts.const_int_locals.end() && it->second >= 0;
}

void note_nonneg_assumption_from_cond(const Expr& cond, std::set<std::string>& out) {
  if (cond.kind == Expr::Kind::BinOp && cond.bin_op == BinOp::And && cond.lhs && cond.rhs) {
    note_nonneg_assumption_from_cond(*cond.lhs, out);
    note_nonneg_assumption_from_cond(*cond.rhs, out);
    return;
  }
  if (cond.kind != Expr::Kind::BinOp || !cond.lhs || !cond.rhs) {
    return;
  }
  if (cond.bin_op == BinOp::Ge) {
    if (cond.lhs->kind == Expr::Kind::Ident) {
      const auto rhs = int_lit_value(*cond.rhs);
      if (rhs && *rhs == 0) {
        out.insert(cond.lhs->ident);
      }
    }
    if (cond.rhs->kind == Expr::Kind::Ident) {
      const auto lhs = int_lit_value(*cond.lhs);
      if (lhs && *lhs == 0) {
        out.insert(cond.rhs->ident);
      }
    }
  }
}

std::vector<std::unique_ptr<Expr>> method_call_arg_list(
    const Expr& receiver, const std::vector<std::unique_ptr<Expr>>& method_args) {
  std::vector<std::unique_ptr<Expr>> args;
  args.push_back(clone_expr(receiver));
  for (const auto& a : method_args) {
    if (a) {
      args.push_back(clone_expr(*a));
    } else {
      args.push_back(nullptr);
    }
  }
  return args;
}

RequiresCheckResult check_requires_with_subst_args(
    const ProcDecl& callee, const std::vector<std::unique_ptr<Expr>>& args,
    const ProofFacts& facts) {
  std::vector<std::string> param_names;
  for (const auto& p : callee.params) {
    param_names.push_back(p.name);
  }
  bool any_unknown = false;
  bool any_violated = false;
  for (const auto& rc : callee.contracts) {
    if (rc.kind != ContractKind::Requires || !rc.expr) {
      continue;
    }
    const auto sub = substitute_call_params(*rc.expr, param_names, args);
    const auto folded = fold_facts_expr(*sub, facts);
    if (expr_statically_true(*folded) || folded_discharged_by_proof_facts(*folded, facts)) {
      continue;
    }
    if (expr_statically_false(*folded)) {
      any_violated = true;
      continue;
    }
    any_unknown = true;
  }
  if (any_violated) {
    return RequiresCheckResult::Violated;
  }
  if (any_unknown) {
    return RequiresCheckResult::Unknown;
  }
  return RequiresCheckResult::Satisfied;
}

RequiresCheckResult check_requires_at_call(const ProcDecl& callee, const Expr& call,
                                           const ProofFacts& facts) {
  if (call.kind != Expr::Kind::Call) {
    return RequiresCheckResult::Unknown;
  }
  return check_requires_with_subst_args(callee, call.args, facts);
}

RequiresCheckResult check_requires_at_method_call(
    const ProcDecl& callee, const Expr& receiver,
    const std::vector<std::unique_ptr<Expr>>& method_args, const ProofFacts& facts) {
  return check_requires_with_subst_args(callee, method_call_arg_list(receiver, method_args), facts);
}

std::string method_call_to_user_string(const Expr& receiver, const std::string& method,
                                       const std::vector<std::unique_ptr<Expr>>& method_args) {
  std::ostringstream os;
  os << expr_to_user_string(receiver) << '.' << method << '(';
  for (std::size_t i = 0; i < method_args.size(); ++i) {
    if (i > 0) {
      os << ", ";
    }
    if (method_args[i]) {
      os << expr_to_user_string(*method_args[i]);
    } else {
      os << '?';
    }
  }
  os << ')';
  return os.str();
}

std::optional<RequiresViolationExplanation> explain_requires_violation_with_args(
    const ProcDecl& callee, const std::vector<std::unique_ptr<Expr>>& args,
    const std::string& call_text, const ProofFacts& facts) {
  std::vector<std::string> param_names;
  for (const auto& p : callee.params) {
    param_names.push_back(p.name);
  }
  for (const auto& rc : callee.contracts) {
    if (rc.kind != ContractKind::Requires || !rc.expr) {
      continue;
    }
    const std::string rule_text = expr_to_user_string(*rc.expr);
    const auto sub = substitute_call_params(*rc.expr, param_names, args);
    const auto folded = fold_facts_expr(*sub, facts);
    if (!expr_statically_false(*folded) || folded_discharged_by_proof_facts(*folded, facts)) {
      continue;
    }
    RequiresViolationExplanation out;
    const std::string check_text = expr_to_user_string(*folded);
    std::ostringstream msg;
    msg << "Cannot call `" << call_text << "`: `" << callee.name
        << "` requires `" << rule_text << "` before it runs";
    if (check_text != rule_text) {
      msg << ", but here that means `" << check_text << "`";
    }
    msg << ", which is not satisfied";
    if (folded->kind == Expr::Kind::BinOp && folded->lhs && folded->rhs) {
      const auto li = int_lit_value(*folded->lhs);
      const auto ri = int_lit_value(*folded->rhs);
      if (li && ri) {
        msg << " (" << false_comparison_phrase(folded->bin_op, *li, *ri) << ")";
      }
    }
    msg << '.';
    out.message = msg.str();
    std::ostringstream hint;
    hint << "A `requires` clause is a precondition — it must hold for this call. ";
    if (folded->kind == Expr::Kind::BinOp && folded->lhs && folded->rhs) {
      const auto li = int_lit_value(*folded->lhs);
      const auto ri = int_lit_value(*folded->rhs);
      if (li && ri) {
        hint << suggest_fix_for_int_comparison(folded->bin_op, *li, *ri);
      } else {
        hint << "Change the argument so the condition holds, or update the callee's `requires` "
                "if the rule is wrong.";
      }
    } else {
      hint << "Change the argument so the condition holds, or update the callee's `requires` "
              "if the rule is wrong.";
    }
    out.hint = hint.str();
    return out;
  }
  return std::nullopt;
}

std::optional<RequiresViolationExplanation> explain_requires_violation(const ProcDecl& callee,
                                                                       const Expr& call,
                                                                       const ProofFacts& facts) {
  if (call.kind != Expr::Kind::Call) {
    return std::nullopt;
  }
  return explain_requires_violation_with_args(callee, call.args, call_to_user_string(call), facts);
}

std::optional<RequiresViolationExplanation> explain_requires_violation_method(
    const ProcDecl& callee, const Expr& receiver, const std::string& method_name,
    const std::vector<std::unique_ptr<Expr>>& method_args, const ProofFacts& facts) {
  return explain_requires_violation_with_args(
      callee, method_call_arg_list(receiver, method_args),
      method_call_to_user_string(receiver, method_name, method_args), facts);
}

std::optional<ResolvedRefinement> resolve_refinement_on_type(const TypeExpr& te,
                                                             AliasTypeLookup lookup) {
  if (te.kind == TypeKind::Refinement) {
    if (!te.refinement_pred) {
      return std::nullopt;
    }
    ResolvedRefinement out;
    out.bind_var = te.refinement_var;
    out.type_label =
        "{" + te.refinement_var + " | " + expr_to_user_string(*te.refinement_pred) + "}";
    out.predicate = te.refinement_pred.get();
    return out;
  }
  if (te.kind == TypeKind::Named) {
    const TypeExpr* def = lookup(te.name);
    if (def == nullptr) {
      return std::nullopt;
    }
    auto resolved = resolve_refinement_on_type(*def, lookup);
    if (resolved) {
      resolved->type_label = te.name;
    }
    return resolved;
  }
  return std::nullopt;
}

RequiresCheckResult check_refinement_argument(const ResolvedRefinement& refinement,
                                              const Expr& arg, const ProofFacts& facts) {
  if (!refinement.predicate) {
    return RequiresCheckResult::Unknown;
  }
  const auto sub = substitute_refinement_binding(*refinement.predicate, refinement.bind_var, arg);
  const auto folded = fold_facts_expr(*sub, facts);
  if (expr_statically_true(*folded) || folded_discharged_by_proof_facts(*folded, facts)) {
    return RequiresCheckResult::Satisfied;
  }
  if (expr_statically_false(*folded)) {
    return RequiresCheckResult::Violated;
  }
  return RequiresCheckResult::Unknown;
}

std::optional<RequiresViolationExplanation> explain_refinement_violation(
    const ResolvedRefinement& refinement, const Expr& arg, const ProofFacts& facts) {
  if (!refinement.predicate) {
    return std::nullopt;
  }
  const std::string value_text = expr_to_user_string(arg);
  const std::string rule_text = expr_to_user_string(*refinement.predicate);
  const auto sub = substitute_refinement_binding(*refinement.predicate, refinement.bind_var, arg);
  const auto folded = fold_facts_expr(*sub, facts);
  if (!expr_statically_false(*folded) || folded_discharged_by_proof_facts(*folded, facts)) {
    return std::nullopt;
  }
  RequiresViolationExplanation out;
  const std::string check_text = expr_to_user_string(*folded);
  std::ostringstream msg;
  msg << "Value `" << value_text << "` does not satisfy refinement type `"
      << refinement.type_label << "` (`" << rule_text << "`";
  if (check_text != rule_text) {
    msg << "; here that means `" << check_text << "`";
  }
  msg << "), which is not satisfied";
  if (folded->kind == Expr::Kind::BinOp && folded->lhs && folded->rhs) {
    const auto li = int_lit_value(*folded->lhs);
    const auto ri = int_lit_value(*folded->rhs);
    if (li && ri) {
      msg << " (" << false_comparison_phrase(folded->bin_op, *li, *ri) << ")";
    }
  }
  msg << '.';
  out.message = msg.str();
  std::ostringstream hint;
  hint << "A refinement type declares which values are allowed (e.g. non-negative integers). ";
  if (folded->kind == Expr::Kind::BinOp && folded->lhs && folded->rhs) {
    const auto li = int_lit_value(*folded->lhs);
    const auto ri = int_lit_value(*folded->rhs);
    if (li && ri) {
      hint << suggest_fix_for_refinement(folded->bin_op, *li, *ri);
    } else {
      hint << "Use a value inside the declared range, or relax the refinement on the type alias.";
    }
  } else {
    hint << "Use a value inside the declared range, or relax the refinement on the type alias.";
  }
  out.hint = hint.str();
  return out;
}

void collect_method_calls_in_expr(const Expr& e, std::vector<const Expr*>& out) {
  if (e.kind == Expr::Kind::MethodCall) {
    out.push_back(&e);
  }
  if (e.lhs) {
    collect_method_calls_in_expr(*e.lhs, out);
  }
  if (e.rhs) {
    collect_method_calls_in_expr(*e.rhs, out);
  }
  if (e.operand) {
    collect_method_calls_in_expr(*e.operand, out);
  }
  if (e.base) {
    collect_method_calls_in_expr(*e.base, out);
  }
  if (e.index) {
    collect_method_calls_in_expr(*e.index, out);
  }
  for (const auto& arg : e.args) {
    if (arg) {
      collect_method_calls_in_expr(*arg, out);
    }
  }
}

void collect_idents_in_expr(const Expr& e, std::set<std::string>& out) {
  if (e.kind == Expr::Kind::Ident) {
    out.insert(e.ident);
    return;
  }
  if (e.lhs) {
    collect_idents_in_expr(*e.lhs, out);
  }
  if (e.rhs) {
    collect_idents_in_expr(*e.rhs, out);
  }
  if (e.operand) {
    collect_idents_in_expr(*e.operand, out);
  }
  if (e.base) {
    collect_idents_in_expr(*e.base, out);
  }
  if (e.index) {
    collect_idents_in_expr(*e.index, out);
  }
  for (const auto& arg : e.args) {
    if (arg) {
      collect_idents_in_expr(*arg, out);
    }
  }
}

void collect_calls_in_expr(const Expr& e, std::vector<const Expr*>& out) {
  if (e.kind == Expr::Kind::Call) {
    out.push_back(&e);
  }
  if (e.lhs) {
    collect_calls_in_expr(*e.lhs, out);
  }
  if (e.rhs) {
    collect_calls_in_expr(*e.rhs, out);
  }
  if (e.operand) {
    collect_calls_in_expr(*e.operand, out);
  }
  if (e.base) {
    collect_calls_in_expr(*e.base, out);
  }
  if (e.index) {
    collect_calls_in_expr(*e.index, out);
  }
  for (const auto& arg : e.args) {
    if (arg) {
      collect_calls_in_expr(*arg, out);
    }
  }
}

void collect_method_calls_in_stmts(const std::vector<Stmt>& stmts,
                                   std::vector<const Expr*>& out) {
  for (const auto& s : stmts) {
    if (s.expr) {
      collect_method_calls_in_expr(*s.expr, out);
    }
    if (s.init) {
      collect_method_calls_in_expr(*s.init, out);
    }
    if (s.cond) {
      collect_method_calls_in_expr(*s.cond, out);
    }
    collect_method_calls_in_stmts(s.then_body, out);
    if (s.else_body) {
      collect_method_calls_in_stmts(*s.else_body, out);
    }
    collect_method_calls_in_stmts(s.while_body, out);
    collect_method_calls_in_stmts(s.for_body, out);
    collect_method_calls_in_stmts(s.par_body, out);
  }
}

void collect_calls_in_stmts(const std::vector<Stmt>& stmts,
                            std::vector<const Expr*>& out) {
  for (const auto& s : stmts) {
    if (s.expr) {
      collect_calls_in_expr(*s.expr, out);
    }
    if (s.init) {
      collect_calls_in_expr(*s.init, out);
    }
    if (s.cond) {
      collect_calls_in_expr(*s.cond, out);
    }
    collect_calls_in_stmts(s.then_body, out);
    if (s.else_body) {
      collect_calls_in_stmts(*s.else_body, out);
    }
    collect_calls_in_stmts(s.while_body, out);
    collect_calls_in_stmts(s.for_body, out);
    collect_calls_in_stmts(s.par_body, out);
  }
}

std::unique_ptr<Expr> subst_ident_lit(const Expr& e, const std::string& from, std::int64_t to) {
  auto out = clone_expr(e);
  if (out->kind == Expr::Kind::Ident && out->ident == from) {
    out->kind = Expr::Kind::IntLit;
    out->int_value = to;
    return out;
  }
  if (out->lhs) {
    out->lhs = subst_ident_lit(*out->lhs, from, to);
  }
  if (out->rhs) {
    out->rhs = subst_ident_lit(*out->rhs, from, to);
  }
  if (out->operand) {
    out->operand = subst_ident_lit(*out->operand, from, to);
  }
  if (out->base) {
    out->base = subst_ident_lit(*out->base, from, to);
  }
  if (out->index) {
    out->index = subst_ident_lit(*out->index, from, to);
  }
  for (auto& arg : out->args) {
    if (arg) {
      arg = subst_ident_lit(*arg, from, to);
    }
  }
  return out;
}

// `name = f(args)`: fold the callee's `ensures` conjuncts (`result == <expr>`
// for scalars, `result[i][j] == <expr>` for array tiles) with the current
// facts; each conjunct that collapses to a constant is recorded under the
// corresponding key (`name`, `name[i][j]`). Returns true if any recorded.
bool try_fold_call_to_const(const std::string& name, const Expr& call,
                            std::map<std::string, std::int64_t>& const_int_locals,
                            std::map<std::string, double>& const_float_locals,
                            const std::function<const ProcDecl*(const std::string&)>& lookup) {
  if (call.kind != Expr::Kind::Call) {
    return false;
  }
  const ProcDecl* callee = lookup(call.ident);
  if (callee == nullptr) {
    return false;
  }
  std::vector<std::string> param_names;
  for (const auto& p : callee->params) {
    param_names.push_back(p.name);
  }
  const auto record = [&](const std::string& key, const Expr& body) {
    const auto sub = substitute_call_params(body, param_names, call.args);
    const auto folded = fold_const_locals(*sub, const_int_locals, const_float_locals);
    const FoldVal fv = fold_const(*folded);
    if (!fv.ok) {
      return false;
    }
    if (fv.is_float) {
      const_float_locals[key] = fv.fv;
    } else {
      const_int_locals[key] = fv.iv;
    }
    return true;
  };
  const auto ensures_conjunct = [&](const Expr& conjunct) -> bool {
    if (conjunct.kind != Expr::Kind::BinOp || conjunct.bin_op != BinOp::Eq || !conjunct.lhs ||
        !conjunct.rhs) {
      return false;
    }
    const Expr* lhs = conjunct.lhs.get();
    const Expr* rhs = conjunct.rhs.get();
    if (lhs->kind == Expr::Kind::Ident && lhs->ident == "result") {
      return record(name, *rhs);
    }
    if (rhs->kind == Expr::Kind::Ident && rhs->ident == "result") {
      return record(name, *lhs);
    }
    // `result[i][j] == <expr>` — a tile element. The AST is nested
    // Index(Index(result, i), j), so canonicalize through array_index_const_key
    // and require the root ident to be `result`.
    const Expr* res = lhs;
    const Expr* body = rhs;
    if (res->kind != Expr::Kind::Index) {
      res = rhs;
      body = lhs;
    }
    if (res->kind != Expr::Kind::Index) {
      return false;
    }
    const auto key = array_index_const_key(*res);
    if (!key || key->rfind("result", 0) != 0) {
      return false;
    }
    // Replace the leading `result` with the assigned name.
    std::string elem_key = name + key->substr(std::string("result").size());
    return record(elem_key, *body);
  };
  bool any = false;
  for (const auto& rc : callee->contracts) {
    if (rc.kind != ContractKind::Ensures || !rc.expr) {
      continue;
    }
    if (rc.expr->kind == Expr::Kind::BinOp && rc.expr->bin_op == BinOp::And && rc.expr->lhs &&
        rc.expr->rhs) {
      // Walk the whole `and` chain as conjuncts.
      std::vector<const Expr*> conjuncts;
      std::vector<const Expr*> stack{rc.expr.get()};
      while (!stack.empty()) {
        const Expr* e = stack.back();
        stack.pop_back();
        if (e->kind == Expr::Kind::BinOp && e->bin_op == BinOp::And && e->lhs && e->rhs) {
          stack.push_back(e->rhs.get());
          stack.push_back(e->lhs.get());
        } else {
          conjuncts.push_back(e);
        }
      }
      for (const Expr* conj : conjuncts) {
        if (ensures_conjunct(*conj)) {
          any = true;
        }
      }
    } else if (ensures_conjunct(*rc.expr)) {
      any = true;
    }
  }
  return any;
}

// Record const facts from one assignment: int and float locals, `a[i] = c`
// array-element stores (int + float), `o.f = c` fields, and call-assignments
// whose callee `ensures result == <expr>` folds to a constant.
void note_assign_facts(const Expr& lhs, const Expr& rhs,
                       std::map<std::string, std::int64_t>& const_int_locals,
                       std::map<std::string, double>& const_float_locals,
                       const Module* module) {
  note_object_field_const_assign(lhs, rhs, const_int_locals);
  note_array_index_const_assign(lhs, rhs, const_int_locals);
  const auto arr_key = array_index_const_key(lhs);
  if (arr_key) {
    if (rhs.kind == Expr::Kind::FloatLit) {
      const_float_locals[*arr_key] = rhs.float_value;
    } else if (rhs.kind == Expr::Kind::Ident) {
      const auto ft = const_float_locals.find(rhs.ident);
      if (ft != const_float_locals.end()) {
        const_float_locals[*arr_key] = ft->second;
      }
    }
  }
  const auto fld_key = object_field_const_key(lhs);
  if (fld_key && rhs.kind == Expr::Kind::FloatLit) {
    const_float_locals[*fld_key] = rhs.float_value;
  }
  if (lhs.kind != Expr::Kind::Ident) {
    return;
  }
  const std::string& name = lhs.ident;
  if (rhs.kind == Expr::Kind::IntLit) {
    const_int_locals[name] = rhs.int_value;
    return;
  }
  if (rhs.kind == Expr::Kind::FloatLit) {
    const_float_locals[name] = rhs.float_value;
    return;
  }
  if (rhs.kind == Expr::Kind::Ident) {
    const auto it = const_int_locals.find(rhs.ident);
    if (it != const_int_locals.end()) {
      const_int_locals[name] = it->second;
    }
    const auto ft = const_float_locals.find(rhs.ident);
    if (ft != const_float_locals.end()) {
      const_float_locals[name] = ft->second;
    }
    return;
  }
  if (rhs.kind == Expr::Kind::Call && module != nullptr) {
    try_fold_call_to_const(
        name, rhs, const_int_locals, const_float_locals,
        [module](const std::string& n) { return find_proc_by_name(*module, n); });
  }
}

// Recognize `while i < N: ...; i = i + 1` loops whose counter starts at a
// known constant, so `a[i] = 1.0` can be unrolled into concrete element
// stores (`a[0] = 1.0`, ...). Conservative: no other writes to `i`, no
// break/continue/return, and at most 64 iterations.
bool simple_counter_loop(const Stmt& loop,
                         const std::map<std::string, std::int64_t>& const_int_locals,
                         std::string* idx, std::int64_t* start, std::int64_t* end) {
  if (loop.kind != Stmt::Kind::While || !loop.cond) {
    return false;
  }
  const Expr& cond = *loop.cond;
  if (cond.kind != Expr::Kind::BinOp || cond.bin_op != BinOp::Lt || !cond.lhs || !cond.rhs) {
    return false;
  }
  if (cond.lhs->kind != Expr::Kind::Ident || cond.rhs->kind != Expr::Kind::IntLit) {
    return false;
  }
  const std::string& i = cond.lhs->ident;
  const auto it = const_int_locals.find(i);
  if (it == const_int_locals.end()) {
    return false;
  }
  const std::int64_t n = cond.rhs->int_value;
  const std::int64_t s = it->second;
  if (n <= s || n - s > 64) {
    return false;
  }
  bool inc = false;
  for (const auto& st : loop.while_body) {
    if (st.kind == Stmt::Kind::Break || st.kind == Stmt::Kind::Continue ||
        st.kind == Stmt::Kind::Return) {
      return false;
    }
    if (st.kind == Stmt::Kind::Assign && st.init && st.init->kind == Expr::Kind::Ident &&
        st.init->ident == i) {
      if (st.expr && st.expr->kind == Expr::Kind::BinOp && st.expr->bin_op == BinOp::Add &&
          st.expr->lhs && st.expr->lhs->kind == Expr::Kind::Ident &&
          st.expr->lhs->ident == i && st.expr->rhs && st.expr->rhs->kind == Expr::Kind::IntLit &&
          st.expr->rhs->int_value == 1) {
        inc = true;
      } else {
        return false;  // some other write to the counter
      }
    }
  }
  if (!inc) {
    return false;
  }
  *idx = i;
  *start = s;
  *end = n;
  return true;
}

void collect_const_facts_in_stmts(const std::vector<Stmt>& stmts,
                                  std::map<std::string, std::int64_t>& const_int_locals,
                                  std::map<std::string, double>& const_float_locals,
                                  const Module* module) {
  for (const auto& s : stmts) {
    if (s.kind == Stmt::Kind::VarDecl && s.init) {
      if (s.init->kind == Expr::Kind::IntLit) {
        const_int_locals[s.var_name] = s.init->int_value;
      } else if (s.init->kind == Expr::Kind::FloatLit) {
        const_float_locals[s.var_name] = s.init->float_value;
      } else if (s.init->kind == Expr::Kind::Ident) {
        const auto it = const_int_locals.find(s.init->ident);
        if (it != const_int_locals.end()) {
          const_int_locals[s.var_name] = it->second;
        }
        const auto ft = const_float_locals.find(s.init->ident);
        if (ft != const_float_locals.end()) {
          const_float_locals[s.var_name] = ft->second;
        }
      } else if (s.init->kind == Expr::Kind::Call && module != nullptr) {
        try_fold_call_to_const(
            s.var_name, *s.init, const_int_locals, const_float_locals,
            [module](const std::string& n) { return find_proc_by_name(*module, n); });
      }
    }
    if (s.kind == Stmt::Kind::Assign && s.init && s.expr) {
      note_assign_facts(*s.init, *s.expr, const_int_locals, const_float_locals, module);
    }
    // Unroll simple counter loops so `a[i] = 1.0` records all element stores.
    std::string loop_i;
    std::int64_t loop_s = 0;
    std::int64_t loop_e = 0;
    if (s.kind == Stmt::Kind::While &&
        simple_counter_loop(s, const_int_locals, &loop_i, &loop_s, &loop_e)) {
      for (std::int64_t k = loop_s; k < loop_e; ++k) {
        for (const auto& body_st : s.while_body) {
          if (body_st.kind == Stmt::Kind::Assign && body_st.init && body_st.expr) {
            const auto lhs = subst_ident_lit(*body_st.init, loop_i, k);
            const auto rhs = subst_ident_lit(*body_st.expr, loop_i, k);
            note_assign_facts(*lhs, *rhs, const_int_locals, const_float_locals, module);
          }
        }
      }
    }
    collect_const_facts_in_stmts(s.then_body, const_int_locals, const_float_locals, module);
    if (s.else_body) {
      collect_const_facts_in_stmts(*s.else_body, const_int_locals, const_float_locals, module);
    }
    collect_const_facts_in_stmts(s.while_body, const_int_locals, const_float_locals, module);
    collect_const_facts_in_stmts(s.for_body, const_int_locals, const_float_locals, module);
    collect_const_facts_in_stmts(s.par_body, const_int_locals, const_float_locals, module);
  }
}

CallerProofFacts collect_caller_proof_facts(const ProcDecl& caller, const Module* module) {
  CallerProofFacts facts;
  collect_const_facts_in_stmts(caller.body, facts.const_int_locals, facts.const_float_locals,
                               module);
  for (const auto& s : caller.body) {
    if (s.kind == Stmt::Kind::If && s.cond) {
      note_nonneg_assumption_from_cond(*s.cond, facts.assum_nonneg_ints);
    }
  }
  return facts;
}

}  // namespace li
