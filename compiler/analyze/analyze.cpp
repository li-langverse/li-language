#include "li/analyze.hpp"

#include "li/error_codes.hpp"

#include <set>
#include <string>

namespace li {
namespace {

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

void collect_expr_idents(const Expr* expr, std::set<std::string>& idents) {
  if (expr == nullptr) {
    return;
  }
  if (expr->kind == Expr::Kind::Ident) {
    idents.insert(expr->ident);
  }
  collect_expr_idents(expr->lhs.get(), idents);
  collect_expr_idents(expr->rhs.get(), idents);
  collect_expr_idents(expr->operand.get(), idents);
  collect_expr_idents(expr->base.get(), idents);
  collect_expr_idents(expr->index.get(), idents);
  for (const auto& arg : expr->args) {
    collect_expr_idents(arg.get(), idents);
  }
}

void collect_stmt_idents(const Stmt& stmt, std::set<std::string>& idents);

void collect_block_idents(const std::vector<Stmt>& body, std::set<std::string>& idents) {
  for (const auto& stmt : body) {
    collect_stmt_idents(stmt, idents);
  }
}

void collect_stmt_idents(const Stmt& stmt, std::set<std::string>& idents) {
  collect_expr_idents(stmt.expr.get(), idents);
  collect_expr_idents(stmt.cond.get(), idents);
  collect_expr_idents(stmt.init.get(), idents);
  for (const auto& c : stmt.for_contracts) {
    collect_expr_idents(c.expr.get(), idents);
  }
  for (const auto& c : stmt.par_contracts) {
    collect_expr_idents(c.expr.get(), idents);
  }
  collect_block_idents(stmt.then_body, idents);
  if (stmt.else_body) {
    collect_block_idents(*stmt.else_body, idents);
  }
  collect_block_idents(stmt.while_body, idents);
  collect_block_idents(stmt.for_body, idents);
  collect_block_idents(stmt.par_body, idents);
}

bool block_terminates(const std::vector<Stmt>& body);

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

bool block_terminates(const std::vector<Stmt>& body) {
  if (body.empty()) {
    return false;
  }
  return stmt_terminates(body.back());
}

void warn_unreachable(const std::vector<Stmt>& body, const std::string& file,
                      DiagnosticBag& diags) {
  bool seen_terminator = false;
  for (const auto& stmt : body) {
    if (seen_terminator) {
      SourceLoc loc{file, 1, 1, stmt.span.start};
      diag_warning(diags, loc, WarningCode::W0402, "unreachable statement");
    }
    if (stmt.kind == Stmt::Kind::If) {
      warn_unreachable(stmt.then_body, file, diags);
      if (stmt.else_body) {
        warn_unreachable(*stmt.else_body, file, diags);
      }
    } else if (stmt.kind == Stmt::Kind::While) {
      warn_unreachable(stmt.while_body, file, diags);
    } else if (stmt.kind == Stmt::Kind::For) {
      warn_unreachable(stmt.for_body, file, diags);
    } else if (stmt.kind == Stmt::Kind::ParallelFor) {
      warn_unreachable(stmt.par_body, file, diags);
    }
    if (stmt_terminates(stmt)) {
      seen_terminator = true;
    }
  }
}

}  // namespace

void run_advisory_passes(const Module& module, const std::string& file,
                         DiagnosticBag& diags) {
  std::set<std::string> used_idents;
  for (const auto& proc : module.procs) {
    for (const auto& c : proc.contracts) {
      collect_expr_idents(c.expr.get(), used_idents);
    }
    collect_block_idents(proc.body, used_idents);
  }

  for (const auto& imp : module.imports) {
    const std::string binding = import_binding_name(imp);
    if (!binding.empty() && used_idents.count(binding) == 0) {
      SourceLoc loc{file, 1, 1, imp.span.start};
      diag_warning(diags, loc, WarningCode::W0401, "unused import `" + imp.module + "`");
    }
  }

  for (const auto& proc : module.procs) {
    warn_unreachable(proc.body, file, diags);
    bool has_requires = false;
    bool has_ensures = false;
    for (const auto& c : proc.contracts) {
      if (c.kind == ContractKind::Requires) {
        has_requires = true;
      } else if (c.kind == ContractKind::Ensures) {
        has_ensures = true;
      }
    }
    if (has_requires && !has_ensures) {
      SourceLoc loc{file, 1, 1, proc.span.start};
      diag_note(diags, loc, NoteCode::N0401,
                "procedure `" + proc.name + "` has requires but no ensures",
                "add `ensures` to document the postcondition");
    }
  }
}

}  // namespace li
