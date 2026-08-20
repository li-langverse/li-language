#include "li/vc_emit.hpp"

#include "li/ast.hpp"
#include "li/call_requires.hpp"
#include "li/numeric_types.hpp"
#include "li/vc_prove.hpp"
#include "li/vc_summary.hpp"
#include "li/vc_witness.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>

namespace li {
namespace {

std::optional<std::int64_t> int_lit_value(const Expr& e) {
  if (e.kind == Expr::Kind::IntLit) return e.int_value;
  return std::nullopt;
}

bool lean_reserved_ident(const std::string& name) {
  static const char* const kReserved[] = {
      "by",  "have", "let", "in",   "fun",  "if",   "then", "else", "match", "with",
      "where", "mut", "do", "for", "while", "return", "structure", "class", "instance",
  };
  for (const char* r : kReserved) {
    if (name == r) {
      return true;
    }
  }
  return false;
}

std::string lean_ident(const std::string& name) {
  if (name == "result") {
    return "result";
  }
  if (name == "true") {
    return "True";
  }
  if (name == "false") {
    return "False";
  }
  if (lean_reserved_ident(name)) {
    return name + "_";
  }
  return name;
}

const TypeAlias* find_type_alias(const Module& module, const std::string& name) {
  for (const auto& alias : module.types) {
    if (alias.name == name) {
      return &alias;
    }
  }
  return nullptr;
}

const Expr* object_receiver_root(const Expr& e) {
  const Expr* cur = &e;
  while (cur && cur->kind == Expr::Kind::FieldAccess) {
    cur = cur->base.get();
  }
  if (cur && cur->kind == Expr::Kind::Ident) {
    return cur;
  }
  return nullptr;
}

void collect_local_types_in_stmts(const std::vector<Stmt>& stmts,
                                  std::map<std::string, const TypeExpr*>& out) {
  for (const auto& s : stmts) {
    if (s.kind == Stmt::Kind::VarDecl) {
      out[s.var_name] = &s.var_type;
    }
    collect_local_types_in_stmts(s.then_body, out);
    if (s.else_body) {
      collect_local_types_in_stmts(*s.else_body, out);
    }
    collect_local_types_in_stmts(s.while_body, out);
    collect_local_types_in_stmts(s.for_body, out);
    collect_local_types_in_stmts(s.par_body, out);
  }
}

std::string object_type_name_from_method_receiver(const ProcDecl& caller, const Expr& recv) {
  const Expr* root = object_receiver_root(recv);
  if (!root) {
    return {};
  }
  std::map<std::string, const TypeExpr*> locals;
  for (const auto& p : caller.params) {
    locals[p.name] = &p.type;
  }
  collect_local_types_in_stmts(caller.body, locals);
  const auto it = locals.find(root->ident);
  if (it == locals.end() || !it->second) {
    return {};
  }
  const TypeExpr* te = it->second;
  while (te && te->kind == TypeKind::Refinement && te->refinement_base) {
    te = te->refinement_base.get();
  }
  if (te && te->kind == TypeKind::Named) {
    return te->name;
  }
  return {};
}

std::string lean_type_name(const TypeExpr& ty, const Module& module);

std::string lean_type_name(const TypeExpr& ty, const Module& module) {
  switch (ty.kind) {
    case TypeKind::Named:
      if (ty.name == "int") {
        return "Int";
      }
      if (ty.name == "float") {
        return "Float";
      }
      if (const auto scalar = lookup_numeric_scalar(ty.name)) {
        if (scalar->kind == NumericScalarKind::Float) {
          return "Float";
        }
        if (scalar->kind == NumericScalarKind::IntSigned ||
            scalar->kind == NumericScalarKind::IntUnsigned) {
          return "Int";
        }
      }
      if (ty.name == "bool") {
        return "Bool";
      }
      if (ty.name == "unit") {
        return "Unit";
      }
      if (const TypeAlias* alias = find_type_alias(module, ty.name)) {
        return lean_type_name(alias->definition, module);
      }
      return "Int";
    case TypeKind::Array:
      if (ty.elem) {
        const std::string elem = lean_type_name(*ty.elem, module);
        const bool wrap_elem = ty.elem->kind == TypeKind::Array;
        return "LiArray " + (wrap_elem ? "(" + elem + ")" : elem) + " " +
               std::to_string(ty.array_size);
      }
      return "LiArray Unit 0";
    case TypeKind::Refinement:
      if (ty.refinement_base) {
        return lean_type_name(*ty.refinement_base, module);
      }
      return "Int";
    case TypeKind::TypeApp:
    case TypeKind::Callable:
    case TypeKind::GenericParam:
    case TypeKind::NamedTuple:
      return "Unit";
  }
  return "Unit";
}

struct VcCtx {
  const ProcDecl* proc = nullptr;
  bool in_ensures = false;
};

std::optional<std::string> expr_to_lean(const Expr& e, const VcCtx& ctx);

std::optional<std::string> expr_to_lean_bin(BinOp op, const std::string& lhs,
                                            const std::string& rhs) {
  switch (op) {
    case BinOp::Add:
      return "(" + lhs + " + " + rhs + ")";
    case BinOp::Sub:
      return "(" + lhs + " - " + rhs + ")";
    case BinOp::Mul:
      return "(" + lhs + " * " + rhs + ")";
    case BinOp::Div:
      return "(" + lhs + " / " + rhs + ")";
    case BinOp::Mod:
      return "(" + lhs + " % " + rhs + ")";
    case BinOp::Le:
      return "(" + lhs + " ≤ " + rhs + ")";
    case BinOp::Lt:
      return "(" + lhs + " < " + rhs + ")";
    case BinOp::Ge:
      return "(" + lhs + " ≥ " + rhs + ")";
    case BinOp::Gt:
      return "(" + lhs + " > " + rhs + ")";
    case BinOp::Eq:
      return "(" + lhs + " = " + rhs + ")";
    case BinOp::Ne:
      return "(" + lhs + " ≠ " + rhs + ")";
    case BinOp::And:
      return "(" + lhs + " ∧ " + rhs + ")";
    case BinOp::Or:
      return "(" + lhs + " ∨ " + rhs + ")";
    default:
      return std::nullopt;
  }
}

std::optional<std::string> expr_to_lean(const Expr& e, const VcCtx& ctx) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      return std::to_string(e.int_value);
    case Expr::Kind::FloatLit: {
      std::ostringstream os;
      os << e.float_value;
      return "(" + os.str() + " : Float)";
    }
    case Expr::Kind::Ident:
      return lean_ident(e.ident);
    case Expr::Kind::BinOp: {
      if (!e.lhs || !e.rhs) {
        return std::nullopt;
      }
      // For `-> unit` procs the only result is `()`, so `result == <lit>` must
      // emit a Unit literal rather than the int literal (Lean would fail to
      // synthesize `OfNat Unit 0`). Unit is a subsingleton, so this stays a
      // valid (open) obligation instead of a typeclass error.
      if ((e.bin_op == BinOp::Eq || e.bin_op == BinOp::Ne) && ctx.proc &&
          ctx.proc->ret_type && ctx.proc->ret_type->name == "unit" && ctx.in_ensures) {
        if (e.lhs->kind == Expr::Kind::Ident && e.lhs->ident == "result") {
          return "(result = ())";
        }
        if (e.rhs->kind == Expr::Kind::Ident && e.rhs->ident == "result") {
          return "(() = result)";
        }
      }
      const auto l = expr_to_lean(*e.lhs, ctx);
      const auto r = expr_to_lean(*e.rhs, ctx);
      if (!l || !r) {
        return std::nullopt;
      }
      return expr_to_lean_bin(e.bin_op, *l, *r);
    }
    case Expr::Kind::Call: {
      if (e.ident == "abs" && e.args.size() == 1 && e.args[0]) {
        const auto inner = expr_to_lean(*e.args[0], ctx);
        if (inner) {
          return "Float.abs " + *inner;
        }
      }
      return std::nullopt;
    }
    case Expr::Kind::Index: {
      if (!e.base || !e.index) {
        return std::nullopt;
      }
      const auto b = expr_to_lean(*e.base, ctx);
      const auto idx = expr_to_lean(*e.index, ctx);
      if (!b || !idx) {
        return std::nullopt;
      }
      return "(" + *b + "[" + *idx + "]!)";
    }
    case Expr::Kind::UnaryNot:
      if (e.operand) {
        const auto inner = expr_to_lean(*e.operand, ctx);
        if (inner) {
          return "(¬" + *inner + ")";
        }
      }
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

std::string proc_section(const std::string& proc) {
  std::string out;
  for (char c : proc) {
    if (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      out.push_back(c);
    } else {
      out.push_back('_');
    }
  }
  return out;
}

namespace {

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

}  // namespace

void emit_contract_def(std::ostream& out, const Module& module, const ProcDecl& proc,
                       const char* kind, std::size_t idx, const Contract& c,
                       const std::string& vc_suffix = "",
                       const std::string* loop_iter = nullptr,
                       std::size_t* native_closed = nullptr) {
  const std::string sec = proc_section(proc.name) + vc_suffix;
  const std::string name = "vc_" + sec + '_' + kind + '_' + std::to_string(idx);
  const auto emit_formals = [&](bool include_result) {
    for (const auto& p : proc.params) {
      out << ' ' << '(' << lean_ident(p.name) << " : " << lean_type_name(p.type, module) << ')';
    }
    if (loop_iter != nullptr) {
      out << " (" << lean_ident(*loop_iter) << " : Int)";
    }
    if (include_result && proc.ret_type &&
        (c.kind == ContractKind::Ensures || c.kind == ContractKind::Invariant)) {
      out << " (result : " << lean_type_name(*proc.ret_type, module) << ')';
    }
  };
  const auto emit_args = [&](bool include_result) {
    for (const auto& p : proc.params) {
      out << ' ' << lean_ident(p.name);
    }
    if (loop_iter != nullptr) {
      out << ' ' << lean_ident(*loop_iter);
    }
    if (include_result && proc.ret_type &&
        (c.kind == ContractKind::Ensures || c.kind == ContractKind::Invariant)) {
      out << " result";
    }
  };

  if (c.kind == ContractKind::Decreases && c.expr) {
    VcCtx dec_ctx;
    dec_ctx.proc = &proc;
    std::string measure = "0";
    if (c.expr->kind == Expr::Kind::IntLit) {
      measure = std::to_string(c.expr->int_value);
    } else if (c.expr->kind == Expr::Kind::Ident) {
      measure = "Int.toNat " + lean_ident(c.expr->ident);
    } else if (auto lean = expr_to_lean(*c.expr, dec_ctx)) {
      measure = "Int.toNat (" + *lean + ")";
    }
    out << "def " << name;
    emit_formals(false);
    out << " : Nat := " << measure << '\n';
    out << "theorem " << name << "_proved";
    emit_formals(false);
    out << " : " << name;
    emit_args(false);
    out << " = " << measure << " := rfl\n";
    return;
  }

  VcCtx ctx;
  ctx.proc = &proc;
  ctx.in_ensures = (c.kind == ContractKind::Ensures);

  std::string prop = "True";
  const CallerProofFacts caller_facts = collect_caller_proof_facts(proc, &module);
  bool mat2_discharge_theorem = false;
  bool sqrt_discharge_theorem = false;
  if (c.kind == ContractKind::Ensures && c.expr) {
    if (ctx.proc != nullptr && witness_mat2_int_at2_spec(*ctx.proc, *c.expr)) {
      mat2_discharge_theorem = true;
    } else if (ctx.proc != nullptr && witness_sqrt_open_bound_spec(*ctx.proc, *c.expr)) {
      sqrt_discharge_theorem = true;
    }
  }
  const bool witnessed =
      contract_witnessed_trivial(proc, c, &module, &caller_facts);
  bool natively_closed = false;
  if (!witnessed && c.expr &&
      discharge_prop_natively(*c.expr) == NativeDischarge::Proved) {
    natively_closed = true;
    if (native_closed != nullptr) {
      ++*native_closed;
    }
  }
  if ((witnessed || natively_closed) && !mat2_discharge_theorem && !sqrt_discharge_theorem) {
    prop = "True";
  } else if (mat2_discharge_theorem && c.kind == ContractKind::Ensures) {
    prop = "Li.Discharge.mat2_at2_float_spec";
    for (const auto& p : proc.params) {
      prop += ' ';
      prop += p.name;
    }
    prop += " (Li.Discharge.mat2_at2_eval";
    for (const auto& p : proc.params) {
      prop += ' ';
      prop += p.name;
    }
    prop += ')';
  } else if (c.expr) {
    if (auto lean = expr_to_lean(*c.expr, ctx)) {
      prop = *lean;
    } else {
      out << "/-! VC " << kind << " (opaque): source expr not yet translated -/\n";
    }
  }

  const bool semantic_ensures =
      (mat2_discharge_theorem || sqrt_discharge_theorem) && c.kind == ContractKind::Ensures;
  out << "def " << name;
  // The sqrt abs lemma's prop references `result` directly, so its def binds
  // the return value even though it is discharged semantically (mat2 embeds
  // the result inside the spec call instead and stays param-only).
  emit_formals(!semantic_ensures || sqrt_discharge_theorem);
  out << " : Prop := " << prop << '\n';

  if (prop == "True" && witnessed && c.kind == ContractKind::Ensures) {
    const Expr* ret = single_return_expr(proc);
    if (ret != nullptr && ret->kind == Expr::Kind::Ident && c.expr &&
        !ensures_rhs_eq_result(*c.expr)) {
      out << "/-! Phase 2f: requires/return witness for ensures (param `"
          << ret->ident << "`) -/\n";
    } else if (ret != nullptr && c.expr) {
      const Expr* rhs = ensures_rhs_eq_result(*c.expr);
      if (rhs != nullptr && witness_dot4_prelude_call(*ret, *rhs)) {
        out << "/-! Phase 2f: prelude dot() return witness -/\n";
      } else if (rhs != nullptr && witness_dot4_matmul(*ret, *rhs)) {
        out << "/-! Phase 2f: array `@` dot return witness -/\n";
      } else if (ctx.proc != nullptr &&
                 witness_dot4_int_loop(*ctx.proc, *rhs)) {
        out << "/-! Phase 2f: fixed-bound dot loop witness (4 iterations) -/\n";
      } else {
        out << "/-! Phase 2f: return expression matches ensures (static witness) -/\n";
      }
    } else {
      out << "/-! Phase 2f: return expression matches ensures (static witness) -/\n";
    }
  }
  if (mat2_discharge_theorem && c.kind == ContractKind::Ensures) {
    out << "theorem " << name << "_proved";
    emit_formals(false);
    out << " : " << name;
    emit_args(false);
    out << " := Li.Discharge.mat2_at2_float_spec_proved";
    for (const auto& p : proc.params) {
      out << ' ' << lean_ident(p.name);
    }
    out << '\n';
  } else if (sqrt_discharge_theorem && c.kind == ContractKind::Ensures) {
    // Intentional open VC: the abs float lemma needs Float lemmas that have
    // not landed, so the ensures def stays an unproved obligation. Emitting a
    // valid def (result bound above) keeps AutoVC buildable while
    // check-autovc-open-goals.sh reports it as still open.
    out << "/-! sqrt abs VC intentionally open (Float lemmas pending); def is valid Lean -/\n";
  } else if (prop == "True") {
    out << "theorem " << name << "_proved";
    emit_formals(true);
    out << " : " << name;
    emit_args(true);
    out << " := trivial\n";
  }
}

bool is_caller_param(const ProcDecl& caller, const std::string& name) {
  return std::any_of(caller.params.begin(), caller.params.end(),
                     [&](const Param& p) { return p.name == name; });
}

void append_call_site_vc_formals(std::ostream& out, const Module& module, const ProcDecl& caller,
                                 const std::set<std::string>& ref_idents) {
  std::map<std::string, const TypeExpr*> locals;
  for (const auto& p : caller.params) {
    locals[p.name] = &p.type;
    out << ' ' << '(' << lean_ident(p.name) << " : " << lean_type_name(p.type, module) << ')';
  }
  collect_local_types_in_stmts(caller.body, locals);
  for (const std::string& name : ref_idents) {
    if (is_caller_param(caller, name)) {
      continue;
    }
    const auto it = locals.find(name);
    if (it == locals.end() || it->second == nullptr) {
      continue;
    }
    out << " (" << lean_ident(name) << " : " << lean_type_name(*it->second, module) << ')';
  }
}

void append_call_site_vc_args(std::ostream& out, const ProcDecl& caller,
                              const std::set<std::string>& ref_idents) {
  for (const auto& p : caller.params) {
    out << ' ' << lean_ident(p.name);
  }
  std::set<std::string> emitted;
  for (const auto& p : caller.params) {
    emitted.insert(p.name);
  }
  for (const std::string& name : ref_idents) {
    if (!emitted.insert(name).second) {
      continue;
    }
    out << ' ' << lean_ident(name);
  }
}

void emit_requires_vcs_for_call(std::ostream& out, const Module& module, const ProcDecl& caller,
                                const ProcDecl& callee, std::size_t call_idx,
                                const std::vector<std::unique_ptr<Expr>>& args,
                                const ProofFacts& facts, std::size_t* native_closed = nullptr) {
  std::vector<std::string> param_names;
  for (const auto& p : callee.params) {
    param_names.push_back(p.name);
  }
  std::size_t req_idx = 0;
  for (const auto& rc : callee.contracts) {
    if (rc.kind != ContractKind::Requires || !rc.expr) {
      continue;
    }
    const std::string name = "vc_" + proc_section(caller.name) + "_call" +
                             std::to_string(call_idx) + '_' + proc_section(callee.name) +
                             "_requires_" + std::to_string(req_idx++);
    const auto sub = substitute_call_params(*rc.expr, param_names, args);
    const auto folded = fold_facts_expr(*sub, facts);
    std::string prop = "True";
    const bool witnessed =
        expr_statically_true(*folded) || folded_discharged_by_proof_facts(*folded, facts);
    bool natively_closed = false;
    if (!witnessed && discharge_prop_natively(*folded) == NativeDischarge::Proved) {
      natively_closed = true;
      if (native_closed != nullptr) {
        ++*native_closed;
      }
    }
    VcCtx ctx;
    if (witnessed || natively_closed) {
      prop = "True";
    } else if (auto lean = expr_to_lean(*folded, ctx)) {
      prop = *lean;
    } else if (auto lean = expr_to_lean(*sub, ctx)) {
      prop = *lean;
    } else {
      out << "/-! VC call-site requires (opaque): callee '" << callee.name << "' at call "
          << call_idx << " -/\n";
    }
    std::set<std::string> ref_idents;
    if (!witnessed && !natively_closed) {
      collect_idents_in_expr(*folded, ref_idents);
    }
    out << "def " << name;
    append_call_site_vc_formals(out, module, caller, ref_idents);
    out << " : Prop := " << prop << '\n';
    if (witnessed || natively_closed) {
      out << "theorem " << name << "_proved";
      append_call_site_vc_formals(out, module, caller, ref_idents);
      out << " : " << name;
      append_call_site_vc_args(out, caller, ref_idents);
      out << " := trivial\n";
    }
  }
}

void emit_call_site_requires(std::ostream& out, const Module& module, const ProcDecl& caller,
                             std::size_t* native_closed = nullptr) {
  const CallerProofFacts caller_facts = collect_caller_proof_facts(caller, &module);
  const ProofFacts facts = caller_facts.view();
  std::vector<const Expr*> calls;
  collect_calls_in_stmts(caller.body, calls);
  std::size_t call_idx = 0;
  for (const Expr* call : calls) {
    if (call == nullptr || call->kind != Expr::Kind::Call) {
      continue;
    }
    const ProcDecl* callee = find_proc_by_name(module, call->ident);
    if (callee == nullptr) {
      continue;
    }
    std::vector<std::string> param_names;
    for (const auto& p : callee->params) {
      param_names.push_back(p.name);
    }
    AliasTypeLookup alias_lookup = [&module](const std::string& name) -> const TypeExpr* {
      const TypeAlias* alias = find_type_alias(module, name);
      if (alias == nullptr) {
        return nullptr;
      }
      return &alias->definition;
    };
    std::size_t ref_idx = 0;
    for (std::size_t p = 0; p < callee->params.size() && p < call->args.size(); ++p) {
      const auto refinement = resolve_refinement_on_type(callee->params[p].type, alias_lookup);
      if (!refinement || call->args[p] == nullptr || !refinement->predicate) {
        continue;
      }
      const std::string name = "vc_" + proc_section(caller.name) + "_call" +
                               std::to_string(call_idx) + '_' + proc_section(callee->name) +
                               "_refine_" + std::to_string(ref_idx++);
      const auto sub = substitute_refinement_binding(*refinement->predicate, refinement->bind_var,
                                                     *call->args[p]);
      const bool witnessed =
          check_refinement_argument(*refinement, *call->args[p], facts) ==
              RequiresCheckResult::Satisfied;
      const auto folded = fold_facts_expr(*sub, facts);
      out << "/-! P-refine folded: " << expr_to_user_string(*folded) << " -/\n";
      bool natively_closed = false;
      if (!witnessed && discharge_prop_natively(*folded) == NativeDischarge::Proved) {
        natively_closed = true;
        if (native_closed != nullptr) {
          ++*native_closed;
        }
      }
      const bool closed = witnessed || natively_closed;
      VcCtx ctx;
      std::string prop = "True";
      bool refinement_discharge = false;
      std::optional<std::int64_t> lit_nonneg;
      if (folded->kind == Expr::Kind::BinOp && folded->bin_op == BinOp::Ge && folded->lhs &&
          folded->rhs) {
        if (const auto n = int_lit_value(*folded->lhs)) {
          if (const auto z = int_lit_value(*folded->rhs); z && *z == 0) {
            lit_nonneg = n;
          }
        } else if (const auto n = int_lit_value(*folded->rhs)) {
          if (const auto z = int_lit_value(*folded->lhs); z && *z == 0) {
            lit_nonneg = n;
          }
        }
      }
      if (closed) {
        // The obligation is discharged: emit a literal True goal (the old code
        // left the unfolded predicate, so `:= trivial` was ill-typed in Lean).
        if (lit_nonneg && witnessed) {
          prop = "Li.Discharge.refinement_nonneg_spec " + std::to_string(*lit_nonneg);
          refinement_discharge = true;
        } else {
          prop = "True";
        }
      } else if (auto lean = expr_to_lean(*folded, ctx)) {
        prop = *lean;
      } else if (folded->kind == Expr::Kind::BinOp && folded->lhs && folded->rhs) {
        const auto li = int_lit_value(*folded->lhs);
        const auto ri = int_lit_value(*folded->rhs);
        if (li && ri) {
          if (auto lean =
                  expr_to_lean_bin(folded->bin_op, std::to_string(*li), std::to_string(*ri))) {
            prop = *lean;
          }
        }
      } else if (!witnessed) {
        out << "/-! VC call-site refinement: param " << p << " of '" << callee->name
            << "' at call " << call_idx << " -/\n";
      }
      std::set<std::string> ref_idents;
      if (!closed) {
        collect_idents_in_expr(*sub, ref_idents);
      }
      out << "def " << name;
      append_call_site_vc_formals(out, module, caller, ref_idents);
      out << " : Prop := " << prop << '\n';
      if (closed) {
        out << "theorem " << name << "_proved";
        append_call_site_vc_formals(out, module, caller, ref_idents);
        out << " : " << name;
        append_call_site_vc_args(out, caller, ref_idents);
        if (refinement_discharge && lit_nonneg) {
          out << " := Li.Discharge.refinement_nonneg_lit_proved " << *lit_nonneg
              << " (by decide)\n";
        } else {
          out << " := trivial\n";
        }
      }
    }
    emit_requires_vcs_for_call(out, module, caller, *callee, call_idx, call->args, facts,
                               native_closed);
    ++call_idx;
  }
  std::vector<const Expr*> methods;
  collect_method_calls_in_stmts(caller.body, methods);
  for (const Expr* mc : methods) {
    if (mc == nullptr || !mc->base) {
      continue;
    }
    const std::string type_name = object_type_name_from_method_receiver(caller, *mc->base);
    if (type_name.empty()) {
      continue;
    }
    const std::string callee_name = type_name + "_" + mc->field_name;
    const ProcDecl* callee = find_proc_by_name(module, callee_name);
    if (callee == nullptr) {
      continue;
    }
    const auto args = method_call_arg_list(*mc->base, mc->args);
    emit_requires_vcs_for_call(out, module, caller, *callee, call_idx, args, facts,
                               native_closed);
    ++call_idx;
  }
}

void walk_contracts(std::ostream& out, const Module& module, const ProcDecl& proc,
                    const std::vector<Contract>& contracts,
                    const std::string& vc_suffix = "",
                    const std::string* loop_iter = nullptr,
                    std::size_t* native_closed = nullptr) {
  std::size_t req = 0;
  std::size_t ens = 0;
  std::size_t dec = 0;
  std::size_t inv = 0;
  for (const auto& c : contracts) {
    switch (c.kind) {
      case ContractKind::Requires:
        emit_contract_def(out, module, proc, "requires", req++, c, vc_suffix, loop_iter,
                          native_closed);
        break;
      case ContractKind::Ensures:
        emit_contract_def(out, module, proc, "ensures", ens++, c, vc_suffix, loop_iter,
                          native_closed);
        break;
      case ContractKind::Decreases:
        emit_contract_def(out, module, proc, "decreases", dec++, c, vc_suffix, loop_iter,
                          native_closed);
        break;
      case ContractKind::Invariant:
        emit_contract_def(out, module, proc, "invariant", inv++, c, vc_suffix, loop_iter,
                          native_closed);
        break;
      case ContractKind::ProbEnsures:
        emit_contract_def(out, module, proc, "prob_ensures", ens++, c, vc_suffix, loop_iter,
                          native_closed);
        break;
    }
  }
}

}  // namespace

bool write_vcs_lean(const Module& module, const std::string& path, std::string* err,
                    std::size_t* native_closed) {
  std::ofstream out(path);
  if (!out) {
    if (err) {
      *err = "cannot open " + path;
    }
    return false;
  }
  out << "-- Auto-generated VC obligations (Phase 2e). Props typecheck in Lean; discharge in 2f.\n";
  out << "import Init.Data.Float\nimport Core\nimport Discharge\n\nopen Li\n\nnamespace AutoVC\n\n";
  for (const auto& proc : module.procs) {
    if (proc.is_extern) {
      continue;
    }
    out << "namespace " << proc_section(proc.name) << "\n\n";
    walk_contracts(out, module, proc, proc.contracts, "", nullptr, native_closed);
    std::size_t par_idx = 0;
    for (const auto& stmt : proc.body) {
      if (stmt.kind == Stmt::Kind::ParallelFor && !stmt.par_contracts.empty()) {
        const std::string suffix = "_par" + std::to_string(par_idx++);
        walk_contracts(out, module, proc, stmt.par_contracts, suffix,
                       stmt.par_iter.empty() ? nullptr : &stmt.par_iter, native_closed);
      }
    }
    emit_call_site_requires(out, module, proc, native_closed);
    out << "\nend " << proc_section(proc.name) << "\n\n";
  }
  out << "end AutoVC\n";
  return true;
}

}  // namespace li
