#include "li/advisory.hpp"

#include "li/error_codes.hpp"

#include <set>
#include <string>

namespace li {
namespace {

SourceLoc loc_at(const std::string& file) { return SourceLoc{file, 1, 1, 0}; }

std::string import_binding_name(const ImportDecl& imp) {
  if (!imp.alias.empty()) {
    return imp.alias;
  }
  const std::size_t dot = imp.module.find('.');
  if (dot == std::string::npos) {
    return imp.module;
  }
  return imp.module.substr(0, dot);
}

void collect_expr_idents(const Expr* expr, std::set<std::string>& out) {
  if (expr == nullptr) {
    return;
  }
  if (expr->kind == Expr::Kind::Ident) {
    out.insert(expr->ident);
  }
  collect_expr_idents(expr->lhs.get(), out);
  collect_expr_idents(expr->rhs.get(), out);
  collect_expr_idents(expr->operand.get(), out);
  collect_expr_idents(expr->base.get(), out);
  collect_expr_idents(expr->index.get(), out);
  for (const auto& arg : expr->args) {
    collect_expr_idents(arg.get(), out);
  }
}

void collect_stmt_idents(const Stmt& stmt, std::set<std::string>& out);

void collect_block_idents(const std::vector<Stmt>& body, std::set<std::string>& out) {
  for (const auto& stmt : body) {
    collect_stmt_idents(stmt, out);
  }
}

void collect_stmt_idents(const Stmt& stmt, std::set<std::string>& out) {
  collect_expr_idents(stmt.expr.get(), out);
  collect_expr_idents(stmt.cond.get(), out);
  collect_expr_idents(stmt.init.get(), out);
  for (const auto& c : stmt.for_contracts) {
    collect_expr_idents(c.expr.get(), out);
  }
  for (const auto& c : stmt.par_contracts) {
    collect_expr_idents(c.expr.get(), out);
  }
  collect_block_idents(stmt.then_body, out);
  if (stmt.else_body) {
    collect_block_idents(*stmt.else_body, out);
  }
  collect_block_idents(stmt.while_body, out);
  collect_block_idents(stmt.for_body, out);
  collect_block_idents(stmt.par_body, out);
}

bool block_terminates(const std::vector<Stmt>& stmts);

bool stmt_terminates(const Stmt& stmt) {
  switch (stmt.kind) {
    case Stmt::Kind::Return:
    case Stmt::Kind::Break:
    case Stmt::Kind::Continue:
      return true;
    case Stmt::Kind::If:
      return stmt.else_body.has_value() && block_terminates(stmt.then_body) &&
             block_terminates(*stmt.else_body);
    default:
      return false;
  }
}

bool block_terminates(const std::vector<Stmt>& stmts) {
  if (stmts.empty()) {
    return false;
  }
  return stmt_terminates(stmts.back());
}

void walk_stmts_unreachable(const std::vector<Stmt>& stmts, const std::string& file,
                            const std::string& proc_name, DiagnosticBag& diags,
                            bool& seen_terminator) {
  for (const auto& stmt : stmts) {
    if (seen_terminator) {
      diag_warning(diags, loc_at(file), WarningCode::W0402,
                   "unreachable statement in '" + proc_name + "'",
                   "remove dead code or guard with a condition");
    }
    if (stmt.kind == Stmt::Kind::If) {
      bool then_term = false;
      bool else_term = false;
      walk_stmts_unreachable(stmt.then_body, file, proc_name, diags, then_term);
      if (stmt.else_body) {
        walk_stmts_unreachable(*stmt.else_body, file, proc_name, diags, else_term);
      }
    } else if (stmt.kind == Stmt::Kind::While) {
      bool inner_term = false;
      walk_stmts_unreachable(stmt.while_body, file, proc_name, diags, inner_term);
    } else if (stmt.kind == Stmt::Kind::For) {
      bool inner_term = false;
      walk_stmts_unreachable(stmt.for_body, file, proc_name, diags, inner_term);
    } else if (stmt.kind == Stmt::Kind::ParallelFor) {
      bool inner_term = false;
      walk_stmts_unreachable(stmt.par_body, file, proc_name, diags, inner_term);
    }
    if (stmt_terminates(stmt)) {
      seen_terminator = true;
    }
  }
}

void check_unused_imports(const Module& module, const std::string& file, DiagnosticBag& diags) {
  std::set<std::string> used;
  for (const auto& proc : module.procs) {
    for (const auto& c : proc.contracts) {
      collect_expr_idents(c.expr.get(), used);
    }
    collect_block_idents(proc.body, used);
  }
  for (const auto& imp : module.imports) {
    const std::string binding = import_binding_name(imp);
    if (!binding.empty() && used.count(binding) == 0) {
      diag_warning(diags, loc_at(file), WarningCode::W0401,
                   "unused import `" + imp.module + "`",
                   "remove the import or reference it explicitly");
    }
  }
}

void check_unreachable_after_return(const Module& module, const std::string& file,
                                    DiagnosticBag& diags) {
  for (const auto& proc : module.procs) {
    bool seen_terminator = false;
    walk_stmts_unreachable(proc.body, file, proc.name, diags, seen_terminator);
  }
}

void check_requires_without_ensures(const Module& module, const std::string& file,
                                    DiagnosticBag& diags) {
  for (const auto& proc : module.procs) {
    bool has_requires = false;
    bool has_ensures = false;
    for (const auto& contract : proc.contracts) {
      if (contract.kind == ContractKind::Requires) {
        has_requires = true;
      } else if (contract.kind == ContractKind::Ensures) {
        has_ensures = true;
      }
    }
    if (has_requires && !has_ensures) {
      diag_note(diags, loc_at(file), NoteCode::N0401,
                "procedure `" + proc.name + "` has requires but no ensures",
                "add `ensures` to capture the intended postcondition");
    }
  }
}

}  // namespace

void run_advisory_passes(const Module& module, const std::string& file_path,
                         const AdvisoryOptions& options, DiagnosticBag& diags) {
  (void)options;
  check_unused_imports(module, file_path, diags);
  check_unreachable_after_return(module, file_path, diags);
  check_requires_without_ensures(module, file_path, diags);
}

}  // namespace li
