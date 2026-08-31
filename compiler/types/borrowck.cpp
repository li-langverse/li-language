#include "li/borrowck.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>

namespace li {
namespace {

struct LocalState {
  bool moved = false;
  int mut_borrows = 0;
  int imm_borrows = 0;
};

struct BorrowCtx {
  std::map<std::string, LocalState> locals;
  std::map<std::string, std::string> borrow_source;
  std::map<std::string, bool> borrow_is_mut;
  const std::map<std::string, const ProcDecl*>& procs;
  DiagnosticBag& diags;
  std::string file;

  SourceLoc loc(const Span& s) const { return SourceLoc{file, 1, 1, s.start}; }

  void declare(const std::string& name) { locals[name] = LocalState{}; }

  void check_use(const std::string& name, const Span& span) {
    const auto it = locals.find(name);
    if (it == locals.end()) {
      return;
    }
    if (it->second.moved) {
      diags.error(loc(span), "use after move of `" + name + "`");
      return;
    }
    if (it->second.mut_borrows > 0) {
      diags.error(loc(span), "cannot use `" + name + "` while mut borrow is active");
    }
  }

  void check_expr_uses(const Expr& e) {
    switch (e.kind) {
      case Expr::Kind::Ident:
        check_use(e.ident, e.span);
        break;
      case Expr::Kind::BinOp:
        if (e.lhs) {
          check_expr_uses(*e.lhs);
        }
        if (e.rhs) {
          check_expr_uses(*e.rhs);
        }
        break;
      case Expr::Kind::Call:
        for (const auto& arg : e.args) {
          if (arg) {
            check_expr_uses(*arg);
          }
        }
        check_call_moves(e);
        break;
      case Expr::Kind::Index:
        if (e.base) {
          check_expr_uses(*e.base);
        }
        if (e.index) {
          check_expr_uses(*e.index);
        }
        break;
      case Expr::Kind::UnaryNot:
        if (e.operand) {
          check_expr_uses(*e.operand);
        }
        break;
      default:
        break;
    }
  }

  bool param_is_var(const TypeExpr& ty) { return ty.is_var; }

  // Scalar values (int/float/bool/ptr/int64/str/unit and refinements of them)
  // are copied when passed by value; only non-scalar heap values (arrays,
  // objects, lists, dicts) are moved, per Li's copy/move semantics.
  bool param_is_scalar(const TypeExpr& ty) {
    if (ty.kind == TypeKind::Refinement) {
      if (ty.refinement_base) {
        return param_is_scalar(*ty.refinement_base);
      }
      return true;
    }
    if (ty.kind != TypeKind::Named) {
      return false;
    }
    return ty.name == "int" || ty.name == "int64" || ty.name == "i64" ||
           ty.name == "long" || ty.name == "float" || ty.name == "float64" ||
           ty.name == "f64" || ty.name == "bool" || ty.name == "ptr" ||
           ty.name == "str" || ty.name == "unit";
  }

  void check_call_moves(const Expr& call) {
    const auto it = procs.find(call.ident);
    if (it == procs.end()) {
      return;
    }
    const ProcDecl& callee = *it->second;
    for (std::size_t n = 0; n < call.args.size() && n < callee.params.size(); ++n) {
      if (!call.args[n] || call.args[n]->kind != Expr::Kind::Ident) {
        continue;
      }
      if (param_is_var(callee.params[n].type)) {
        continue;
      }
      // Scalars are copied, not moved, on by-value call.
      if (param_is_scalar(callee.params[n].type)) {
        continue;
      }
      const std::string& name = call.args[n]->ident;
      auto lit = locals.find(name);
      if (lit != locals.end()) {
        lit->second.moved = true;
      }
    }
  }

  void check_borrow(const Stmt& s) {
    if (!s.init || s.init->kind != Expr::Kind::Ident) {
      declare(s.var_name);
      return;
    }
    const std::string& src = s.init->ident;
    check_use(src, s.init->span);
    auto& state = locals[src];
    if (s.borrow_mut) {
      if (state.mut_borrows > 0 || state.imm_borrows > 0) {
        diags.error(loc(s.span), "cannot borrow mut while existing borrow of `" + src + "` is active");
      }
      state.mut_borrows++;
    } else {
      if (state.mut_borrows > 0) {
        diags.error(loc(s.span), "cannot borrow imm while mut borrow of `" + src + "` is active");
      }
      state.imm_borrows++;
    }
    declare(s.var_name);
    borrow_source[s.var_name] = src;
    borrow_is_mut[s.var_name] = s.borrow_mut;
  }

  void release_borrows_in_block(const std::vector<Stmt>& body) {
    for (const auto& s : body) {
      if (s.kind == Stmt::Kind::Borrow) {
        const auto it = borrow_source.find(s.var_name);
        if (it != borrow_source.end()) {
          auto& state = locals[it->second];
          if (borrow_is_mut[s.var_name]) {
            state.mut_borrows--;
          } else {
            state.imm_borrows--;
          }
        }
      }
    }
  }

  void check_stmt(const Stmt& s) {
    if (s.kind == Stmt::Kind::VarDecl) {
      declare(s.var_name);
      if (s.init) {
        check_expr_uses(*s.init);
      }
      return;
    }
    if (s.kind == Stmt::Kind::Borrow) {
      check_borrow(s);
      return;
    }
    if (s.kind == Stmt::Kind::Return && s.expr) {
      check_expr_uses(*s.expr);
      return;
    }
    if (s.kind == Stmt::Kind::If) {
      if (s.cond) {
        check_expr_uses(*s.cond);
      }
      for (const auto& inner : s.then_body) {
        check_stmt(inner);
      }
      return;
    }
    if (s.expr) {
      check_expr_uses(*s.expr);
    }
  }

  void check_proc(const ProcDecl& p) {
    if (p.is_extern) {
      return;
    }
    locals.clear();
    borrow_source.clear();
    borrow_is_mut.clear();
    for (const auto& param : p.params) {
      declare(param.name);
    }
    for (const auto& s : p.body) {
      check_stmt(s);
    }
    release_borrows_in_block(p.body);
  }
};

bool type_mentions_heap(const TypeExpr& te) {
  if (te.kind == TypeKind::TypeApp &&
      (te.name == "list" || te.name == "dict" || te.name == "set")) {
    return true;
  }
  for (const auto& arg : te.type_args) {
    if (arg && type_mentions_heap(*arg)) {
      return true;
    }
  }
  if (te.elem && type_mentions_heap(*te.elem)) {
    return true;
  }
  return false;
}

bool expr_mentions_io(const Expr& e,
                       const std::map<std::string, const ProcDecl*>& proc_map) {
  if (e.kind == Expr::Kind::Call) {
    const bool builtin_output =
        (e.ident == "echo" || e.ident == "print") && proc_map.count(e.ident) == 0;
    if (builtin_output) {
      return true;
    }
    for (const auto& arg : e.args) {
      if (arg && expr_mentions_io(*arg, proc_map)) {
        return true;
      }
    }
    return false;
  }
  if (e.lhs && expr_mentions_io(*e.lhs, proc_map)) {
    return true;
  }
  if (e.rhs && expr_mentions_io(*e.rhs, proc_map)) {
    return true;
  }
  if (e.operand && expr_mentions_io(*e.operand, proc_map)) {
    return true;
  }
  if (e.base && expr_mentions_io(*e.base, proc_map)) {
    return true;
  }
  if (e.index && expr_mentions_io(*e.index, proc_map)) {
    return true;
  }
  return false;
}

bool stmt_mentions_io(const Stmt& s,
                      const std::map<std::string, const ProcDecl*>& proc_map) {
  if (s.expr && expr_mentions_io(*s.expr, proc_map)) {
    return true;
  }
  if (s.cond && expr_mentions_io(*s.cond, proc_map)) {
    return true;
  }
  if (s.init && expr_mentions_io(*s.init, proc_map)) {
    return true;
  }
  for (const auto& inner : s.then_body) {
    if (stmt_mentions_io(inner, proc_map)) {
      return true;
    }
  }
  if (s.else_body) {
    for (const auto& inner : *s.else_body) {
      if (stmt_mentions_io(inner, proc_map)) {
        return true;
      }
    }
  }
  for (const auto& inner : s.while_body) {
    if (stmt_mentions_io(inner, proc_map)) {
      return true;
    }
  }
  for (const auto& inner : s.for_body) {
    if (stmt_mentions_io(inner, proc_map)) {
      return true;
    }
  }
  for (const auto& inner : s.par_body) {
    if (stmt_mentions_io(inner, proc_map)) {
      return true;
    }
  }
  return false;
}

bool proc_mentions_io(const ProcDecl& p,
                      const std::map<std::string, const ProcDecl*>& proc_map) {
  for (const auto& s : p.body) {
    if (stmt_mentions_io(s, proc_map)) {
      return true;
    }
  }
  return false;
}

bool proc_mentions_heap(const ProcDecl& p) {
  for (const auto& param : p.params) {
    if (type_mentions_heap(param.type)) {
      return true;
    }
  }
  if (p.ret_type && type_mentions_heap(*p.ret_type)) {
    return true;
  }
  for (const auto& s : p.body) {
    if (s.kind == Stmt::Kind::VarDecl && type_mentions_heap(s.var_type)) {
      return true;
    }
  }
  return false;
}

bool has_effect(const std::vector<std::string>& raises, const std::string& name) {
  for (const auto& e : raises) {
    if (e == name) {
      return true;
    }
  }
  return false;
}

}  // namespace

void borrow_check_module(const Module& module, DiagnosticBag& diags,
                         std::size_t main_proc_count) {
  std::map<std::string, const ProcDecl*> proc_map;
  for (const auto& proc : module.procs) {
    proc_map[proc.name] = &proc;
  }
  BorrowCtx ctx{ {}, {}, {}, proc_map, diags, "module" };
  // Only the main module's own proc bodies are borrow-checked; imported procs
  // are in scope (proc_map) for call resolution but are lowered without a
  // borrow-check pass (see typecheck_module).
  const std::size_t check_count = std::min(main_proc_count, module.procs.size());
  for (std::size_t n = 0; n < check_count; ++n) {
    ctx.check_proc(module.procs[n]);
  }
}

bool proc_mentions_extern_call(const ProcDecl& p,
                               const std::map<std::string, const ProcDecl*>& proc_map) {
  for (const auto& s : p.body) {
    if (s.expr && s.expr->kind == Expr::Kind::Call &&
        s.expr->ident != "echo" &&
        (s.expr->ident != "print" || proc_map.find("print") != proc_map.end())) {
      const auto it = proc_map.find(s.expr->ident);
      if (it != proc_map.end() && it->second->is_extern) {
        return true;
      }
    }
  }
  return false;
}

void effects_check_module(const Module& module, DiagnosticBag& diags,
                          std::size_t main_proc_count) {
  std::map<std::string, const ProcDecl*> proc_map;
  for (const auto& proc : module.procs) {
    proc_map[proc.name] = &proc;
  }
  // Only the main module's own proc bodies are effects-checked; imported procs
  // are in scope (proc_map) but are lowered without an effects-check pass.
  const std::size_t check_count = std::min(main_proc_count, module.procs.size());
  for (std::size_t n = 0; n < check_count; ++n) {
    const ProcDecl& proc = module.procs[n];
    if (proc.is_extern) {
      continue;
    }
    const SourceLoc loc{ "module", 1, 1, proc.span.start };
    if (proc_mentions_io(proc, proc_map) && !has_effect(proc.raises, "IO")) {
      diags.error(loc, "proc calls echo or print but does not declare raises IO");
    }
    if (proc_mentions_extern_call(proc, proc_map) && !has_effect(proc.raises, "IO")) {
      diags.error(loc, "proc calls extern but does not declare raises IO");
    }
    if (proc_mentions_heap(proc) && !has_effect(proc.raises, "Alloc")) {
      diags.error(loc, "proc uses heap type but does not declare raises Alloc");
    }
  }
}

}  // namespace li
