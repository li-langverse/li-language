#include "li/lexer.hpp"
#include "li/parser.hpp"

#include <utility>

namespace li {

namespace {

struct Parser {
  const std::vector<Token>& tokens;
  std::size_t i = 0;
  std::string file;
  DiagnosticBag& diags;

  explicit Parser(const std::vector<Token>& toks, std::string file_name,
                  DiagnosticBag& bag)
      : tokens(toks), file(std::move(file_name)), diags(bag) {}

  const Token& cur() const { return tokens[i]; }
  const Token& peek(std::size_t off = 0) const {
    const std::size_t j = i + off;
    return j < tokens.size() ? tokens[j] : tokens.back();
  }
  bool at(TokenKind k) const { return cur().kind == k; }
  bool accept(TokenKind k) {
    if (at(k)) {
      i++;
      return true;
    }
    return false;
  }
  bool expect(TokenKind k, const char* what) {
    if (!accept(k)) {
      SourceLoc loc{file, cur().line, cur().column, cur().start};
      diags.error(loc, std::string("expected ") + what);
      return false;
    }
    return true;
  }
  void skip_newlines() {
    while (at(TokenKind::Newline)) {
      i++;
    }
  }

  /// Newlines and indents inside `(...)` parameter lists (not proc bodies).
  void skip_param_layout() {
    skip_newlines();
    while (at(TokenKind::Indent)) {
      i++;
    }
  }

  SourceLoc loc(const Token& t) const {
    return SourceLoc{file, t.line, t.column, t.start};
  }

  std::unique_ptr<Expr> parse_expr(int min_prec = 0);
  std::unique_ptr<Expr> parse_primary();
  std::unique_ptr<Expr> parse_postfix(std::unique_ptr<Expr> base);
  TypeExpr parse_type();
  std::vector<std::string> parse_type_params(std::vector<std::string>* bounds_out = nullptr);
  std::vector<ProcDecl> parse_trait_methods();
  ProcDecl parse_trait_method();
  std::vector<TypeField> parse_type_fields();
  Param parse_param();
  std::unique_ptr<Expr> parse_contract_expr();
  void parse_prob_ensures_tail(Contract& c);
  Contract parse_contract();
  std::unique_ptr<Expr> parse_decorator_value();
  Decorator parse_decorator();
  std::vector<Decorator> parse_decorator_list();
  std::vector<Stmt> parse_block();
  Stmt parse_stmt();
  ProcDecl parse_proc(bool is_extern = false);
  TypeAlias parse_type_alias();
  ErrorDecl parse_error_decl();
  ImportDecl parse_import();
  TheoremDecl parse_theorem(bool is_axiom, bool is_lemma);
  bool accept_def_kw() {
    if (at(TokenKind::KwDef)) {
      i++;
      return true;
    }
    if (at(TokenKind::KwProc)) {
      diags.error(loc(cur()), "use 'def' for Li procedures; 'proc' is removed");
      i++;
      return false;
    }
    return false;
  }
  bool at_fn_kw() const { return at(TokenKind::KwDef) || at(TokenKind::KwProc); }
  bool at_for_kw() const {
    return at(TokenKind::KwFor) || (at(TokenKind::Ident) && cur().text == "for");
  }
  bool consume_for_kw() {
    if (at_for_kw()) {
      i++;
      return true;
    }
    return false;
  }

  bool parse_module(Module& out) {
    skip_newlines();
    while (!at(TokenKind::Eof)) {
      if (at(TokenKind::KwImport)) {
        out.imports.push_back(parse_import());
        skip_newlines();
      } else if (at(TokenKind::KwExtern)) {
        i++;
        if (at(TokenKind::KwDef)) {
          i++;
        } else if (at(TokenKind::KwProc)) {
          diags.error(loc(cur()),
                      "use 'extern def' for trusted FFI; 'extern proc' is removed");
          i++;
        } else {
          diags.error(loc(cur()), "expected 'def' after 'extern'");
          return false;
        }
        out.procs.push_back(parse_proc(true));
        skip_newlines();
      } else if (at(TokenKind::KwAsync)) {
        i++;
        ProcDecl proc = parse_proc(false);
        proc.is_async = true;
        out.procs.push_back(std::move(proc));
        skip_newlines();
      } else if (at(TokenKind::At)) {
        std::vector<Decorator> decos = parse_decorator_list();
        if (!at_fn_kw()) {
          diags.error(loc(cur()), "expected 'def' after decorators");
          return false;
        }
        ProcDecl proc = parse_proc(false);
        proc.decorators = std::move(decos);
        out.procs.push_back(std::move(proc));
        skip_newlines();
      } else if (at(TokenKind::KwPrivate) || at(TokenKind::KwPublic)) {
        if (peek(1).kind == TokenKind::KwDef || at_fn_kw()) {
          out.procs.push_back(parse_proc(false));
        } else {
          diags.error(loc(cur()), "expected 'def' after visibility");
          return false;
        }
        skip_newlines();
      } else if (at(TokenKind::KwDef)) {
        out.procs.push_back(parse_proc(false));
        skip_newlines();
      } else if (at(TokenKind::KwProc)) {
        diags.error(loc(cur()), "use 'def' for Li procedures; 'proc' is removed");
        i++;
        skip_newlines();
      } else if (at(TokenKind::KwType)) {
        out.types.push_back(parse_type_alias());
        skip_newlines();
      } else if (at(TokenKind::KwError)) {
        out.errors.push_back(parse_error_decl());
        skip_newlines();
      } else if (at(TokenKind::KwAxiom) || at(TokenKind::KwTheorem) || at(TokenKind::KwLemma)) {
        const Token kw = cur();
        i++;
        out.theorems.push_back(parse_theorem(kw.kind == TokenKind::KwAxiom,
                                             kw.kind == TokenKind::KwLemma));
        skip_newlines();
      } else {
        diags.error(loc(cur()), "expected top-level declaration");
        return false;
      }
    }
    return true;
  }
};

std::unique_ptr<Expr> Parser::parse_primary() {
  const Token& t = cur();
  if (t.kind == TokenKind::KwAwait) {
    i++;
    auto inner = parse_primary();
    if (!inner) {
      return nullptr;
    }
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::Await;
    e->span = {t.start, inner->span.end};
    e->operand = std::move(inner);
    return parse_postfix(std::move(e));
  }
  if (t.kind == TokenKind::KwNone) {
    diags.error(loc(t), "None requires an explicit Option[T] context; use unit for no return value (see docs/language/option-types.md; example: `def find() -> Option[int]` returns `None` when no value exists)");
    i++;
    return nullptr;
  }
  if (t.kind == TokenKind::KwSome) {
    diags.error(loc(t), "Some is reserved for Option[T] construction; see docs/language/option-types.md");
    i++;
    return nullptr;
  }
  if (t.kind == TokenKind::StringLit) {
    i++;
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::StringLit;
    e->span = {t.start, t.end};
    const std::string raw(t.text);
    if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
      e->str_value = raw.substr(1, raw.size() - 2);
    } else {
      e->str_value = raw;
    }
    return parse_postfix(std::move(e));
  }
  if (t.kind == TokenKind::IntLit) {
    i++;
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::IntLit;
    e->span = {t.start, t.end};
    e->int_value = t.int_value;
    e->lit_suffix = t.lit_suffix;
    return parse_postfix(std::move(e));
  }
  if (t.kind == TokenKind::FloatLit) {
    i++;
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::FloatLit;
    e->span = {t.start, t.end};
    e->float_value = t.float_value;
    e->lit_suffix = t.lit_suffix;
    return parse_postfix(std::move(e));
  }
  if (t.kind == TokenKind::BinaryLit) {
    i++;
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::BinaryLit;
    e->span = {t.start, t.end};
    e->int_value = t.int_value;
    e->str_value = std::string(t.text);
    return parse_postfix(std::move(e));
  }
  if (t.kind == TokenKind::Ident || t.kind == TokenKind::KwResult ||
      t.kind == TokenKind::KwTrue || t.kind == TokenKind::KwFalse) {
    const std::string name(t.text);
    i++;
    auto e = std::make_unique<Expr>();
    e->span = {t.start, t.end};
    if (accept(TokenKind::LParen)) {
      e->kind = Expr::Kind::Call;
      e->ident = name;
      if (!at(TokenKind::RParen)) {
        do {
          e->args.push_back(parse_expr());
        } while (accept(TokenKind::Comma));
      }
      if (!expect(TokenKind::RParen, "')'")) {
        return nullptr;
      }
    } else {
      e->kind = Expr::Kind::Ident;
      e->ident = name;
    }
    return parse_postfix(std::move(e));
  }
  if (accept(TokenKind::LParen)) {
    auto inner = parse_expr();
    if (accept(TokenKind::Comma)) {
      diags.error(loc(cur()), "tuple literals are not supported; use an object type");
      while (!at(TokenKind::RParen) && !at(TokenKind::Eof)) i++;
    }
    if (!expect(TokenKind::RParen, "')'")) {
      return nullptr;
    }
    return parse_postfix(std::move(inner));
  }
  diags.error(loc(t), "expected expression");
  return nullptr;
}

namespace {

bool peel_method_receiver(Expr* field_access, std::string* method_name,
                          std::unique_ptr<Expr>* receiver_out) {
  if (!field_access || field_access->kind != Expr::Kind::FieldAccess || !method_name ||
      !receiver_out) {
    return false;
  }
  *method_name = field_access->field_name;
  if (field_access->base->kind == Expr::Kind::FieldAccess) {
    return peel_method_receiver(field_access->base.get(), method_name, receiver_out);
  }
  *receiver_out = std::move(field_access->base);
  return true;
}

}  // namespace

std::unique_ptr<Expr> Parser::parse_postfix(std::unique_ptr<Expr> base) {
  while (accept(TokenKind::Dot)) {
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected field name after '.'");
      return base;
    }
    const Token field = cur();
    i++;
    auto node = std::make_unique<Expr>();
    node->kind = Expr::Kind::FieldAccess;
    node->span = {base->span.start, field.end};
    node->base = std::move(base);
    node->field_name = std::string(field.text);
    base = std::move(node);
  }
  while (accept(TokenKind::LBracket)) {
    auto idx = parse_expr();
    if (!expect(TokenKind::RBracket, "']'")) {
      return nullptr;
    }
    auto node = std::make_unique<Expr>();
    node->kind = Expr::Kind::Index;
    node->span = {base->span.start, tokens[i - 1].end};
    node->base = std::move(base);
    node->index = std::move(idx);
    base = std::move(node);
  }
  if (base && base->kind == Expr::Kind::FieldAccess && accept(TokenKind::LParen)) {
    std::string method_name;
    std::unique_ptr<Expr> receiver;
    if (peel_method_receiver(base.get(), &method_name, &receiver)) {
      auto call = std::make_unique<Expr>();
      call->kind = Expr::Kind::MethodCall;
      call->span = base->span;
      call->field_name = method_name;
      call->base = std::move(receiver);
      if (!at(TokenKind::RParen)) {
        do {
          call->args.push_back(parse_expr());
        } while (accept(TokenKind::Comma));
      }
      if (!expect(TokenKind::RParen, "')'")) {
        return nullptr;
      }
      call->span.end = tokens[i - 1].end;
      return parse_postfix(std::move(call));
    }
  }
  return base;
}

int prec(TokenKind k) {
  switch (k) {
    case TokenKind::Arrow: return 0;
    case TokenKind::KwOr: return 1;
    case TokenKind::KwAnd: return 2;
    case TokenKind::EqEq:
    case TokenKind::Ne:
    case TokenKind::Lt:
    case TokenKind::Le:
    case TokenKind::Gt:
    case TokenKind::Ge: return 3;
    case TokenKind::Plus:
    case TokenKind::Minus: return 4;
    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent:
    case TokenKind::SlashSlash: return 5;
    case TokenKind::StarStar: return 6;
    case TokenKind::At: return 6;
    default: return -1;
  }
}

BinOp binop(TokenKind k) {
  switch (k) {
    case TokenKind::Arrow: return BinOp::Implies;
    case TokenKind::Plus: return BinOp::Add;
    case TokenKind::Minus: return BinOp::Sub;
    case TokenKind::Star: return BinOp::Mul;
    case TokenKind::StarStar: return BinOp::Pow;
    case TokenKind::Slash: return BinOp::Div;
    case TokenKind::SlashSlash: return BinOp::FloorDiv;
    case TokenKind::Percent: return BinOp::Mod;
    case TokenKind::At: return BinOp::MatMul;
    case TokenKind::Le: return BinOp::Le;
    case TokenKind::Lt: return BinOp::Lt;
    case TokenKind::Ge: return BinOp::Ge;
    case TokenKind::Gt: return BinOp::Gt;
    case TokenKind::EqEq: return BinOp::Eq;
    case TokenKind::Ne: return BinOp::Ne;
    case TokenKind::KwAnd: return BinOp::And;
    case TokenKind::KwOr: return BinOp::Or;
    default: return BinOp::Add;
  }
}

std::unique_ptr<Expr> Parser::parse_expr(int min_prec) {
  std::unique_ptr<Expr> left;
  if (at(TokenKind::KwNot)) {
    const Token t = cur();
    i++;
    left = std::make_unique<Expr>();
    left->kind = Expr::Kind::UnaryNot;
    left->span = {t.start, t.end};
    left->operand = parse_expr(100);
  } else if (at(TokenKind::Minus)) {
    const Token t = cur();
    i++;
    auto inner = parse_primary();
    if (inner && inner->kind == Expr::Kind::IntLit) {
      inner->int_value = -inner->int_value;
      left = std::move(inner);
    } else if (inner && inner->kind == Expr::Kind::FloatLit) {
      inner->float_value = -inner->float_value;
      left = std::move(inner);
    } else if (inner) {
      // `-x` on a general expression: keep the negation as a real AST node
      // (previously the minus was silently dropped, producing wrong code).
      left = std::make_unique<Expr>();
      left->kind = Expr::Kind::UnaryMinus;
      left->span = {t.start, t.end};
      left->operand = std::move(inner);
    } else {
      left = std::move(inner);
    }
  } else {
    left = parse_primary();
  }
  if (!left) {
    return nullptr;
  }
  while (true) {
    const int p = prec(cur().kind);
    if (p < 0 || p < min_prec) {
      break;
    }
    const Token op = cur();
    i++;
    auto right = parse_expr(p + 1);
    if (!right) {
      return nullptr;
    }
    auto node = std::make_unique<Expr>();
    node->kind = Expr::Kind::BinOp;
    node->span = {left->span.start, right->span.end};
    node->bin_op = binop(op.kind);
    node->lhs = std::move(left);
    node->rhs = std::move(right);
    left = std::move(node);
  }
  // Conditional expression: expr if cond else expr
  if (at(TokenKind::KwIf)) {
    const Token if_tok = cur();
    i++;
    auto cond = parse_expr();
    if (!cond) {
      return nullptr;
    }
    if (!expect(TokenKind::KwElse, "'else' in conditional expression")) {
      return nullptr;
    }
    auto else_expr = parse_expr();
    if (!else_expr) {
      return nullptr;
    }
    auto node = std::make_unique<Expr>();
    node->kind = Expr::Kind::Conditional;
    node->span = {left->span.start, else_expr->span.end};
    node->operand = std::move(left);
    node->cond = std::move(cond);
    node->rhs = std::move(else_expr);
    return node;
  }
  return left;
}

TypeExpr Parser::parse_type() {
  if (accept(TokenKind::LBrace)) {
    TypeExpr ty;
    ty.kind = TypeKind::Refinement;
    ty.span = {peek(-1).start, peek(-1).end};
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected refinement binding name");
      return ty;
    }
    ty.refinement_var = std::string(cur().text);
    i++;
    expect(TokenKind::Colon, "':'");
    ty.refinement_base = std::make_unique<TypeExpr>(parse_type());
    if (!expect(TokenKind::Pipe, "'|'")) {
      return ty;
    }
    ty.refinement_pred = parse_contract_expr();
    if (!expect(TokenKind::RBrace, "'}'")) {
      return ty;
    }
    ty.span.end = peek(-1).end;
    return ty;
  }
  TypeExpr ty;
  if (at(TokenKind::KwVar)) {
    ty.is_var = true;
    i++;
  }
  const Token& t = cur();
  ty.span = {t.start, t.end};
  if (t.kind != TokenKind::Ident && t.kind != TokenKind::KwCallable &&
      t.kind != TokenKind::KwProtocol) {
    diags.error(loc(t), "expected type");
    return ty;
  }
  const std::string name(t.text);
  i++;
  if (name == "Callable" && accept(TokenKind::LBracket)) {
    ty.kind = TypeKind::Callable;
    ty.name = "Callable";
    if (!accept(TokenKind::LBracket)) {
      diags.error(loc(cur()), "expected '[' for Callable argument list");
      return ty;
    }
    if (!at(TokenKind::RBracket)) {
      do {
        ty.type_args.push_back(std::make_unique<TypeExpr>(parse_type()));
      } while (accept(TokenKind::Comma));
    }
    expect(TokenKind::RBracket, "']'");
    expect(TokenKind::Comma, "','");
    ty.callable_ret = std::make_unique<TypeExpr>(parse_type());
    expect(TokenKind::RBracket, "']'");
    return ty;
  }
  if (name == "array" && accept(TokenKind::LBracket)) {
    ty.kind = TypeKind::Array;
    ty.name = "array";
    if (!at(TokenKind::IntLit)) {
      diags.error(loc(cur()), "expected array size");
      return ty;
    }
    ty.array_size = cur().int_value;
    i++;
    expect(TokenKind::Comma, "','");
    ty.elem = std::make_unique<TypeExpr>(parse_type());
    expect(TokenKind::RBracket, "']'");
  } else if (name == "tuple" && accept(TokenKind::LBracket)) {
    ty.name = "tuple";
    if (at(TokenKind::Ident) && peek(1).kind == TokenKind::Colon) {
      ty.kind = TypeKind::NamedTuple;
      if (!at(TokenKind::RBracket)) {
        do {
          TypeField field;
          field.name = std::string(cur().text);
          i++;
          expect(TokenKind::Colon, "':'");
          field.type = std::make_unique<TypeExpr>(parse_type());
          ty.named_fields.push_back(std::move(field));
        } while (accept(TokenKind::Comma));
      }
    } else {
      ty.kind = TypeKind::TypeApp;
      if (!at(TokenKind::RBracket)) {
        ty.type_args.push_back(std::make_unique<TypeExpr>(parse_type()));
        if (accept(TokenKind::Comma)) {
          if (at(TokenKind::Ellipsis)) {
            i++;
            ty.tuple_variadic = true;
          } else {
            do {
              ty.type_args.push_back(std::make_unique<TypeExpr>(parse_type()));
            } while (accept(TokenKind::Comma));
          }
        }
      }
    }
    expect(TokenKind::RBracket, "']'");
  } else if (name == "simd" && accept(TokenKind::LBracket)) {
    ty.kind = TypeKind::TypeApp;
    ty.name = "simd";
    ty.type_args.push_back(std::make_unique<TypeExpr>(parse_type()));
    expect(TokenKind::Comma, "','");
    if (at(TokenKind::IntLit)) {
      ty.array_size = cur().int_value;
      i++;
    } else {
      ty.type_args.push_back(std::make_unique<TypeExpr>(parse_type()));
    }
    expect(TokenKind::RBracket, "']'");
  } else {
    ty.kind = TypeKind::Named;
    ty.name = name;
    if (accept(TokenKind::LBracket)) {
      ty.kind = TypeKind::TypeApp;
      if (!at(TokenKind::RBracket)) {
        do {
          ty.type_args.push_back(std::make_unique<TypeExpr>(parse_type()));
        } while (accept(TokenKind::Comma));
      }
      expect(TokenKind::RBracket, "']'");
    }
  }
  return ty;
}

std::vector<std::string> Parser::parse_type_params(std::vector<std::string>* bounds_out) {
  std::vector<std::string> params;
  if (!accept(TokenKind::LBracket)) {
    return params;
  }
  if (!at(TokenKind::RBracket)) {
    do {
      if (!at(TokenKind::Ident)) {
        diags.error(loc(cur()), "expected type parameter name");
        break;
      }
      params.push_back(std::string(cur().text));
      i++;
      if (bounds_out) {
        bounds_out->push_back({});
        if (accept(TokenKind::Colon)) {
          if (!at(TokenKind::Ident)) {
            diags.error(loc(cur()), "expected trait name after ':' in type parameter");
          } else {
            bounds_out->back() = std::string(cur().text);
            i++;
          }
        }
      }
    } while (accept(TokenKind::Comma));
  }
  expect(TokenKind::RBracket, "']'");
  return params;
}

Param Parser::parse_param() {
  skip_param_layout();
  const Token& t = cur();
  Param p;
  p.span = {t.start, t.end};
  p.name = std::string(t.text);
  i++;
  expect(TokenKind::Colon, "':'");
  p.type = parse_type();
  return p;
}

std::unique_ptr<Expr> Parser::parse_contract_expr() {
  skip_newlines();
  return parse_expr();
}

void Parser::parse_prob_ensures_tail(Contract& c) {
  if (c.kind != ContractKind::ProbEnsures) {
    return;
  }
  skip_newlines();
  while (at(TokenKind::Ident) && (cur().text == "given" || cur().text == "samples")) {
    const std::string tag = std::string(cur().text);
    i++;
    if (tag == "given") {
      if (!at(TokenKind::Ident)) {
        diags.error(loc(cur()), "expected axiom name after 'given'");
        return;
      }
      c.prob_given = std::string(cur().text);
      i++;
    } else {
      if (!at(TokenKind::IntLit)) {
        diags.error(loc(cur()), "expected sample count after 'samples'");
        return;
      }
      c.prob_samples = cur().int_value;
      i++;
    }
    skip_newlines();
  }
}

Contract Parser::parse_contract() {
  Contract c;
  const Token kw = cur();
  if (kw.kind == TokenKind::KwRequires) {
    c.kind = ContractKind::Requires;
  } else if (kw.kind == TokenKind::KwEnsures) {
    c.kind = ContractKind::Ensures;
  } else if (kw.kind == TokenKind::KwProbEnsures) {
    c.kind = ContractKind::ProbEnsures;
  } else if (kw.kind == TokenKind::KwDecreases) {
    c.kind = ContractKind::Decreases;
  } else {
    c.kind = ContractKind::Invariant;
  }
  c.span = {kw.start, kw.end};
  i++;
  c.expr = parse_contract_expr();
  parse_prob_ensures_tail(c);
  skip_newlines();
  return c;
}

std::vector<Stmt> Parser::parse_block() {
  std::vector<Stmt> body;
  skip_newlines();
  if (!expect(TokenKind::Indent, "indented block")) {
    return body;
  }
  skip_newlines();
  while (!at(TokenKind::Dedent) && !at(TokenKind::Eof)) {
    const std::size_t before = i;
    body.push_back(parse_stmt());
    if (i == before) {
      diags.error(loc(cur()), "failed to parse statement");
      break;
    }
    skip_newlines();
  }
  expect(TokenKind::Dedent, "dedent");
  return body;
}

std::unique_ptr<Expr> Parser::parse_decorator_value() {
  const Token& t = cur();
  if (t.kind == TokenKind::Ident || t.kind == TokenKind::KwTrue ||
      t.kind == TokenKind::KwFalse) {
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::Ident;
    e->span = {t.start, t.end};
    e->ident = std::string(t.text);
    i++;
    return e;
  }
  if (t.kind == TokenKind::IntLit) {
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::IntLit;
    e->span = {t.start, t.end};
    e->int_value = t.int_value;
    e->lit_suffix = t.lit_suffix;
    i++;
    return e;
  }
  if (t.kind == TokenKind::FloatLit) {
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::FloatLit;
    e->span = {t.start, t.end};
    e->float_value = t.float_value;
    e->lit_suffix = t.lit_suffix;
    i++;
    return e;
  }
  if (t.kind == TokenKind::BinaryLit) {
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::BinaryLit;
    e->span = {t.start, t.end};
    e->int_value = t.int_value;
    e->str_value = std::string(t.text);
    i++;
    return e;
  }
  if (t.kind == TokenKind::StringLit) {
    auto e = std::make_unique<Expr>();
    e->kind = Expr::Kind::StringLit;
    e->span = {t.start, t.end};
    e->str_value = std::string(t.text);
    i++;
    return e;
  }
  diags.error(loc(t), "expected decorator argument");
  return nullptr;
}

Decorator Parser::parse_decorator() {
  Decorator deco;
  if (!expect(TokenKind::At, "'@'")) {
    return deco;
  }
  if (!at(TokenKind::Ident) && !at(TokenKind::KwAsync)) {
    diags.error(loc(cur()), "expected decorator name after '@'");
    return deco;
  }
  const Token name = cur();
  deco.span = {name.start, name.end};
  deco.name = std::string(name.text);
  i++;
  if (accept(TokenKind::LParen)) {
    if (!at(TokenKind::RParen)) {
      do {
        DecoratorArg arg;
        if (!at(TokenKind::Ident)) {
          diags.error(loc(cur()), "expected decorator argument name");
          break;
        }
        const Token key = cur();
        arg.name = std::string(key.text);
        i++;
        if (accept(TokenKind::Eq)) {
          arg.value = parse_decorator_value();
        } else {
          arg.value = std::make_unique<Expr>();
          arg.value->kind = Expr::Kind::Ident;
          arg.value->span = {key.start, key.end};
          arg.value->ident = arg.name;
        }
        deco.args.push_back(std::move(arg));
      } while (accept(TokenKind::Comma));
    }
    expect(TokenKind::RParen, "')'");
    deco.span.end = tokens[i > 0 ? i - 1 : 0].end;
  }
  return deco;
}

std::vector<Decorator> Parser::parse_decorator_list() {
  std::vector<Decorator> decos;
  skip_newlines();
  while (at(TokenKind::At)) {
    decos.push_back(parse_decorator());
    skip_newlines();
  }
  return decos;
}

Stmt Parser::parse_stmt() {
  Stmt s;
  if (at(TokenKind::At)) {
    std::vector<Decorator> decos = parse_decorator_list();
    if (at(TokenKind::KwWhile)) {
      const Token t = cur();
      s.kind = Stmt::Kind::While;
      s.span = {t.start, t.end};
      s.decorators = std::move(decos);
      i++;
      s.cond = parse_expr();
      if (at(TokenKind::Colon)) {
        i++;
      }
      skip_newlines();
      s.while_body = parse_block();
      return s;
    }
    if (at(TokenKind::Ident) && cur().text == "parallel") {
      const Token start_tok = cur();
      s.decorators = std::move(decos);
      i++;
      if (!consume_for_kw()) {
        diags.error({file, start_tok.line, 1, start_tok.start},
                    "expected 'for' after 'parallel'");
      }
      s.kind = Stmt::Kind::ParallelFor;
      if (!at(TokenKind::Ident)) {
        diags.error({file, start_tok.line, 1, start_tok.start},
                    "expected loop variable");
      } else {
        s.par_iter = std::string(cur().text);
        i++;
      }
      if (!at(TokenKind::Ident) || cur().text != "in") {
        diags.error({file, start_tok.line, 1, start_tok.start},
                    "expected 'in' in parallel for");
      } else {
        i++;
      }
      if (at(TokenKind::IntLit)) {
        s.par_start = cur().int_value;
        i++;
      }
      if (at(TokenKind::DotDotLt)) {
        i++;
      } else {
        diags.error({file, start_tok.line, 1, start_tok.start},
                    "parallel for requires '..<' range");
      }
      if (at(TokenKind::IntLit)) {
        s.par_end = cur().int_value;
        i++;
      }
      skip_newlines();
      if (accept(TokenKind::Indent)) {
        skip_newlines();
        while (at(TokenKind::KwRequires) || at(TokenKind::KwEnsures) ||
               at(TokenKind::KwProbEnsures) || at(TokenKind::KwDecreases) ||
               at(TokenKind::KwInvariant)) {
          s.par_contracts.push_back(parse_contract());
        }
        expect(TokenKind::Dedent, "dedent");
        skip_newlines();
      }
      if (accept(TokenKind::Eq)) {
        skip_newlines();
        if (at(TokenKind::Indent)) {
          s.par_body = parse_block();
        }
      }
      s.span = {start_tok.start, cur().start};
      return s;
    }
    if (at(TokenKind::KwFor)) {
      const Token start_tok = cur();
      s.kind = Stmt::Kind::For;
      s.decorators = std::move(decos);
      i++;
      if (!at(TokenKind::Ident)) {
        diags.error({file, start_tok.line, 1, start_tok.start},
                    "expected loop variable after 'for'");
      } else {
        s.for_iter = std::string(cur().text);
        i++;
      }
      if (!at(TokenKind::Ident) || cur().text != "in") {
        diags.error({file, start_tok.line, 1, start_tok.start}, "expected 'in' in for loop");
      } else {
        i++;
      }
      if (at(TokenKind::IntLit)) {
        s.for_start = cur().int_value;
        i++;
      }
      if (at(TokenKind::DotDotLt)) {
        i++;
      } else {
        diags.error({file, start_tok.line, 1, start_tok.start}, "for loop requires '..<' range");
      }
      if (at(TokenKind::IntLit)) {
        s.for_end = cur().int_value;
        i++;
      }
      if (at(TokenKind::Colon)) {
        i++;
      }
      skip_newlines();
      s.for_body = parse_block();
      s.span = {start_tok.start, cur().start};
      return s;
    }
    diags.error(loc(cur()), "expected while, for, or parallel for after decorators");
    return s;
  }
  if (at(TokenKind::Ident) && cur().text == "discard") {
    s.kind = Stmt::Kind::Expr;
    s.span = {cur().start, cur().end};
    i++;
    skip_newlines();
    return s;
  }
  if (at(TokenKind::Ident) && cur().text == "borrow") {
    const Token t = cur();
    s.kind = Stmt::Kind::Borrow;
    s.span = {t.start, t.end};
    i++;
    if (at(TokenKind::Ident) && cur().text == "mut") {
      s.borrow_mut = true;
      i++;
    } else if (at(TokenKind::Ident) && cur().text == "imm") {
      i++;
    }
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected borrow binding name");
      return s;
    }
    s.var_name = std::string(cur().text);
    i++;
    expect(TokenKind::Eq, "'='");
    s.init = parse_expr();
    skip_newlines();
    return s;
  }
  if (at(TokenKind::KwVar)) {
    const Token t = cur();
    s.kind = Stmt::Kind::VarDecl;
    s.span = {t.start, t.end};
    i++;
    const Token name = cur();
    s.var_name = std::string(name.text);
    i++;
    expect(TokenKind::Colon, "':'");
    s.var_type = parse_type();
    if (accept(TokenKind::Eq)) {
      s.init = parse_expr();
    }
    skip_newlines();
    return s;
  }
  if (at(TokenKind::KwBreak)) {
    const Token t = cur();
    s.kind = Stmt::Kind::Break;
    s.span = {t.start, t.end};
    i++;
    skip_newlines();
    return s;
  }
  if (at(TokenKind::KwContinue)) {
    const Token t = cur();
    s.kind = Stmt::Kind::Continue;
    s.span = {t.start, t.end};
    i++;
    skip_newlines();
    return s;
  }
  if (at(TokenKind::KwFor)) {
    const Token start_tok = cur();
    s.kind = Stmt::Kind::For;
    i++;
    if (!at(TokenKind::Ident)) {
      diags.error({file, start_tok.line, 1, start_tok.start}, "expected loop variable after 'for'");
    } else {
      s.for_iter = std::string(cur().text);
      i++;
    }
    if (!at(TokenKind::Ident) || cur().text != "in") {
      diags.error({file, start_tok.line, 1, start_tok.start}, "expected 'in' in for loop");
    } else {
      i++;
    }
    if (at(TokenKind::IntLit)) {
      s.for_start = cur().int_value;
      i++;
    }
    if (at(TokenKind::DotDotLt)) {
      i++;
    } else {
      diags.error({file, start_tok.line, 1, start_tok.start}, "for loop requires '..<' range");
    }
    if (at(TokenKind::IntLit)) {
      s.for_end = cur().int_value;
      i++;
    }
    if (at(TokenKind::Colon)) {
      i++;
    }
    skip_newlines();
    s.for_body = parse_block();
    s.span = {start_tok.start, cur().start};
    return s;
  }
  if (at(TokenKind::KwWhile)) {
    const Token t = cur();
    s.kind = Stmt::Kind::While;
    s.span = {t.start, t.end};
    i++;
    s.cond = parse_expr();
    if (at(TokenKind::Colon)) {
      i++;
    }
    skip_newlines();
    s.while_body = parse_block();
    return s;
  }
  if (at(TokenKind::Ident) && cur().text == "parallel") {
    const Token start_tok = cur();
    i++;
    if (!consume_for_kw()) {
      diags.error({file, start_tok.line, 1, start_tok.start},
                  "expected 'for' after 'parallel'");
    }
    s.kind = Stmt::Kind::ParallelFor;
    if (!at(TokenKind::Ident)) {
      diags.error({file, start_tok.line, 1, start_tok.start},
                  "expected loop variable");
    } else {
      s.par_iter = std::string(cur().text);
      i++;
    }
    if (!at(TokenKind::Ident) || cur().text != "in") {
      diags.error({file, start_tok.line, 1, start_tok.start},
                  "expected 'in' in parallel for");
    } else {
      i++;
    }
    if (at(TokenKind::IntLit)) {
      s.par_start = cur().int_value;
      i++;
    }
    if (at(TokenKind::DotDotLt)) {
      i++;
    } else {
      diags.error({file, start_tok.line, 1, start_tok.start},
                  "parallel for requires '..<' range");
    }
    if (at(TokenKind::IntLit)) {
      s.par_end = cur().int_value;
      i++;
    }
    skip_newlines();
    if (accept(TokenKind::Indent)) {
      skip_newlines();
      while (at(TokenKind::KwRequires) || at(TokenKind::KwEnsures) ||
             at(TokenKind::KwProbEnsures) || at(TokenKind::KwDecreases) ||
             at(TokenKind::KwInvariant)) {
        s.par_contracts.push_back(parse_contract());
      }
      expect(TokenKind::Dedent, "dedent");
      skip_newlines();
    }
    if (accept(TokenKind::Eq)) {
      skip_newlines();
      if (at(TokenKind::Indent)) {
        s.par_body = parse_block();
      }
    }
    s.span = {start_tok.start, cur().start};
    return s;
  }
  if (at(TokenKind::KwWith)) {
    diags.error(loc(cur()), "with/context-manager statements are not supported; use explicit cleanup");
    i++;
    return s;
  }
  if (at(TokenKind::KwReturn)) {
    const Token t = cur();
    s.kind = Stmt::Kind::Return;
    s.span = {t.start, t.end};
    i++;
    if (!at(TokenKind::Newline) && !at(TokenKind::Dedent) && !at(TokenKind::Eof)) {
      s.expr = parse_expr();
    }
    skip_newlines();
    return s;
  }
  if (at(TokenKind::KwIf)) {
    const Token t = cur();
    s.kind = Stmt::Kind::If;
    s.span = {t.start, t.end};
    i++;
    s.cond = parse_expr();
    expect(TokenKind::Colon, "':'");
    skip_newlines();
    s.then_body = parse_block();
    // `elif` chains desugar to nested `if`s; a trailing `else` closes the chain.
    // The token after parse_block() is at the same indentation as the `if`.
    Stmt* tail = &s;
    for (;;) {
      skip_newlines();
      if (at(TokenKind::KwElif)) {
        const Token et = cur();
        Stmt nested;
        nested.kind = Stmt::Kind::If;
        nested.span = {et.start, et.end};
        i++;
        nested.cond = parse_expr();
        expect(TokenKind::Colon, "':'");
        skip_newlines();
        nested.then_body = parse_block();
        tail->else_body = std::vector<Stmt>{};
        tail->else_body->push_back(std::move(nested));
        tail = &tail->else_body->back();
      } else if (at(TokenKind::KwElse)) {
        i++;
        expect(TokenKind::Colon, "':'");
        skip_newlines();
        tail->else_body = parse_block();
        break;
      } else {
        break;
      }
    }
    return s;
  }
  const std::size_t save = i;
  auto lhs = parse_primary();
  if (lhs) {
    lhs = parse_postfix(std::move(lhs));
  }
  if (lhs && accept(TokenKind::Eq)) {
    s.kind = Stmt::Kind::Assign;
    s.span = {lhs->span.start, cur().end};
    s.init = std::move(lhs);
    s.expr = parse_expr();
    skip_newlines();
    return s;
  }
  i = save;
  s.kind = Stmt::Kind::Expr;
  s.expr = parse_expr();
  skip_newlines();
  return s;
}

ProcDecl Parser::parse_proc(bool is_extern) {
  ProcDecl proc;
  proc.is_extern = is_extern;
  if (!is_extern) {
    if (at(TokenKind::KwPrivate)) {
      proc.visibility = Visibility::Private;
      i++;
    } else if (at(TokenKind::KwPublic)) {
      i++;
    }
    if (!accept_def_kw()) {
      diags.error(loc(cur()), "expected 'def'");
    }
  }
  const Token name = cur();
  proc.span = {name.start, name.end};
  proc.name = std::string(name.text);
  i++;
  proc.type_params = parse_type_params(&proc.type_param_bounds);
  expect(TokenKind::LParen, "'('");
  skip_param_layout();
  if (!at(TokenKind::RParen)) {
    do {
      proc.params.push_back(parse_param());
      skip_param_layout();
    } while (accept(TokenKind::Comma));
  }
  skip_param_layout();
  expect(TokenKind::RParen, "')'");
  auto parse_raises = [&]() {
    if (!at(TokenKind::KwRaises)) {
      return;
    }
    i++;
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected effect name after raises");
      return;
    }
    do {
      proc.raises.push_back(std::string(cur().text));
      i++;
    } while (accept(TokenKind::Comma));
    skip_newlines();
  };
  parse_raises();
  if (accept(TokenKind::Arrow)) {
    proc.ret_type = parse_type();
  }
  skip_newlines();
  parse_raises();
  while (at(TokenKind::KwRequires) || at(TokenKind::KwEnsures) ||
         at(TokenKind::KwProbEnsures) || at(TokenKind::KwDecreases) ||
         at(TokenKind::KwInvariant)) {
    proc.contracts.push_back(parse_contract());
  }
  if (is_extern) {
    skip_newlines();
    return proc;
  }
  expect(TokenKind::Eq, "'='");
  skip_newlines();
  proc.body = parse_block();
  return proc;
}

ErrorDecl Parser::parse_error_decl() {
  ErrorDecl err;
  const Token kw = cur();
  err.span = {kw.start, kw.end};
  i++;
  if (!at(TokenKind::Ident)) {
    diags.error(loc(cur()), "expected error type name after 'error'");
    return err;
  }
  err.name = std::string(cur().text);
  i++;
  expect(TokenKind::Colon, "':'");
  if (!at(TokenKind::StringLit)) {
    diags.error(loc(cur()), "expected string message template after error name");
    return err;
  }
  err.message_template = std::string(cur().text);
  err.span.end = cur().end;
  i++;
  return err;
}

TypeAlias Parser::parse_type_alias() {
  TypeAlias alias;
  expect(TokenKind::KwType, "'type'");
  const Token name = cur();
  alias.span = {name.start, name.end};
  alias.name = std::string(name.text);
  i++;
  alias.type_params = parse_type_params();
  expect(TokenKind::Eq, "'='");
  skip_newlines();
  if (at(TokenKind::Ident) && cur().text == "typedict") {
    alias.alias_kind = AliasKind::TypedDict;
    i++;
    skip_newlines();
    alias.fields = parse_type_fields();
    skip_newlines();
    return alias;
  }
  if (at(TokenKind::KwEnum)) {
    alias.alias_kind = AliasKind::Enum;
    i++;
    skip_newlines();
    while (at(TokenKind::Ident) && cur().text != "proc" && cur().text != "def" &&
           cur().text != "type" && cur().text != "import") {
      alias.enum_variants.push_back(std::string(cur().text));
      i++;
      skip_newlines();
    }
    return alias;
  }
  if (at(TokenKind::Ident) && cur().text == "trait") {
    alias.alias_kind = AliasKind::Trait;
    i++;
    skip_newlines();
    alias.trait_methods = parse_trait_methods();
    skip_newlines();
    return alias;
  }
  if (at(TokenKind::KwObject)) {
    alias.alias_kind = AliasKind::Object;
    i++;
    skip_newlines();
    if (at(TokenKind::Ident) && cur().text == "of") {
      i++;
      if (!at(TokenKind::Ident)) {
        diags.error(loc(cur()), "expected base type name after 'object of'");
      } else {
        alias.base_object = std::string(cur().text);
        i++;
      }
      skip_newlines();
    }
    alias.fields = parse_type_fields();
    skip_newlines();
    return alias;
  }
  alias.definition = parse_type();
  skip_newlines();
  return alias;
}

ImportDecl Parser::parse_import() {
  ImportDecl imp;
  const Token start = cur();
  expect(TokenKind::KwImport, "'import'");
  if (!at(TokenKind::Ident)) {
    diags.error(loc(cur()), "expected module name after import");
    return imp;
  }
  imp.module = std::string(cur().text);
  i++;
  while (accept(TokenKind::Dot)) {
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected identifier after '.' in module path");
      break;
    }
    imp.module.push_back('.');
    imp.module.append(cur().text);
    i++;
  }
  if (at(TokenKind::Ident) && cur().text == "as") {
    i++;
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected alias after 'as'");
    } else {
      imp.alias = std::string(cur().text);
      i++;
    }
  } else {
    imp.alias = imp.module;
  }
  imp.span = {start.start, tokens[i > 0 ? i - 1 : i].end};
  return imp;
}

TheoremDecl Parser::parse_theorem(bool is_axiom, bool is_lemma) {
  TheoremDecl thm;
  thm.is_axiom = is_axiom;
  thm.is_lemma = is_lemma;
  const Token name = cur();
  thm.span = {name.start, name.end};
  thm.name = std::string(name.text);
  i++;
  if (at(TokenKind::LParen)) {
    i++;
    skip_param_layout();
    if (!at(TokenKind::RParen)) {
      do {
        thm.params.push_back(parse_param());
        skip_param_layout();
      } while (accept(TokenKind::Comma));
    }
    skip_param_layout();
    expect(TokenKind::RParen, "')'");
  }
  skip_newlines();
  expect(TokenKind::Colon, "':'");
  thm.proposition = parse_contract_expr();
  skip_newlines();
  return thm;
}

ProcDecl Parser::parse_trait_method() {
  ProcDecl proc;
  if (at(TokenKind::KwPrivate)) {
    proc.visibility = Visibility::Private;
    i++;
  } else if (at(TokenKind::KwPublic)) {
    i++;
  }
  if (!accept_def_kw()) {
    diags.error(loc(cur()), "expected 'def' in trait method");
    return proc;
  }
  const Token name = cur();
  proc.span = {name.start, name.end};
  proc.name = std::string(name.text);
  i++;
  expect(TokenKind::LParen, "'('");
  skip_param_layout();
  if (!at(TokenKind::RParen)) {
    do {
      proc.params.push_back(parse_param());
      skip_param_layout();
    } while (accept(TokenKind::Comma));
  }
  skip_param_layout();
  expect(TokenKind::RParen, "')'");
  if (accept(TokenKind::Arrow)) {
    proc.ret_type = parse_type();
  }
  skip_newlines();
  while (at(TokenKind::KwRequires) || at(TokenKind::KwEnsures) ||
         at(TokenKind::KwProbEnsures) || at(TokenKind::KwDecreases) ||
         at(TokenKind::KwInvariant)) {
    proc.contracts.push_back(parse_contract());
  }
  skip_newlines();
  return proc;
}

std::vector<ProcDecl> Parser::parse_trait_methods() {
  std::vector<ProcDecl> methods;
  skip_newlines();
  while (at(TokenKind::KwPrivate) || at(TokenKind::KwPublic) || at_fn_kw()) {
    methods.push_back(parse_trait_method());
    skip_newlines();
  }
  return methods;
}

std::vector<TypeField> Parser::parse_type_fields() {
  std::vector<TypeField> fields;
  skip_newlines();
  while (at(TokenKind::KwPrivate) || at(TokenKind::KwPublic) ||
         (at(TokenKind::Ident) && peek(1).kind == TokenKind::Colon)) {
    if ((at(TokenKind::KwPrivate) || at(TokenKind::KwPublic)) &&
        (peek(1).kind == TokenKind::KwDef || at_fn_kw())) {
      break;
    }
    TypeField field;
    field.visibility = Visibility::Public;
    if (at(TokenKind::KwPrivate)) {
      field.visibility = Visibility::Private;
      i++;
    } else if (at(TokenKind::KwPublic)) {
      i++;
    }
    if (!at(TokenKind::Ident)) {
      diags.error(loc(cur()), "expected field name");
      break;
    }
    field.name = std::string(cur().text);
    i++;
    expect(TokenKind::Colon, "':'");
    TypeExpr parsed = parse_type();
    if (parsed.kind == TypeKind::TypeApp && parsed.name == "NotRequired" &&
        !parsed.type_args.empty()) {
      field.optional = true;
      field.type = std::move(parsed.type_args[0]);
    } else {
      field.type = std::make_unique<TypeExpr>(std::move(parsed));
    }
    fields.push_back(std::move(field));
    skip_newlines();
  }
  return fields;
}

}  // namespace

ParseResult parse_module(std::string source, std::string file) {
  ParseResult result;
  Lexer lexer(std::move(source), file);
  if (!lexer.tokenize(result.diagnostics)) {
    return result;
  }
  Parser p{lexer.tokens(), file, result.diagnostics};
  Module m;
  if (p.parse_module(m)) {
    result.module = std::move(m);
  }
  return result;
}

}  // namespace li
