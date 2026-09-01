#include "li/typecheck.hpp"

#include "li/borrowck.hpp"
#include "li/proof_cli.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace li {
namespace {

enum class TyKind {
  Int, Int64, Ptr, Float, Bool, Str, Array, Simd, List, Dict, Tuple, TypedDict, Enum,
  Named, TypeVar, Protocol, Callable
};

struct Ty;

using TyPtr = std::shared_ptr<Ty>;

struct Ty {
  TyKind kind = TyKind::Int;
  std::int64_t array_size = 0;
  std::int64_t simd_lanes = 0;
  bool bounded_index = false;
  bool requires_bounded_index = false;
  std::shared_ptr<Ty> elem;
  std::string name;
  std::vector<std::shared_ptr<Ty>> type_args;
  std::shared_ptr<Ty> callable_ret;
  std::vector<std::pair<std::string, TyPtr>> fields;
  bool tuple_variadic = false;
  std::vector<std::string> enum_variants;
};

TyPtr make_int() { return std::make_shared<Ty>(Ty{TyKind::Int}); }
TyPtr make_float() { return std::make_shared<Ty>(Ty{TyKind::Float}); }
TyPtr make_bool() { return std::make_shared<Ty>(Ty{TyKind::Bool}); }
TyPtr make_str() { return std::make_shared<Ty>(Ty{TyKind::Str}); }
TyPtr make_i64() { return std::make_shared<Ty>(Ty{TyKind::Int64}); }
TyPtr make_ptr() { return std::make_shared<Ty>(Ty{TyKind::Ptr}); }

TyPtr make_simd(std::int64_t lanes) {
  auto t = std::make_shared<Ty>();
  t->kind = TyKind::Simd;
  t->name = "simd";
  t->simd_lanes = lanes;
  return t;
}

TyPtr make_type_var(std::string name) {
  auto t = std::make_shared<Ty>();
  t->kind = TyKind::TypeVar;
  t->name = std::move(name);
  return t;
}

TyPtr make_protocol(std::string name) {
  auto t = std::make_shared<Ty>();
  t->kind = TyKind::Protocol;
  t->name = std::move(name);
  return t;
}

bool is_true_literal(const Expr& e) {
  return e.kind == Expr::Kind::Ident && e.ident == "true";
}

bool is_false_literal(const Expr& e) {
  return e.kind == Expr::Kind::Ident && e.ident == "false";
}

bool is_unit_type(const TypeExpr& type) {
  return type.kind == TypeKind::Named && type.name == "unit";
}

const Expr* direct_return_expr(const ProcDecl& proc) {
  const Expr* result = nullptr;
  for (const auto& stmt : proc.body) {
    if (stmt.kind != Stmt::Kind::Return || !stmt.expr) {
      continue;
    }
    if (result != nullptr) {
      return nullptr;
    }
    result = stmt.expr.get();
  }
  return result;
}

std::optional<long double> literal_number(const Expr& e) {
  if (e.kind == Expr::Kind::IntLit) {
    return static_cast<long double>(e.int_value);
  }
  if (e.kind == Expr::Kind::FloatLit) {
    return static_cast<long double>(e.float_value);
  }
  return std::nullopt;
}

std::optional<bool> compare_literal_numbers(BinOp op, long double left, long double right) {
  switch (op) {
    case BinOp::Eq: return left == right;
    case BinOp::Ne: return left != right;
    case BinOp::Lt: return left < right;
    case BinOp::Le: return left <= right;
    case BinOp::Gt: return left > right;
    case BinOp::Ge: return left >= right;
    default: return std::nullopt;
  }
}

bool literal_comparison_is_false(const Expr& comparison, const Expr& returned) {
  if (comparison.kind != Expr::Kind::BinOp || !comparison.lhs || !comparison.rhs) {
    return false;
  }
  const Expr* lhs = comparison.lhs.get();
  const Expr* rhs = comparison.rhs.get();
  if (lhs->kind == Expr::Kind::Ident && lhs->ident == "result") {
    lhs = &returned;
  } else if (rhs->kind == Expr::Kind::Ident && rhs->ident == "result") {
    rhs = &returned;
  } else {
    return false;
  }
  const auto left = literal_number(*lhs);
  const auto right = literal_number(*rhs);
  if (!left || !right) {
    return false;
  }
  const auto holds = compare_literal_numbers(comparison.bin_op, *left, *right);
  return holds.has_value() && !*holds;
}

bool postcondition_is_false_for_literal_return(const ProcDecl& proc, const Contract& contract) {
  if (contract.kind != ContractKind::Ensures || !contract.expr) {
    return false;
  }
  const Expr* returned = direct_return_expr(proc);
  return returned != nullptr && literal_comparison_is_false(*contract.expr, *returned);
}

bool literal_requires_is_false(const ProcDecl& callee, const Expr& call) {
  for (const auto& contract : callee.contracts) {
    if (contract.kind != ContractKind::Requires || !contract.expr) {
      continue;
    }
    if (contract.expr->kind == Expr::Kind::Ident && contract.expr->ident == "false") {
      return true;
    }
    if (contract.expr->kind != Expr::Kind::BinOp || !contract.expr->lhs || !contract.expr->rhs) {
      continue;
    }
    const Expr* lhs = contract.expr->lhs.get();
    const Expr* rhs = contract.expr->rhs.get();
    const Expr* arg = nullptr;
    const Expr* literal = nullptr;
    bool parameter_on_left = false;
    for (std::size_t i = 0; i < callee.params.size() && i < call.args.size(); ++i) {
      if (lhs->kind == Expr::Kind::Ident && callee.params[i].name == lhs->ident) {
        arg = call.args[i].get();
        literal = rhs;
        parameter_on_left = true;
        break;
      }
      if (rhs->kind == Expr::Kind::Ident && callee.params[i].name == rhs->ident) {
        arg = call.args[i].get();
        literal = lhs;
        break;
      }
    }
    if (!arg || !literal || arg->kind != Expr::Kind::IntLit || literal->kind != Expr::Kind::IntLit) {
      continue;
    }
    const std::int64_t left = parameter_on_left ? arg->int_value : literal->int_value;
    const std::int64_t right = parameter_on_left ? literal->int_value : arg->int_value;
    const auto holds = compare_literal_numbers(contract.expr->bin_op, left, right);
    if (holds.has_value() && !*holds) {
      return true;
    }
  }
  return false;
}

struct AliasEntry {
  AliasKind alias_kind = AliasKind::Type;
  std::vector<std::string> type_params;
  const TypeExpr* definition = nullptr;
  const std::vector<TypeField>* fields = nullptr;
  const std::vector<std::string>* enum_variants = nullptr;
  bool is_protocol = false;
};

struct Ctx {
  std::map<std::string, AliasEntry> aliases;
  std::map<std::string, const ProcDecl*> procs;
  std::map<std::string, TyPtr> locals;
  std::map<std::string, TyPtr> type_vars;
  DiagnosticBag& diags;
  std::string file;

  SourceLoc loc(const Span& s) const { return SourceLoc{file, 1, 1, s.start}; }

  std::unique_ptr<TypeExpr> clone_type(const TypeExpr& te) const {
    auto out = std::make_unique<TypeExpr>();
    out->kind = te.kind;
    out->span = te.span;
    out->name = te.name;
    out->array_size = te.array_size;
    out->refinement_var = te.refinement_var;
    if (te.elem) {
      out->elem = clone_type(*te.elem);
    }
    if (te.refinement_base) {
      out->refinement_base = clone_type(*te.refinement_base);
    }
    if (te.refinement_pred) {
      out->refinement_pred = nullptr;
    }
    if (te.callable_ret) {
      out->callable_ret = clone_type(*te.callable_ret);
    }
    for (const auto& arg : te.type_args) {
      out->type_args.push_back(clone_type(*arg));
    }
    out->tuple_variadic = te.tuple_variadic;
    return out;
  }

  std::unique_ptr<TypeExpr> substitute(const TypeExpr& te,
                                       const std::map<std::string, const TypeExpr*>& subst) const {
    if (te.kind == TypeKind::Named) {
      const auto it = subst.find(te.name);
      if (it != subst.end()) {
        return clone_type(*it->second);
      }
    }
    auto out = clone_type(te);
    if (out->elem) {
      out->elem = substitute(*out->elem, subst);
    }
    if (out->refinement_base) {
      out->refinement_base = substitute(*out->refinement_base, subst);
    }
    if (out->callable_ret) {
      out->callable_ret = substitute(*out->callable_ret, subst);
    }
    for (auto& arg : out->type_args) {
      arg = substitute(*arg, subst);
    }
    return out;
  }

  bool same_kind(const TyPtr& a, const TyPtr& b) const {
    if (a->kind != b->kind) {
      return false;
    }
    if (a->kind == TyKind::Array) {
      return a->array_size == b->array_size && same_kind(a->elem, b->elem);
    }
    if (a->kind == TyKind::Simd) {
      return a->simd_lanes == b->simd_lanes;
    }
    if (a->kind == TyKind::List || a->kind == TyKind::Dict || a->kind == TyKind::Tuple) {
      if (a->tuple_variadic != b->tuple_variadic) {
        return false;
      }
      if (a->type_args.size() != b->type_args.size()) {
        return false;
      }
      for (std::size_t n = 0; n < a->type_args.size(); ++n) {
        if (!same_kind(a->type_args[n], b->type_args[n])) {
          return false;
        }
      }
      return a->name == b->name;
    }
    if (a->kind == TyKind::TypedDict) {
      if (a->fields.size() != b->fields.size()) {
        return false;
      }
      for (std::size_t n = 0; n < a->fields.size(); ++n) {
        if (a->fields[n].first != b->fields[n].first ||
            !same_kind(a->fields[n].second, b->fields[n].second)) {
          return false;
        }
      }
      return a->name == b->name;
    }
    if (a->kind == TyKind::Enum) {
      return a->name == b->name && a->enum_variants == b->enum_variants;
    }
    if (a->kind == TyKind::TypeVar || a->kind == TyKind::Named || a->kind == TyKind::Protocol) {
      return a->name == b->name;
    }
    return true;
  }

  TyPtr resolve_builtin_collection(const TypeExpr& te) {
    auto t = std::make_shared<Ty>();
    if (te.name == "list") {
      if (te.type_args.size() != 1) {
        diags.error(loc(te.span), "list requires exactly 1 type argument");
        return make_int();
      }
      t->kind = TyKind::List;
      t->name = "list";
      t->type_args.push_back(resolve_type_expr(*te.type_args[0]));
      return t;
    }
    if (te.name == "dict") {
      if (te.type_args.size() != 2) {
        diags.error(loc(te.span), "dict requires exactly 2 type arguments");
        return make_int();
      }
      t->kind = TyKind::Dict;
      t->name = "dict";
      t->type_args.push_back(resolve_type_expr(*te.type_args[0]));
      t->type_args.push_back(resolve_type_expr(*te.type_args[1]));
      return t;
    }
    if (te.name == "tuple") {
      t->kind = TyKind::Tuple;
      t->name = "tuple";
      t->tuple_variadic = te.tuple_variadic;
      for (const auto& arg : te.type_args) {
        t->type_args.push_back(resolve_type_expr(*arg));
      }
      if (t->type_args.empty() && !te.tuple_variadic) {
        diags.error(loc(te.span), "tuple requires at least 1 type argument");
      }
      return t;
    }
    diags.error(loc(te.span), "unknown collection type '" + te.name + "'");
    return make_int();
  }

  TyPtr resolve_typedict(const std::string& name, const std::vector<TypeField>& fields,
                         const Span& span) {
    auto t = std::make_shared<Ty>();
    t->kind = TyKind::TypedDict;
    t->name = name;
    for (const auto& field : fields) {
      if (!field.type) {
        continue;
      }
      t->fields.emplace_back(field.name, resolve_type_expr(*field.type));
    }
    (void)span;
    return t;
  }

  TyPtr resolve_enum(const std::string& name, const std::vector<std::string>& variants,
                     const Span& span) {
    auto t = std::make_shared<Ty>();
    t->kind = TyKind::Enum;
    t->name = name;
    t->enum_variants = variants;
    (void)span;
    return t;
  }

  bool satisfies_protocol(const TyPtr& value, const TyPtr& protocol) const {
    if (protocol->kind != TyKind::Protocol) {
      return same_kind(value, protocol);
    }
    if (protocol->name == "Sized") {
      return value->kind == TyKind::Array;
    }
    return false;
  }

  bool assignable(const TyPtr& value, const TyPtr& expected) const {
    if (expected->kind == TyKind::TypeVar) {
      return true;
    }
    if (expected->kind == TyKind::Protocol) {
      return satisfies_protocol(value, expected);
    }
    if (value->kind == TyKind::TypeVar) {
      return expected->kind == TyKind::TypeVar && value->name == expected->name;
    }
    // String literals coerce to `ptr` (they are stored as char pointers at the
    // ABI level), and `ptr` and `int64` are ABI-interchangeable in Li.
    if (value->kind == TyKind::Str && expected->kind == TyKind::Ptr) {
      return true;
    }
    if ((value->kind == TyKind::Ptr && expected->kind == TyKind::Int64) ||
        (value->kind == TyKind::Int64 && expected->kind == TyKind::Ptr)) {
      return true;
    }
    return same_kind(value, expected);
  }

  static void collect_refinement_bounds(const Expr& expr, const std::string& variable,
                                        std::int64_t& upper_bound) {
    if (expr.kind == Expr::Kind::BinOp && expr.lhs && expr.rhs) {
      if (expr.bin_op == BinOp::And) {
        collect_refinement_bounds(*expr.lhs, variable, upper_bound);
        collect_refinement_bounds(*expr.rhs, variable, upper_bound);
        return;
      }
      if (expr.bin_op == BinOp::Lt && expr.lhs->kind == Expr::Kind::Ident &&
          expr.lhs->ident == variable && expr.rhs->kind == Expr::Kind::IntLit) {
        upper_bound = expr.rhs->int_value;
      }
    }
  }

  TyPtr resolve_type_expr(const TypeExpr& te) {
    if (te.kind == TypeKind::Refinement) {
      TyPtr base = resolve_type_expr(*te.refinement_base);
      std::int64_t upper_bound = -1;
      if (te.refinement_pred) {
        collect_refinement_bounds(*te.refinement_pred, te.refinement_var, upper_bound);
      }
      if (upper_bound >= 0) {
        base->bounded_index = true;
        base->array_size = upper_bound;
      }
      return base;
    }
    if (te.kind == TypeKind::Callable) {
      auto t = std::make_shared<Ty>();
      t->kind = TyKind::Callable;
      t->name = "Callable";
      for (const auto& arg : te.type_args) {
        t->type_args.push_back(resolve_type_expr(*arg));
      }
      if (te.callable_ret) {
        t->callable_ret = resolve_type_expr(*te.callable_ret);
      }
      return t;
    }
    if (te.kind == TypeKind::Array) {
      auto t = std::make_shared<Ty>();
      t->kind = TyKind::Array;
      t->array_size = te.array_size;
      t->elem = resolve_type_expr(*te.elem);
      return t;
    }
    if (te.kind == TypeKind::NamedTuple) {
      auto t = std::make_shared<Ty>();
      t->kind = TyKind::Tuple;
      t->name = "tuple";
      for (const auto& field : te.named_fields) {
        if (!field.type) {
          continue;
        }
        t->fields.emplace_back(field.name, resolve_type_expr(*field.type));
        t->type_args.push_back(resolve_type_expr(*field.type));
      }
      return t;
    }
    if (te.kind == TypeKind::TypeApp) {
      if (te.name == "simd") {
        if (te.type_args.size() != 1 || te.array_size <= 0) {
          diags.error(loc(te.span), "simd requires an element type and positive lane count");
          return make_int();
        }
        const TyPtr elem = resolve_type_expr(*te.type_args[0]);
        if (elem->kind != TyKind::Float) {
          diags.error(loc(te.span), "simd currently requires f64 elements");
          return make_int();
        }
        if (te.array_size != 4) {
          diags.error(loc(te.span), "unsupported SIMD lane count");
          return make_int();
        }
        return make_simd(te.array_size);
      }
      if (te.name == "list" || te.name == "dict" || te.name == "tuple") {
        return resolve_builtin_collection(te);
      }
      const auto it = aliases.find(te.name);
      if (it == aliases.end()) {
        diags.error(loc(te.span), "unknown type '" + te.name + "'");
        return make_int();
      }
      const AliasEntry& entry = it->second;
      if (entry.alias_kind != AliasKind::Type || entry.definition == nullptr) {
        diags.error(loc(te.span), "type '" + te.name + "' is not a generic alias");
        return make_int();
      }
      if (entry.type_params.size() != te.type_args.size()) {
        diags.error(loc(te.span), "generic arity mismatch for '" + te.name + "'");
        return make_int();
      }
      std::map<std::string, const TypeExpr*> subst;
      for (std::size_t n = 0; n < entry.type_params.size(); ++n) {
        subst[entry.type_params[n]] = te.type_args[n].get();
      }
      const std::unique_ptr<TypeExpr> expanded = substitute(*entry.definition, subst);
      return resolve_type_expr(*expanded);
    }
    if (te.kind == TypeKind::Named) {
      const auto tv = type_vars.find(te.name);
      if (tv != type_vars.end()) {
        return tv->second;
      }
      const auto it = aliases.find(te.name);
      if (it != aliases.end()) {
        if ((it->second.alias_kind == AliasKind::TypedDict ||
             it->second.alias_kind == AliasKind::Object) &&
            it->second.fields) {
          return resolve_typedict(te.name, *it->second.fields, te.span);
        }
        if (it->second.alias_kind == AliasKind::Enum && it->second.enum_variants) {
          return resolve_enum(te.name, *it->second.enum_variants, te.span);
        }
        if (it->second.is_protocol) {
          return make_protocol(te.name);
        }
        if (!it->second.type_params.empty()) {
          diags.error(loc(te.span), "generic type '" + te.name + "' requires type arguments");
          return make_int();
        }
        if (it->second.definition) {
          return resolve_type_expr(*it->second.definition);
        }
      }
      if (te.name == "int") {
        return make_int();
      }
      if (te.name == "float" || te.name == "float64" || te.name == "f64") {
        return make_float();
      }
      if (te.name == "bool") {
        return make_bool();
      }
      if (te.name == "ptr") {
        return make_ptr();
      }
      if (te.name == "int64" || te.name == "i64" || te.name == "long") {
        return make_i64();
      }
      if (te.name == "str") {
        auto t = std::make_shared<Ty>();
        t->kind = TyKind::Str;
        t->name = "str";
        return t;
      }
      if (te.name == "Any") {
        diags.error(loc(te.span), "type 'Any' is forbidden");
        return make_int();
      }
      if (te.name == "unit") {
        return make_int();
      }
      if (te.name == "Protocol") {
        return make_protocol("Protocol");
      }
      diags.error(loc(te.span), "unknown type '" + te.name + "'");
      return make_int();
    }
    return make_int();
  }

  TyPtr type_of(const Expr& e) {
    switch (e.kind) {
      case Expr::Kind::IntLit:
        return make_int();
      case Expr::Kind::FloatLit:
        return make_float();
      case Expr::Kind::StringLit:
        return make_str();
      case Expr::Kind::Ident: {
        const auto it = locals.find(e.ident);
        if (it == locals.end()) {
          diags.error(loc(e.span), "unknown variable '" + e.ident + "'");
          return make_int();
        }
        return it->second;
      }
      case Expr::Kind::BinOp: {
        const TyPtr l = type_of(*e.lhs);
        const TyPtr r = type_of(*e.rhs);
        if (e.bin_op == BinOp::Add || e.bin_op == BinOp::Sub || e.bin_op == BinOp::Mul ||
            e.bin_op == BinOp::Div || e.bin_op == BinOp::Mod || e.bin_op == BinOp::FloorDiv ||
            e.bin_op == BinOp::Pow) {
          if (l->kind == TyKind::Int && r->kind == TyKind::Int) {
            return make_int();
          }
          if (l->kind == TyKind::Float && r->kind == TyKind::Float) {
            return make_float();
          }
          // Elementwise array binop: `a + b` / `a * b` on same-shape float or
          // int arrays lowers to ArrayBinOpF64/I64; size is max for broadcast
          // (array[1] operands), matching the walker's esz = max(szl, szr).
          if (l->kind == TyKind::Array && r->kind == TyKind::Array && l->elem && r->elem &&
              l->elem->kind == r->elem->kind &&
              (l->elem->kind == TyKind::Float || l->elem->kind == TyKind::Int)) {
            auto t = std::make_shared<Ty>();
            t->kind = TyKind::Array;
            t->elem = l->elem;
            t->array_size = std::max(l->array_size, r->array_size);
            return t;
          }
          // Float scale: `2.0 * x` / `x * 2.0` on a float array -> ArrayScaleF64.
          if ((l->kind == TyKind::Float && r->kind == TyKind::Array && r->elem &&
               r->elem->kind == TyKind::Float) ||
              (r->kind == TyKind::Float && l->kind == TyKind::Array && l->elem &&
               l->elem->kind == TyKind::Float)) {
            const TyPtr arr = l->kind == TyKind::Array ? l : r;
            auto t = std::make_shared<Ty>();
            t->kind = TyKind::Array;
            t->elem = arr->elem;
            t->array_size = arr->array_size;
            return t;
          }
          diags.error(loc(e.span),
                      "cannot mix int and float in arithmetic without explicit cast");
          return make_int();
        }
        if (e.bin_op == BinOp::MatMul) {
          // `A @ B` over matrices (array[M, array[K, float]]) returns the
          // result matrix array[rows(l), array[cols(r), elem]].
          const bool l_mat = l->kind == TyKind::Array && l->elem &&
                             l->elem->kind == TyKind::Array;
          const bool r_mat = r->kind == TyKind::Array && r->elem &&
                             r->elem->kind == TyKind::Array;
          if (l_mat && r_mat) {
            auto inner = std::make_shared<Ty>();
            inner->kind = TyKind::Array;
            inner->array_size = r->elem->array_size;
            inner->elem = r->elem->elem;
            auto t = std::make_shared<Ty>();
            t->kind = TyKind::Array;
            t->array_size = l->array_size;
            t->elem = inner;
            return t;
          }
          // `x @ y` dot product over float arrays lowers to ArrayDotF64 ->
          // float. Int arrays would be an int dot.
          const bool any_float =
              l->kind == TyKind::Float || r->kind == TyKind::Float ||
              (l->kind == TyKind::Array && l->elem && l->elem->kind == TyKind::Float) ||
              (r->kind == TyKind::Array && r->elem && r->elem->kind == TyKind::Float);
          return any_float ? make_float() : make_int();
        }
        return make_bool();
      }
      case Expr::Kind::Call: {
        if (e.ident == "echo") {
          if (e.args.size() != 1) {
            diags.error(loc(e.span), "echo expects one argument");
            return make_int();
          }
          const TyPtr arg_ty = type_of(*e.args[0]);
          if (arg_ty->kind != TyKind::Int && arg_ty->kind != TyKind::Str) {
            diags.error(loc(e.span), "echo expects int or str");
          }
          return make_int();
        }
        // Builtin array reductions / kernels (self-hosted walker reference):
        // sum(a) / norm(a) return the element type, dot(a,b) returns float,
        // axpy(alpha,x,y) is a statement (int).
        if ((e.ident == "sum" || e.ident == "norm") && e.args.size() == 1) {
          const TyPtr arg_ty = type_of(*e.args[0]);
          if (arg_ty->kind == TyKind::Array && arg_ty->elem) {
            return arg_ty->elem;
          }
          return make_int();
        }
        if (e.ident == "dot" && e.args.size() == 2) {
          (void)type_of(*e.args[0]);
          (void)type_of(*e.args[1]);
          return make_float();
        }
        if (e.ident == "axpy" && e.args.size() == 3) {
          (void)type_of(*e.args[0]);
          (void)type_of(*e.args[1]);
          (void)type_of(*e.args[2]);
          return make_int();
        }
        if (e.ident == "__li_simd_splat_f64" && e.args.size() == 1) {
          (void)type_of(*e.args[0]);
          return make_simd(4);
        }
        if (e.ident == "__li_simd_mul_f64" && e.args.size() == 2) {
          const TyPtr left = type_of(*e.args[0]);
          const TyPtr right = type_of(*e.args[1]);
          if (left->kind != TyKind::Simd || right->kind != TyKind::Simd ||
              left->simd_lanes != right->simd_lanes) {
            diags.error(loc(e.span), "SIMD operands must have matching lane types");
          }
          return make_simd(4);
        }
        if (e.ident == "__li_horiz_sum_f64" && e.args.size() == 1) {
          const TyPtr arg = type_of(*e.args[0]);
          if (arg->kind != TyKind::Simd) {
            diags.error(loc(e.span), "SIMD horizontal sum requires a SIMD value");
          }
          return make_float();
        }
        const auto pit = procs.find(e.ident);
        if (pit != procs.end()) {
          const ProcDecl& callee = *pit->second;
          for (const auto& arg : e.args) {
            (void)type_of(*arg);
          }
          if (literal_requires_is_false(callee, e)) {
            diags.error(loc(e.span),
                        "E0304: call to '" + callee.name + "' violates its requires clause");
          }
          if (callee.ret_type) {
            return resolve_type_expr(*callee.ret_type);
          }
          return make_int();
        }
        for (const auto& arg : e.args) {
          (void)type_of(*arg);
        }
        return make_int();
      }
      case Expr::Kind::UnaryNot:
        return make_bool();
      case Expr::Kind::UnaryMinus:
        // Negation preserves the operand type (int or float).
        if (e.operand) {
          return type_of(*e.operand);
        }
        return make_int();
      case Expr::Kind::Index: {
        const TyPtr base = type_of(*e.base);
        const TyPtr idx = type_of(*e.index);
        if (idx->kind != TyKind::Int) {
          diags.error(loc(e.span), "array index must be int");
          return make_int();
        }
        if (base->kind != TyKind::Array) {
          diags.error(loc(e.span), "index on non-array type");
          return make_int();
        }
        if (e.index->kind == Expr::Kind::IntLit) {
          const auto i = e.index->int_value;
          if (i < 0 || i >= base->array_size) {
            diags.error(loc(e.span), "array index out of range");
          }
        } else if (e.index->kind == Expr::Kind::Ident && base->requires_bounded_index) {
          const auto index_it = locals.find(e.index->ident);
          if (index_it == locals.end() || !index_it->second->bounded_index ||
              index_it->second->array_size > base->array_size) {
            diags.error(loc(e.span), "array index must be constant or refinement-bounded");
          }
        }
        return base->elem;
      }
      case Expr::Kind::Field: {
        const TyPtr base = type_of(*e.base);
        std::string fname;
        if (e.index && e.index->kind == Expr::Kind::Ident) {
          fname = e.index->ident;
        }
        if (base->kind != TyKind::TypedDict) {
          diags.error(loc(e.span), "field access on non-object type");
          return make_int();
        }
        for (const auto& f : base->fields) {
          if (f.first == fname) {
            return f.second;
          }
        }
        diags.error(loc(e.span), "unknown field '" + fname + "'");
        return make_int();
      }
    }
    return make_int();
  }

  void check_call_args(const Expr& call) {
    if (call.kind != Expr::Kind::Call) {
      return;
    }
    const auto it = procs.find(call.ident);
    if (it == procs.end()) {
      return;
    }
    const ProcDecl& callee = *it->second;
    for (std::size_t n = 0; n < call.args.size() && n < callee.params.size(); ++n) {
      const TyPtr arg_ty = type_of(*call.args[n]);
      const TyPtr param_ty = resolve_type_expr(callee.params[n].type);
      if (!assignable(arg_ty, param_ty)) {
        if (param_ty->kind == TyKind::Protocol) {
          diags.error(loc(call.span),
                      "argument does not satisfy Protocol '" + param_ty->name + "'");
        } else {
          diags.error(loc(call.span), "argument type mismatch in call to '" + call.ident + "'");
        }
      }
    }
  }

  void check_stmt(const Stmt& s) {
    if (s.kind == Stmt::Kind::VarDecl) {
      const TyPtr declared = resolve_type_expr(s.var_type);
      if (s.init) {
        const TyPtr got = type_of(*s.init);
        if (!assignable(got, declared)) {
          if (declared->kind == TyKind::Protocol) {
            diags.error(loc(s.span),
                        "value does not satisfy Protocol '" + declared->name + "'");
          } else {
            diags.error(loc(s.span), "variable type mismatch");
          }
        }
      }
      locals[s.var_name] = declared;
      return;
    }
    if (s.kind == Stmt::Kind::Return && s.expr) {
      type_of(*s.expr);
      return;
    }
    if (s.kind == Stmt::Kind::If) {
      if (s.cond) {
        type_of(*s.cond);
      }
      for (const auto& inner : s.then_body) {
        check_stmt(inner);
      }
      if (s.else_body) {
        for (const auto& inner : *s.else_body) {
          check_stmt(inner);
        }
      }
      return;
    }
    if (s.kind == Stmt::Kind::While) {
      if (s.cond) {
        // A canonical counted-loop guard proves the loop index is bounded
        // while the body executes: `while i < N`. Keep this local inference
        // deliberately narrow; all other dynamic indices still need a
        // refinement-typed index.
        if (s.cond->kind == Expr::Kind::BinOp && s.cond->bin_op == BinOp::Lt &&
            s.cond->lhs && s.cond->rhs && s.cond->lhs->kind == Expr::Kind::Ident &&
            s.cond->rhs->kind == Expr::Kind::IntLit) {
          const auto it = locals.find(s.cond->lhs->ident);
          if (it != locals.end()) {
            it->second->bounded_index = true;
            it->second->array_size = s.cond->rhs->int_value;
          }
        }
        type_of(*s.cond);
      }
      for (const auto& inner : s.while_body) {
        check_stmt(inner);
      }
      return;
    }
    if (s.kind == Stmt::Kind::For) {
      auto index_type = make_int();
      index_type->bounded_index = true;
      index_type->array_size = s.for_end;
      locals[s.for_index] = index_type;
      for (const auto& inner : s.for_body) {
        check_stmt(inner);
      }
      return;
    }
    if (s.kind == Stmt::Kind::ParallelFor) {
      auto index_type = make_int();
      index_type->bounded_index = true;
      index_type->array_size = s.par_end;
      locals[s.par_index] = index_type;
      for (const auto& inner : s.par_body) {
        check_stmt(inner);
      }
      return;
    }
    if (s.kind == Stmt::Kind::Borrow) {
      if (s.init && s.init->kind == Expr::Kind::Ident) {
        const auto it = locals.find(s.init->ident);
        if (it == locals.end()) {
          diags.error(loc(s.span), "unknown borrow source '" + s.init->ident + "'");
        } else {
          locals[s.var_name] = it->second;
        }
      }
      return;
    }
    if (s.kind == Stmt::Kind::Assign && s.init && s.expr) {
      type_of(*s.init);
      type_of(*s.expr);
      return;
    }
    if (s.expr) {
      if (s.expr->kind == Expr::Kind::Call) {
        check_call_args(*s.expr);
      }
      type_of(*s.expr);
    }
  }

  void check_proc(const ProcDecl& p) {
    if (p.is_extern) {
      return;
    }
    locals.clear();
    type_vars.clear();
    for (const auto& tp : p.type_params) {
      type_vars[tp] = make_type_var(tp);
    }
    bool has_requires = false;
    bool has_ensures = false;
    for (const auto& c : p.contracts) {
      if (c.kind == ContractKind::Requires) {
        has_requires = true;
      }
      if (c.kind == ContractKind::Ensures) {
        has_ensures = true;
      }
    }
    if (!has_requires) {
      diags.error(loc(p.span), "proc missing requires clause");
    }
    if (!has_ensures) {
      diags.error(loc(p.span), "proc missing ensures clause");
    }
    if (p.ret_type && !is_unit_type(*p.ret_type)) {
      for (const auto& c : p.contracts) {
        if (c.kind == ContractKind::Ensures && c.expr && is_false_literal(*c.expr)) {
          diags.error(loc(c.span), "E0303: `ensures false` is impossible for a value-returning procedure");
          break;
        }
        if (postcondition_is_false_for_literal_return(p, c)) {
          diags.error(loc(c.span),
                      "E0303: postcondition is false for the procedure's literal return value");
          break;
        }
        if (!allow_open_vc() && c.kind == ContractKind::Ensures && c.expr &&
            is_true_literal(*c.expr)) {
          diags.error(loc(c.span),
                      "E0303: `ensures true` is not allowed for value-returning procedures; "
                      "relate `result` to the computation");
          break;
        }
      }
    }
    std::optional<TyPtr> ret_ty;
    if (p.ret_type) {
      ret_ty = resolve_type_expr(*p.ret_type);
    }
    for (const auto& param : p.params) {
      const TyPtr pt = resolve_type_expr(param.type);
      if (pt->kind == TyKind::Array && !param.type.is_var) {
        pt->requires_bounded_index = true;
      }
      locals[param.name] = pt;
    }
    for (const auto& s : p.body) {
      if (s.kind == Stmt::Kind::Return && s.expr) {
        const TyPtr got = type_of(*s.expr);
        if (ret_ty && !assignable(got, *ret_ty)) {
          diags.error(loc(s.span), "return type mismatch for generic type parameter");
        }
        continue;
      }
      check_stmt(s);
    }
  }
};

}  // namespace

TypecheckResult typecheck_module(const Module& module, std::size_t main_proc_count) {
  TypecheckResult result;
  Ctx ctx{{}, {}, {}, {}, result.diagnostics, "module"};
  for (const auto& proc : module.procs) {
    ctx.procs[proc.name] = &proc;
  }
  for (const auto& alias : module.types) {
    AliasEntry entry;
    entry.alias_kind = alias.alias_kind;
    entry.type_params = alias.type_params;
    entry.definition = &alias.definition;
    entry.fields = &alias.fields;
    entry.enum_variants = &alias.enum_variants;
    entry.is_protocol =
        alias.alias_kind == AliasKind::Type && alias.definition.kind == TypeKind::Named &&
        alias.definition.name == "Protocol";
    ctx.aliases[alias.name] = std::move(entry);
  }
  // Only the main module's own proc bodies are checked. Imported procs are
  // in scope (ctx.procs) so calls resolve their signatures, but their bodies
  // are lowered without a typecheck pass, so checking them now would reject
  // files whose imports use constructs this frontend does not yet support.
  const std::size_t check_count = std::min(main_proc_count, module.procs.size());
  for (std::size_t n = 0; n < check_count; ++n) {
    ctx.check_proc(module.procs[n]);
  }
  borrow_check_module(module, result.diagnostics, main_proc_count);
  effects_check_module(module, result.diagnostics, main_proc_count);
  result.ok = result.diagnostics.empty();
  return result;
}

}  // namespace li
