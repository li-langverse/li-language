#include "li/borrowck.hpp"

#include "li/error_codes.hpp"

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
      diag_error(diags, loc(span), ErrorCode::E0311,
                 "Variable `" + name + "` was moved and cannot be used again.",
                 "Use the new owner, or borrow before the move if you need shared access.");
      return;
    }
    if (it->second.mut_borrows > 0) {
      diag_error(diags, loc(span), ErrorCode::E0310,
                 "Cannot read `" + name + "` while a mutable borrow is still active.",
                 "End the `borrow mut` scope (or drop the binding) before using the value.");
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
      case Expr::Kind::MethodCall:
        if (e.base) {
          check_expr_uses(*e.base);
        }
        for (const auto& arg : e.args) {
          if (arg) {
            check_expr_uses(*arg);
          }
        }
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
      case Expr::Kind::UnaryMinus:
        if (e.operand) {
          check_expr_uses(*e.operand);
        }
        break;
      case Expr::Kind::Await:
        if (e.operand) {
          check_expr_uses(*e.operand);
        }
        break;
      default:
        break;
    }
  }

  bool param_is_var(const TypeExpr& ty) { return ty.is_var; }

  // Scalars (int/float/i64/bool/…) are copied by value in the ABI, so passing
  // one to a non-`var` param does not move the caller's binding. Only array and
  // object params carry ownership; `var` params are by-reference either way.
  bool scalar_param_type(const TypeExpr& ty) {
    if (ty.kind == TypeKind::Refinement) {
      return ty.refinement_base ? scalar_param_type(*ty.refinement_base) : false;
    }
    if (ty.kind == TypeKind::Named) {
      return ty.name == "int" || ty.name == "float" || ty.name == "i64" ||
             ty.name == "int64" || ty.name == "bool" || ty.name == "f32" ||
             ty.name == "f64" || ty.name == "u32" || ty.name == "u64" ||
             ty.name == "unit";
    }
    return false;
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
      if (param_is_var(callee.params[n].type) || scalar_param_type(callee.params[n].type)) {
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
        diag_error(diags, loc(s.span), ErrorCode::E0310,
                   "Cannot take `borrow mut` of `" + src +
                       "` while another borrow is still active.",
                   "Wait until existing `borrow` / `borrow mut` bindings go out of scope.");
      }
      state.mut_borrows++;
    } else {
      if (state.mut_borrows > 0) {
        diag_error(diags, loc(s.span), ErrorCode::E0310,
                   "Cannot take `borrow imm` of `" + src + "` while a mutable borrow is active.",
                   "Release the `borrow mut` binding first.");
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
  if (te.kind == TypeKind::Named &&
      (te.name == "str" || te.name == "bytes" || te.name == "stringview" ||
       te.name == "Bytes" || te.name == "StringView")) {
    return true;
  }
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

bool stmt_mentions_print(const Stmt& s) {
  if (s.expr) {
    if (s.expr->kind == Expr::Kind::Call && s.expr->ident == "print") {
      return true;
    }
    if (s.expr->kind == Expr::Kind::Ident && s.expr->ident == "print") {
      return true;
    }
  }
  if (s.kind == Stmt::Kind::If) {
    for (const auto& inner : s.then_body) {
      if (stmt_mentions_print(inner)) {
        return true;
      }
    }
  }
  return false;
}

bool proc_mentions_print(const ProcDecl& p) {
  for (const auto& s : p.body) {
    if (stmt_mentions_print(s)) {
      return true;
    }
  }
  return false;
}

bool proc_has_decorator(const ProcDecl& p, const std::string& name) {
  for (const auto& d : p.decorators) {
    if (d.name == name) {
      return true;
    }
  }
  return false;
}

bool proc_is_async(const ProcDecl& p) {
  return p.is_async || proc_has_decorator(p, "async");
}

bool expr_has_await(const Expr& e) {
  if (e.kind == Expr::Kind::Await) {
    return true;
  }
  switch (e.kind) {
    case Expr::Kind::BinOp:
      return (e.lhs && expr_has_await(*e.lhs)) || (e.rhs && expr_has_await(*e.rhs));
    case Expr::Kind::Call:
      for (const auto& arg : e.args) {
        if (arg && expr_has_await(*arg)) {
          return true;
        }
      }
      return false;
    case Expr::Kind::MethodCall:
      if (e.base && expr_has_await(*e.base)) {
        return true;
      }
      for (const auto& arg : e.args) {
        if (arg && expr_has_await(*arg)) {
          return true;
        }
      }
      return false;
    case Expr::Kind::UnaryNot:
      return e.operand && expr_has_await(*e.operand);
    case Expr::Kind::UnaryMinus:
      return e.operand && expr_has_await(*e.operand);
    case Expr::Kind::Index:
      return (e.base && expr_has_await(*e.base)) || (e.index && expr_has_await(*e.index));
    case Expr::Kind::FieldAccess:
      return e.base && expr_has_await(*e.base);
    default:
      return false;
  }
}

bool stmt_has_await(const Stmt& s) {
  if (s.expr && expr_has_await(*s.expr)) {
    return true;
  }
  if (s.init && expr_has_await(*s.init)) {
    return true;
  }
  if (s.cond && expr_has_await(*s.cond)) {
    return true;
  }
  for (const auto& child : s.then_body) {
    if (stmt_has_await(child)) {
      return true;
    }
  }
  if (s.else_body) {
    for (const auto& child : *s.else_body) {
      if (stmt_has_await(child)) {
        return true;
      }
    }
  }
  for (const auto& child : s.while_body) {
    if (stmt_has_await(child)) {
      return true;
    }
  }
  for (const auto& child : s.for_body) {
    if (stmt_has_await(child)) {
      return true;
    }
  }
  for (const auto& child : s.par_body) {
    if (stmt_has_await(child)) {
      return true;
    }
  }
  return false;
}

void collect_calls(const ProcDecl& p, std::vector<std::string>& out) {
  for (const auto& s : p.body) {
    if (s.expr && s.expr->kind == Expr::Kind::Call) {
      out.push_back(s.expr->ident);
    }
  }
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

void borrow_check_module(const Module& module, DiagnosticBag& diags) {
  std::map<std::string, const ProcDecl*> proc_map;
  for (const auto& proc : module.procs) {
    proc_map[proc.name] = &proc;
  }
  BorrowCtx ctx{ {}, {}, {}, proc_map, diags, "module" };
  for (const auto& proc : module.procs) {
    ctx.check_proc(proc);
  }
}

bool extern_call_skips_io_effect(const std::string& callee) {
  if (callee == "li_rt_volatile_sink_f64") {
    return true;
  }
  if (callee.rfind("li_reduce_sum_", 0) == 0) {
    return true;
  }
  return false;
}

bool proc_mentions_extern_call(const ProcDecl& p,
                               const std::map<std::string, const ProcDecl*>& proc_map) {
  for (const auto& s : p.body) {
    if (s.expr && s.expr->kind == Expr::Kind::Call && s.expr->ident != "print") {
      const auto it = proc_map.find(s.expr->ident);
      if (it != proc_map.end() && it->second->is_extern &&
          !extern_call_skips_io_effect(s.expr->ident)) {
        return true;
      }
    }
  }
  return false;
}

void effects_check_module(const Module& module, DiagnosticBag& diags) {
  std::map<std::string, const ProcDecl*> proc_map;
  for (const auto& proc : module.procs) {
    proc_map[proc.name] = &proc;
  }
  for (const auto& proc : module.procs) {
    if (proc.is_extern) {
      continue;
    }
    const SourceLoc loc{ "module", 1, 1, proc.span.start };
    if (proc_mentions_print(proc) && !has_effect(proc.raises, "IO")) {
      diags.error(loc, "proc calls print but does not declare raises IO");
    }
    if (proc_mentions_extern_call(proc, proc_map) && !has_effect(proc.raises, "IO")) {
      diags.error(loc, "proc calls extern but does not declare raises IO");
    }
    if (proc_mentions_heap(proc) && !has_effect(proc.raises, "Alloc")) {
      diags.error(loc, "proc uses heap type but does not declare raises Alloc");
    }
  }
  for (const auto& proc : module.procs) {
    if (proc.is_extern) {
      continue;
    }
    const SourceLoc loc{"module", 1, 1, proc.span.start};
    if (proc_is_async(proc) && !has_effect(proc.raises, "Async")) {
      diags.error(loc, "async proc does not declare raises Async");
    }
    for (const auto& s : proc.body) {
      if (stmt_has_await(s) && !proc_is_async(proc)) {
        diags.error(loc, "await is only allowed in async proc");
        break;
      }
    }
    std::vector<std::string> calls;
    collect_calls(proc, calls);
    for (const std::string& callee_name : calls) {
      const auto it = proc_map.find(callee_name);
      if (it == proc_map.end()) {
        continue;
      }
      const ProcDecl& callee = *it->second;
      if (has_effect(callee.raises, "Net") && !has_effect(proc.raises, "Net")) {
        diags.error(loc, "proc calls `" + callee_name +
                             "` which raises Net but caller does not declare raises Net");
      }
      if (has_effect(callee.raises, "Async") && !has_effect(proc.raises, "Async")) {
        diags.error(loc, "proc calls `" + callee_name +
                             "` which raises Async but caller does not declare raises Async");
      }
    }
  }
}

}  // namespace li
