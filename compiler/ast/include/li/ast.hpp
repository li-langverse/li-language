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

struct TypeField {
  std::string name;
  std::unique_ptr<TypeExpr> type;
  bool optional = false;
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

enum class AliasKind { Type, TypedDict, Enum };

struct Param {
  Span span;
  std::string name;
  TypeExpr type;
};

enum class ContractKind { Requires, Ensures, Decreases, Invariant };

enum class BinOp { Add, Sub, Mul, Div, Le, Lt, Ge, Gt, Eq, Ne, And, Or };

struct Expr {
  enum class Kind { IntLit, FloatLit, StringLit, Ident, BinOp, Call, UnaryNot, Index };
  Kind kind = Kind::IntLit;
  Span span;
  std::int64_t int_value = 0;
  double float_value = 0.0;
  std::string ident;
  std::string str_value;
  BinOp bin_op = BinOp::Add;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  std::unique_ptr<Expr> operand;
  std::unique_ptr<Expr> base;
  std::unique_ptr<Expr> index;
  std::vector<std::unique_ptr<Expr>> args;
};

struct Contract {
  ContractKind kind;
  Span span;
  std::unique_ptr<Expr> expr;
};

struct Stmt {
  enum class Kind { Return, If, While, Expr, VarDecl, Borrow, Assign };
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
};

struct ProcDecl {
  Span span;
  std::string name;
  bool is_extern = false;
  std::vector<std::string> type_params;
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
  TypeExpr definition;
  std::vector<TypeField> fields;
  std::vector<std::string> enum_variants;
};

struct Module {
  std::vector<TypeAlias> types;
  std::vector<ProcDecl> procs;
};

std::string debug_expr(const Expr& e, int indent = 0);
std::string debug_module(const Module& m);

}  // namespace li
