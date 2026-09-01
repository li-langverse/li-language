#include "li/ast.hpp"

#include <sstream>

namespace li {
namespace {

std::string pad(int n) { return std::string(static_cast<std::size_t>(n), ' '); }

}  // namespace

std::string debug_expr(const Expr& e, int indent) {
  std::ostringstream os;
  const auto p = pad(indent);
  switch (e.kind) {
    case Expr::Kind::IntLit:
      os << p << "IntLit(" << e.int_value << ")\n";
      break;
    case Expr::Kind::Ident:
      os << p << "Ident(" << e.ident << ")\n";
      break;
    case Expr::Kind::UnaryNot:
      os << p << "Not\n";
      os << debug_expr(*e.operand, indent + 2);
      break;
    case Expr::Kind::BinOp:
      os << p << "BinOp\n";
      os << debug_expr(*e.lhs, indent + 2);
      os << debug_expr(*e.rhs, indent + 2);
      break;
    case Expr::Kind::Call:
      os << p << "Call(" << e.ident << ")\n";
      for (const auto& a : e.args) {
        os << debug_expr(*a, indent + 2);
      }
      break;
  }
  return os.str();
}

std::string debug_module(const Module& m) {
  std::ostringstream os;
  for (const auto& proc : m.procs) {
    os << "proc " << proc.name << '\n';
    for (const auto& stmt : proc.body) {
      if (stmt.kind == Stmt::Kind::Return && stmt.expr) {
        os << "  return\n" << debug_expr(*stmt.expr, 4);
      } else if (stmt.kind == Stmt::Kind::If) {
        os << "  if\n";
        if (stmt.cond) {
          os << debug_expr(*stmt.cond, 4);
        }
      }
    }
  }
  return os.str();
}

}  // namespace li
