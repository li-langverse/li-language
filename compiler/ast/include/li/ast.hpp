#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace li {

struct Span {
  std::size_t start = 0;
  std::size_t end = 0;
};

struct Expr;
struct TypeExpr;

enum class Visibility { Public, Private };

struct TypeField {
  std::string name;
  std::unique_ptr<TypeExpr> type;
  bool optional = false;
  Visibility visibility = Visibility::Public;
};

enum class TypeKind { Named, Array, Refinement, TypeApp, Callable, GenericParam, NamedTuple };

struct TypeExpr {
  TypeKind kind = TypeKind::Named;
  Span span;
  std::string name;
  bool is_var = false;
  std::int64_t array_size = 0;
  std::unique_ptr<TypeExpr> elem;
  std::string refinement_var;
  std::unique_ptr<TypeExpr> refinement_base;
  std::unique_ptr<Expr> refinement_pred;
  std::vector<std::unique_ptr<TypeExpr>> type_args;
  std::unique_ptr<TypeExpr> callable_ret;
  std::vector<TypeField> named_fields;
  bool tuple_variadic = false;
};

enum class AliasKind { Type, TypedDict, Enum, Object, Trait };

struct Param {
  Span span;
  std::string name;
  TypeExpr type;
};

enum class ContractKind { Requires, Ensures, Decreases, Invariant, ProbEnsures };

enum class BinOp {
  Add, Sub, Mul, Div, Mod, FloorDiv, Pow, MatMul, Le, Lt, Ge, Gt, Eq, Ne, And, Or, Implies
};

struct Expr {
  enum class Kind {
    IntLit,
    FloatLit,
    BinaryLit,
    StringLit,
    Ident,
    BinOp,
    Call,
    UnaryNot,
    Index,
    FieldAccess,
    MethodCall,
    Await,
  };
  Kind kind = Kind::IntLit;
  Span span;
  std::int64_t int_value = 0;
  double float_value = 0.0;
  std::string ident;
  std::string str_value;
  /// Literal suffix (`f32`, `i32`, `u`, …) when present on numeric literals.
  std::string lit_suffix;
  BinOp bin_op = BinOp::Add;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  std::unique_ptr<Expr> operand;
  std::unique_ptr<Expr> base;
  std::unique_ptr<Expr> index;
  std::string field_name;
  std::vector<std::unique_ptr<Expr>> args;
};

struct ImportDecl {
  Span span;
  std::string module;
  std::string alias;
};

struct Contract {
  ContractKind kind;
  Span span;
  std::unique_ptr<Expr> expr;
  /// Monte Carlo hypothesis for `prob_ensures` (e.g. OsRngUniform).
  std::string prob_given;
  /// MC trial count; 0 = use `lic build --prob-check` default.
  std::int64_t prob_samples = 0;
};

struct DecoratorArg {
  std::string name;
  std::unique_ptr<Expr> value;
};

struct Decorator {
  Span span;
  std::string name;
  std::vector<DecoratorArg> args;
};

struct Stmt {
  enum class Kind {
    Return,
    If,
    While,
    For,
    ParallelFor,
    Break,
    Continue,
    Expr,
    VarDecl,
    Borrow,
    Assign,
  };
  Kind kind = Kind::Return;
  Span span;
  std::unique_ptr<Expr> expr;
  std::unique_ptr<Expr> cond;
  std::vector<Stmt> then_body;
  std::optional<std::vector<Stmt>> else_body;
  std::vector<Stmt> while_body;
  std::string var_name;
  TypeExpr var_type;
  std::unique_ptr<Expr> init;
  bool borrow_mut = false;
  // for i in start..<end  (serial)
  std::string for_iter;
  std::int64_t for_start = 0;
  std::int64_t for_end = 0;
  std::vector<Contract> for_contracts;
  std::vector<Stmt> for_body;
  // parallel for i in start..<end
  std::string par_iter;
  std::int64_t par_start = 0;
  std::int64_t par_end = 0;
  std::vector<Contract> par_contracts;
  std::vector<Stmt> par_body;
  std::vector<Decorator> decorators;
};

struct ErrorDecl {
  Span span;
  std::string name;
  std::string message_template;
};

struct ProcDecl {
  Span span;
  std::string name;
  Visibility visibility = Visibility::Public;
  bool is_extern = false;
  bool is_async = false;
  std::vector<Decorator> decorators;
  std::vector<std::string> type_params;
  /// Parallel to `type_params` — trait name after `:` (e.g. `def f[T: Hash]`).
  std::vector<std::string> type_param_bounds;
  std::vector<Param> params;
  std::optional<TypeExpr> ret_type;
  std::vector<std::string> raises;
  std::vector<Contract> contracts;
  std::vector<Stmt> body;
};

struct TypeAlias {
  Span span;
  std::string name;
  std::vector<std::string> type_params;
  AliasKind alias_kind = AliasKind::Type;
  /// For `type Derived = object of Base` — nominal supertype (static subtyping only).
  std::string base_object;
  TypeExpr definition;
  std::vector<TypeField> fields;
  std::vector<std::string> enum_variants;
  /// Required method signatures for `type Name = trait` (bodies empty).
  std::vector<ProcDecl> trait_methods;
};

/// A math axiom or theorem declared in Li source (proof-db layer).
///
/// `axiom`   — trusted declaration, no proof (tiny trusted base).
/// `theorem`/`lemma` — a proposition that lic must discharge (Layer 2+);
/// the proposition is a bool expression over `params`; `->` is implication.
struct TheoremDecl {
  Span span;
  std::string name;
  bool is_axiom = false;
  bool is_lemma = false;
  std::vector<Param> params;
  std::unique_ptr<Expr> proposition;
};

struct Module {
  std::vector<ImportDecl> imports;
  std::vector<TypeAlias> types;
  std::vector<ErrorDecl> errors;
  std::vector<ProcDecl> procs;
  std::vector<TheoremDecl> theorems;
};

std::string debug_expr(const Expr& e, int indent = 0);
std::string debug_module(const Module& m);

}  // namespace li
