#include "li/ast_dump.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <vector>

namespace li {
namespace {

struct Dumper {
  std::string_view src;
  std::ostringstream out;

  explicit Dumper(std::string_view s) : src(s) {}

  void sep() { out << ' '; }
  void i64(std::int64_t v) { out << v; }
  void text(const std::string& s) { out << s; }
  void lexeme(const Span& sp) {
    if (sp.end > sp.start && sp.end <= src.size()) {
      out.write(src.data() + static_cast<std::ptrdiff_t>(sp.start),
                sp.end - sp.start);
    }
  }
  void nl() { out << '\n'; }

  void line(int code) {
    out << code;
    nl();
  }
  void line_i(int code, std::int64_t v) {
    out << code;
    sep();
    i64(v);
    nl();
  }
  void line_text(int code, const std::string& s) {
    out << code;
    sep();
    text(s);
    nl();
  }
  void line_lex(int code, const Span& sp) {
    out << code;
    sep();
    lexeme(sp);
    nl();
  }
  void line_text_i(int code, const std::string& s, std::int64_t v) {
    out << code;
    sep();
    text(s);
    sep();
    i64(v);
    nl();
  }
  void line_text_ii(int code, const std::string& s, std::int64_t a, std::int64_t b) {
    out << code;
    sep();
    text(s);
    sep();
    i64(a);
    sep();
    i64(b);
    nl();
  }
  void line_text_iii(int code, const std::string& s, std::int64_t a, std::int64_t b,
                     std::int64_t c) {
    out << code;
    sep();
    text(s);
    sep();
    i64(a);
    sep();
    i64(b);
    sep();
    i64(c);
    nl();
  }
  void line_ii(int code, std::int64_t a, std::int64_t b) {
    out << code;
    sep();
    i64(a);
    sep();
    i64(b);
    nl();
  }
  void line_iii(int code, std::int64_t a, std::int64_t b, std::int64_t c) {
    out << code;
    sep();
    i64(a);
    sep();
    i64(b);
    sep();
    i64(c);
    nl();
  }
  void line_text_text(int code, const std::string& a, const std::string& b) {
    out << code;
    sep();
    text(a);
    sep();
    text(b);
    nl();
  }
  void line_i_text_i(int code, std::int64_t a, const std::string& b, std::int64_t c) {
    out << code;
    sep();
    i64(a);
    sep();
    text(b);
    sep();
    i64(c);
    nl();
  }
  void line_text_i_text(int code, const std::string& a, std::int64_t b, const std::string& c) {
    out << code;
    sep();
    text(a);
    sep();
    i64(b);
    sep();
    text(c);
    nl();
  }
};

void dump_expr(const Expr& e, Dumper& d);
void dump_type(const TypeExpr& t, Dumper& d);
void dump_stmt(const Stmt& s, Dumper& d);

int binop_token_kind(BinOp b) {
  switch (b) {
    case BinOp::Add: return 59;        // Plus
    case BinOp::Sub: return 60;        // Minus
    case BinOp::Mul: return 61;        // Star
    case BinOp::Div: return 63;        // Slash
    case BinOp::Mod: return 65;        // Percent
    case BinOp::FloorDiv: return 64;   // SlashSlash
    case BinOp::Pow: return 62;        // StarStar
    case BinOp::MatMul: return 76;     // At
    case BinOp::Le: return 66;
    case BinOp::Lt: return 67;
    case BinOp::Ge: return 68;
    case BinOp::Gt: return 69;
    case BinOp::Eq: return 70;         // EqEq
    case BinOp::Ne: return 71;
    case BinOp::And: return 37;        // KwAnd
    case BinOp::Or: return 38;         // KwOr
    case BinOp::Implies: return 57;    // Arrow
  }
  return 0;
}

void dump_expr(const Expr& e, Dumper& d) {
  switch (e.kind) {
    case Expr::Kind::IntLit:
      d.line_lex(60, e.span);
      break;
    case Expr::Kind::FloatLit:
      d.line_lex(61, e.span);
      break;
    case Expr::Kind::BinaryLit:
      d.line_lex(62, e.span);
      break;
    case Expr::Kind::StringLit:
      d.line_lex(63, e.span);
      break;
    case Expr::Kind::Ident:
      d.line_lex(64, e.span);
      break;
    case Expr::Kind::Call:
      d.line_text(66, e.ident);
      for (const auto& a : e.args) {
        dump_expr(*a, d);
      }
      break;
    case Expr::Kind::BinOp:
      dump_expr(*e.lhs, d);
      d.line_i(65, binop_token_kind(e.bin_op));
      dump_expr(*e.rhs, d);
      break;
    case Expr::Kind::UnaryNot:
      d.line(67);
      if (e.operand) {
        dump_expr(*e.operand, d);
      }
      break;
    case Expr::Kind::UnaryMinus:
      d.line(72);
      if (e.operand) {
        dump_expr(*e.operand, d);
      }
      break;
    case Expr::Kind::Index:
      dump_expr(*e.base, d);
      d.line(68);
      if (e.index) {
        dump_expr(*e.index, d);
      }
      break;
    case Expr::Kind::FieldAccess:
      dump_expr(*e.base, d);
      d.line_text(69, e.field_name);
      break;
    case Expr::Kind::MethodCall:
      dump_expr(*e.base, d);
      d.line_text(70, e.field_name);
      for (const auto& a : e.args) {
        dump_expr(*a, d);
      }
      break;
    case Expr::Kind::Await:
      d.line(71);
      if (e.operand) {
        dump_expr(*e.operand, d);
      }
      break;
    case Expr::Kind::Conditional:
      d.line(83);
      if (e.operand) {
        dump_expr(*e.operand, d);
      }
      if (e.cond) {
        dump_expr(*e.cond, d);
      }
      if (e.rhs) {
        dump_expr(*e.rhs, d);
      }
      break;
  }
}

void dump_type(const TypeExpr& t, Dumper& d) {
  switch (t.kind) {
    case TypeKind::Named:
      d.line_text_i(20, t.name, t.is_var ? 1 : 0);
      break;
    case TypeKind::Array:
      d.line_ii(21, t.is_var ? 1 : 0, t.array_size);
      if (t.elem) {
        dump_type(*t.elem, d);
      }
      break;
    case TypeKind::Refinement:
      d.line_text_i(22, t.refinement_var, 0);
      if (t.refinement_base) {
        dump_type(*t.refinement_base, d);
      }
      if (t.refinement_pred) {
        dump_expr(*t.refinement_pred, d);
      }
      break;
    case TypeKind::TypeApp:
      if (t.name == "simd") {
        d.line_i(28, t.is_var ? 1 : 0);
        for (const auto& a : t.type_args) {
          dump_type(*a, d);
        }
        // The lane count literal (when present) comes after the first arg in
        // the source, so it prints as a trailing marker node (code 83).
        if (t.array_size != 0) {
          d.line_i(83, t.array_size);
        }
      } else {
        d.line_text_ii(23, t.name, t.is_var ? 1 : 0, 0);
        for (const auto& a : t.type_args) {
          dump_type(*a, d);
        }
        // `tuple[T, ...]`: the Ellipsis follows the first arg, so the marker
        // (code 82) prints after the args on both implementations.
        if (t.name == "tuple" && t.tuple_variadic) {
          d.line(82);
        }
      }
      break;
    case TypeKind::Callable:
      d.line_i(24, t.is_var ? 1 : 0);
      for (const auto& a : t.type_args) {
        dump_type(*a, d);
      }
      if (t.callable_ret) {
        dump_type(*t.callable_ret, d);
      }
      break;
    case TypeKind::GenericParam:
      d.line_text_i(25, t.name, t.is_var ? 1 : 0);
      break;
    case TypeKind::NamedTuple:
      d.line_ii(26, t.is_var ? 1 : 0, t.tuple_variadic ? 1 : 0);
      for (const auto& f : t.named_fields) {
        d.line_text_ii(27, f.name, f.optional ? 1 : 0,
                       f.visibility == Visibility::Private ? 1 : 0);
        if (f.type) {
          dump_type(*f.type, d);
        }
      }
      break;
  }
}

void dump_contract(const Contract& c, Dumper& d) {
  int kind = 0;
  switch (c.kind) {
    case ContractKind::Requires: kind = 0; break;
    case ContractKind::Ensures: kind = 1; break;
    case ContractKind::Decreases: kind = 2; break;
    case ContractKind::Invariant: kind = 3; break;
    case ContractKind::ProbEnsures: kind = 4; break;
  }
  // The proposition is parsed before the given/samples tail, so the expr
  // prints first and the contract line after it (both implementations).
  if (c.expr) {
    dump_expr(*c.expr, d);
  }
  d.line_i_text_i(7, kind, c.prob_given, c.prob_samples);
}

void dump_decorator(const Decorator& deco, Dumper& d) {
  d.line_text(8, deco.name);
  for (const auto& arg : deco.args) {
    d.line_text(9, arg.name);
    if (arg.value) {
      dump_expr(*arg.value, d);
    }
  }
}

void dump_block(const std::vector<Stmt>& body, Dumper& d) {
  d.line(51);
  for (const auto& s : body) {
    dump_stmt(s, d);
  }
  d.line(52);
}

void dump_stmt(const Stmt& s, Dumper& d) {
  for (const auto& deco : s.decorators) {
    dump_decorator(deco, d);
  }
  switch (s.kind) {
    case Stmt::Kind::Return:
      d.line(40);
      if (s.expr) {
        dump_expr(*s.expr, d);
      }
      break;
    case Stmt::Kind::If:
      d.line(41);
      if (s.cond) {
        dump_expr(*s.cond, d);
      }
      dump_block(s.then_body, d);
      if (s.else_body) {
        dump_block(*s.else_body, d);
      }
      break;
    case Stmt::Kind::While:
      d.line(42);
      if (s.cond) {
        dump_expr(*s.cond, d);
      }
      dump_block(s.while_body, d);
      break;
    case Stmt::Kind::For:
      d.line_text_ii(43, s.for_iter, s.for_start, s.for_end);
      for (const auto& c : s.for_contracts) {
        dump_contract(c, d);
      }
      dump_block(s.for_body, d);
      break;
    case Stmt::Kind::ParallelFor:
      d.line_text_ii(44, s.par_iter, s.par_start, s.par_end);
      for (const auto& c : s.par_contracts) {
        dump_contract(c, d);
      }
      // The parallel-for body block is optional; the AST records only the
      // parsed statements, so a missing `=`/block and an empty parsed block
      // both dump as an empty block on both implementations.
      dump_block(s.par_body, d);
      break;
    case Stmt::Kind::Break:
      d.line(45);
      break;
    case Stmt::Kind::Continue:
      d.line(46);
      break;
    case Stmt::Kind::Expr:
      d.line(47);
      if (s.expr) {
        dump_expr(*s.expr, d);
      }
      break;
    case Stmt::Kind::VarDecl:
      d.line_text(48, s.var_name);
      dump_type(s.var_type, d);
      if (s.init) {
        dump_expr(*s.init, d);
      }
      break;
    case Stmt::Kind::Borrow:
      d.line_text_i(49, s.var_name, s.borrow_mut ? 1 : 0);
      if (s.init) {
        dump_expr(*s.init, d);
      }
      break;
    case Stmt::Kind::Assign:
      d.line(50);
      if (s.init) {
        dump_expr(*s.init, d);
      }
      if (s.expr) {
        dump_expr(*s.expr, d);
      }
      break;
  }
}

void dump_param(const Param& p, Dumper& d) {
  d.line_text(6, p.name);
  dump_type(p.type, d);
}

/// Dump a proc with no body — used for trait methods (the C++ parser never
/// parses a body for those).
void dump_proc_no_body(const ProcDecl& proc, Dumper& d) {
  d.line_text_iii(4, proc.name,
                  proc.visibility == Visibility::Private ? 1 : 0,
                  proc.is_extern ? 1 : 0, proc.is_async ? 1 : 0);
  for (const auto& deco : proc.decorators) {
    dump_decorator(deco, d);
  }
  for (std::size_t i = 0; i < proc.type_params.size(); ++i) {
    const std::string bound =
        i < proc.type_param_bounds.size() ? proc.type_param_bounds[i] : "";
    d.line_text_text(5, proc.type_params[i], bound);
  }
  for (const auto& p : proc.params) {
    dump_param(p, d);
  }
  for (const auto& r : proc.raises) {
    d.line_text(10, r);
  }
  if (proc.ret_type) {
    dump_type(*proc.ret_type, d);
  }
  for (const auto& c : proc.contracts) {
    dump_contract(c, d);
  }
}

void dump_proc(const ProcDecl& proc, Dumper& d) {
  dump_proc_no_body(proc, d);
  if (!proc.is_extern) {
    dump_block(proc.body, d);
  }
}

void dump_theorem(const TheoremDecl& thm, Dumper& d) {
  d.line_text_ii(3, thm.name, thm.is_axiom ? 1 : 0, thm.is_lemma ? 1 : 0);
  for (const auto& p : thm.params) {
    dump_param(p, d);
  }
  if (thm.proposition) {
    dump_expr(*thm.proposition, d);
  }
}

void dump_type_alias(const TypeAlias& alias, Dumper& d) {
  d.line_text_i_text(80, alias.name, static_cast<int>(alias.alias_kind), alias.base_object);
  for (const auto& tp : alias.type_params) {
    d.line_text_text(5, tp, "");
  }
  switch (alias.alias_kind) {
    case AliasKind::TypedDict:
    case AliasKind::Object:
      for (const auto& f : alias.fields) {
        d.line_text_ii(27, f.name, f.optional ? 1 : 0,
                       f.visibility == Visibility::Private ? 1 : 0);
        if (f.type) {
          dump_type(*f.type, d);
        }
      }
      break;
    case AliasKind::Enum:
      for (const auto& v : alias.enum_variants) {
        d.line_text(81, v);
      }
      break;
    case AliasKind::Trait:
      for (const auto& m : alias.trait_methods) {
        dump_proc_no_body(m, d);
      }
      break;
    case AliasKind::Type:
      dump_type(alias.definition, d);
      break;
  }
}

struct DeclRef {
  std::size_t start = 0;
  int tag = 0;  // 0 import, 1 type alias, 2 error, 3 proc, 4 theorem
  std::size_t idx = 0;
};

}  // namespace

std::string dump_module_ast(const Module& m, std::string_view source) {
  Dumper d(source);
  std::vector<DeclRef> decls;
  for (std::size_t i = 0; i < m.imports.size(); ++i) {
    decls.push_back({m.imports[i].span.start, 0, i});
  }
  for (std::size_t i = 0; i < m.types.size(); ++i) {
    decls.push_back({m.types[i].span.start, 1, i});
  }
  for (std::size_t i = 0; i < m.errors.size(); ++i) {
    decls.push_back({m.errors[i].span.start, 2, i});
  }
  for (std::size_t i = 0; i < m.procs.size(); ++i) {
    decls.push_back({m.procs[i].span.start, 3, i});
  }
  for (std::size_t i = 0; i < m.theorems.size(); ++i) {
    decls.push_back({m.theorems[i].span.start, 4, i});
  }
  std::stable_sort(decls.begin(), decls.end(),
                   [](const DeclRef& a, const DeclRef& b) { return a.start < b.start; });
  for (const auto& ref : decls) {
    switch (ref.tag) {
      case 0: {
        const auto& imp = m.imports[ref.idx];
        d.line_text_text(1, imp.module, imp.alias);
        break;
      }
      case 1:
        dump_type_alias(m.types[ref.idx], d);
        break;
      case 2: {
        const auto& err = m.errors[ref.idx];
        d.line_text_text(2, err.name, err.message_template);
        break;
      }
      case 3:
        dump_proc(m.procs[ref.idx], d);
        break;
      case 4:
        dump_theorem(m.theorems[ref.idx], d);
        break;
    }
  }
  return d.out.str();
}

}  // namespace li
