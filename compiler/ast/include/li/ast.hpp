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
struct Contract;
struct Decorator;

struct TypeField {
  std::string name;
  std::unique_ptr<TypeExpr> type;
  bool optional = false;
  bool public_field = true;
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

enum class AliasKind { Type, TypedDict, Enum, Object };

struct Param {
  Span span;
  std::string name;
  TypeExpr type;
};

enum class ContractKind { Requires, Ensures, Decreases, Invariant };

// Ordering is part of the MIR dump ABI: the self-hosted walker emits these
// exact binop codes (Le=8, Ge=10, And=14, ...), so Pow/MatMul/Implies must
// stay in position between FloorDiv and Le.
enum class BinOp {
  Add, Sub, Mul, Div, Mod, FloorDiv, Pow, MatMul, Le, Lt, Ge, Gt, Eq, Ne, And,
  Or, Implies
};

struct Expr {
  enum class Kind { IntLit, FloatLit, StringLit, Ident, BinOp, Call, UnaryNot, UnaryMinus, Index, Await, Field, MethodCall };
  Kind kind = Kind::IntLit;
  Span span;
  std::int64_t int_value = 0;
  double float_value = 0.0;
  bool is_binary = false;
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
  // `prob_ensures` tail (our lexer folds it into KwInvariant, so text must
  // disambiguate). The walker dumps given/samples on the code-7 line for
  // these contracts and drops the tail for plain invariants.
  bool is_prob = false;
  std::string prob_given;      // last `given <dist>` distribution name
  std::int64_t prob_samples = 0;
};

/// One `def name(params) [: ret] <contracts>` inside `type X = trait`. The
/// walker dumps these as FN lines (code 4) with no body; retention mirrors
/// parse_trait_method's dump exactly.
struct TraitMethod {
  Span span;
  int visibility = 0;  // 1 = `public`; the walker's kind-16 prefix
  std::string name;
  std::vector<Param> params;
  std::optional<TypeExpr> ret_type;
  std::vector<Contract> contracts;
};

// axiom/theorem/lemma declaration: `theorem name(params) : <proposition>`.
// No MIR is emitted for these (the walker's parse_theorem also only dumps
// them), so retention is AST-level only.
struct TheoremDecl {
  Span span;
  std::string name;
  std::vector<Param> params;
  bool is_axiom = false;
  bool is_lemma = false;
  std::unique_ptr<Expr> statement;
};

struct Stmt {
  enum class Kind { Return, If, While, Expr, VarDecl, Borrow, Assign, Break, Continue, ParallelFor, For };
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
  // ParallelFor: `parallel for <par_index> in <par_start>..<<par_end>` with
  // the body in par_body (walker lowers it into a synthetic __li_par_* fn).
  std::string par_index;
  std::int64_t par_start = 0;
  std::int64_t par_end = 0;
  std::vector<Stmt> par_body;
  // For: `for <for_index> in <for_start>..<<for_end>` with the body in
  // for_body. `for_vectorized`/`for_lanes` come from a statement-level
  // `@vectorized(lanes=N)` decorator directly above the for (walker gates
  // ArraySimdScope INS 49 enter/exit on ftmp 4095).
  std::string for_index;
  std::int64_t for_start = 0;
  std::int64_t for_end = 0;
  std::vector<Stmt> for_body;
  bool for_vectorized = false;
  std::int64_t for_lanes = 0;
  // ParallelFor carries its own `requires/ensures/decreases/invariant`
  // contracts (walker parse_parallel_for parses them into the code-7 stream
  // between the 44 line and the body block). Statement-level decorators
  // (`@parallel(...)`, `@vectorized(...)`) arrive on the while/for/parallel
  // variants the walker's parse_stmt decorator branch accepts.
  std::vector<struct Contract> contracts;
  std::vector<Decorator> decorators;
};

// One `arg [= value]` inside a `@name(args)` decorator. value_kind is the
// walker's emission form: 0 bare (the arg name re-emitted as an ident),
// 1 ident/bool text, 2 int literal, 3 float literal, 4 binary literal,
// 5 string literal; value_text keeps the raw lexeme for the dump.
struct DecArg {
  std::string name;
  int value_kind = 0;
  std::string value_text;
};

// One `@name` / `@name(args)` line in a decorator list attached to a def.
struct Decorator {
  std::string name;
  std::vector<DecArg> args;
  std::int64_t lanes = 0;
  bool vectorized = false;
  bool no_vectorize = false;
  bool parallel = false;
  bool disjoint = false;
  bool cpu = false;
  bool is_async = false;
};

struct ProcDecl {
  Span span;
  std::string name;
  bool is_extern = false;
  std::vector<std::string> type_params;
  /** Parallel to type_params: the `T: Bound` bound ident, empty when
   *  unconstrained. The walker emits `5 <name> <bound>` for procs. */
  std::vector<std::string> type_param_bounds;
  std::vector<Param> params;
  std::optional<TypeExpr> ret_type;
  std::vector<std::string> raises;
  std::vector<Contract> contracts;
  std::vector<Stmt> body;
  std::vector<Decorator> decorators;
  /** `async def` keyword (bare form): sets the FN async bit but, unlike the
   *  @async decorator, emits no DEC line. */
  bool is_async = false;
  /** `private def` — the proc is invisible to importing modules (the walker
   *  records it with ep_priv and excludes private procs from import scope). */
  bool is_private = false;
};

struct TypeAlias {
  Span span;
  std::string name;
  std::vector<std::string> type_params;
  AliasKind alias_kind = AliasKind::Type;
  TypeExpr definition;
  std::vector<TypeField> fields;
  std::vector<std::string> enum_variants;
  /** `object of Base` parent object name (empty for plain `object`). */
  std::string base;
  /** `type X = trait` — kind 4 in the walker's alias dump. Kept as a flag
   *  (not an AliasKind) so the typecheck alias space treats the trait name
   *  exactly as it did before retention landed. */
  bool is_trait = false;
  std::vector<TraitMethod> trait_methods;
};

struct ImportDecl {
  Span span;
  std::string module;
  std::string alias;
  bool has_alias = false;
};

struct Module {
  std::vector<TypeAlias> types;
  std::vector<ProcDecl> procs;
  std::vector<ImportDecl> imports;
  /// axiom/theorem/lemma declarations retained for the AST dump (no MIR).
  std::vector<TheoremDecl> theorems;
};

std::string debug_expr(const Expr& e, int indent = 0);
std::string debug_module(const Module& m);

}  // namespace li
