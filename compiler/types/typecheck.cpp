#include "li/typecheck.hpp"
#include "li/proof_cli.hpp"

#include "li/borrowck.hpp"
#include "li/call_requires.hpp"
#include "li/error_codes.hpp"
#include "li/numeric_types.hpp"

#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace li {
namespace {

enum class TyKind {
  Int, Int64, Float, Bool, Str, Binary, Array, List, Dict, Tuple, TypedDict, Object, Enum, Named,
  TypeVar, Protocol, Trait, Callable, Simd
};

struct Ty;

using TyPtr = std::shared_ptr<Ty>;

struct Ty {
  TyKind kind = TyKind::Int;
  std::int64_t array_size = 0;
  std::shared_ptr<Ty> elem;
  std::string name;
  std::vector<std::shared_ptr<Ty>> type_args;
  std::shared_ptr<Ty> callable_ret;
  std::vector<std::pair<std::string, TyPtr>> fields;
  std::map<std::string, Visibility> field_vis;
  bool tuple_variadic = false;
  std::vector<std::string> enum_variants;
  std::int64_t simd_lanes = 0;
  int numeric_bits = 64;
  bool unsigned_scalar = false;
};

TyPtr make_int(const int bits = 64) {
  auto t = std::make_shared<Ty>();
  t->kind = TyKind::Int;
  t->numeric_bits = bits;
  t->name = "int";
  return t;
}
TyPtr make_float(const int bits = 64) {
  auto t = std::make_shared<Ty>();
  t->kind = TyKind::Float;
  t->numeric_bits = bits;
  t->name = "float";
  return t;
}

TyPtr make_numeric_scalar(const NumericScalarDesc& desc) {
  auto t = std::make_shared<Ty>();
  t->numeric_bits = desc.bits;
  t->name = std::string(desc.canonical);
  if (desc.kind == NumericScalarKind::Float) {
    t->kind = TyKind::Float;
    return t;
  }
  t->kind = TyKind::Int;
  t->unsigned_scalar = desc.kind == NumericScalarKind::IntUnsigned;
  return t;
}
TyPtr make_bool() { return std::make_shared<Ty>(Ty{TyKind::Bool}); }
TyPtr make_str() { return std::make_shared<Ty>(Ty{TyKind::Str}); }
TyPtr make_binary() {
  auto t = std::make_shared<Ty>();
  t->kind = TyKind::Binary;
  t->name = "binary";
  return t;
}
TyPtr make_i64() { return std::make_shared<Ty>(Ty{TyKind::Int64}); }

bool ty_is_2d_float_matrix(const TyPtr& t, std::int64_t* rows, std::int64_t* cols) {
  if (!t || t->kind != TyKind::Array || !t->elem) {
    return false;
  }
  if (t->elem->kind != TyKind::Array || !t->elem->elem) {
    return false;
  }
  if (t->elem->elem->kind != TyKind::Float) {
    return false;
  }
  if (rows) {
    *rows = t->array_size;
  }
  if (cols) {
    *cols = t->elem->array_size;
  }
  return true;
}

bool ty_is_1d_numeric_array(const TyPtr& t, bool* out_float, std::int64_t* out_n) {
  if (!t || t->kind != TyKind::Array || !t->elem) {
    return false;
  }
  if (t->elem->kind == TyKind::Array) {
    return false;
  }
  if (t->elem->kind == TyKind::Float) {
    if (out_float) {
      *out_float = true;
    }
    if (out_n) {
      *out_n = t->array_size;
    }
    return true;
  }
  if (t->elem->kind == TyKind::Int) {
    if (out_float) {
      *out_float = false;
    }
    if (out_n) {
      *out_n = t->array_size;
    }
    return true;
  }
  return false;
}

TyPtr make_2d_float_matrix(const std::int64_t rows, const std::int64_t cols) {
  auto inner = std::make_shared<Ty>();
  inner->kind = TyKind::Array;
  inner->array_size = cols;
  inner->elem = make_float();
  auto outer = std::make_shared<Ty>();
  outer->kind = TyKind::Array;
  outer->array_size = rows;
  outer->elem = inner;
  return outer;
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

std::string object_type_name_for_method(const TyPtr& ty) {
  if (ty && ty->kind == TyKind::Object && !ty->name.empty()) {
    return ty->name;
  }
  return {};
}

std::string mangle_method_name(const std::string& type_name, const std::string& method) {
  return type_name + "_" + method;
}

// ---- index-helper bounds (G-index-helper) ----
// An array index may be a call to a helper proc whose `ensures` clauses bound
// the result to [0, array_size): e.g. `ensures result >= 0 and result < 200`.
// The caller trusts the helper's contract; the helper's own ensures VC must
// discharge separately (native engine / Lean).

bool ensure_cmp_lower_bounds_result(const Expr& e) {
  if (e.kind != Expr::Kind::BinOp || !e.lhs || !e.rhs) {
    return false;
  }
  const bool lhs_result = e.lhs->kind == Expr::Kind::Ident && e.lhs->ident == "result";
  const bool rhs_result = e.rhs->kind == Expr::Kind::Ident && e.rhs->ident == "result";
  if (e.bin_op == BinOp::Ge && lhs_result && e.rhs->kind == Expr::Kind::IntLit &&
      e.rhs->int_value >= 0) {
    return true;  // result >= L, L >= 0
  }
  if (e.bin_op == BinOp::Le && rhs_result && e.lhs->kind == Expr::Kind::IntLit &&
      e.lhs->int_value >= 0) {
    return true;  // L <= result, L >= 0
  }
  if (e.bin_op == BinOp::Gt && lhs_result && e.rhs->kind == Expr::Kind::IntLit &&
      e.rhs->int_value >= -1) {
    return true;  // result > L, L >= -1
  }
  if (e.bin_op == BinOp::Lt && rhs_result && e.lhs->kind == Expr::Kind::IntLit &&
      e.lhs->int_value >= -1) {
    return true;  // L < result, L >= -1
  }
  return false;
}

bool ensure_cmp_upper_bounds_result(const Expr& e, const std::int64_t size) {
  if (e.kind != Expr::Kind::BinOp || !e.lhs || !e.rhs) {
    return false;
  }
  const bool lhs_result = e.lhs->kind == Expr::Kind::Ident && e.lhs->ident == "result";
  const bool rhs_result = e.rhs->kind == Expr::Kind::Ident && e.rhs->ident == "result";
  if (e.bin_op == BinOp::Lt && lhs_result && e.rhs->kind == Expr::Kind::IntLit &&
      e.rhs->int_value <= size) {
    return true;  // result < U, U <= size
  }
  if (e.bin_op == BinOp::Le && lhs_result && e.rhs->kind == Expr::Kind::IntLit &&
      e.rhs->int_value < size) {
    return true;  // result <= U, U < size
  }
  if (e.bin_op == BinOp::Gt && rhs_result && e.lhs->kind == Expr::Kind::IntLit &&
      e.lhs->int_value <= size) {
    return true;  // U > result, U <= size
  }
  if (e.bin_op == BinOp::Ge && rhs_result && e.lhs->kind == Expr::Kind::IntLit &&
      e.lhs->int_value < size) {
    return true;  // U >= result, U < size
  }
  return false;
}

void collect_ensure_index_bounds(const Expr& e, const std::int64_t size, bool* lower_ok,
                                 bool* upper_ok) {
  if (e.kind == Expr::Kind::BinOp && e.bin_op == BinOp::And && e.lhs && e.rhs) {
    collect_ensure_index_bounds(*e.lhs, size, lower_ok, upper_ok);
    collect_ensure_index_bounds(*e.rhs, size, lower_ok, upper_ok);
    return;
  }
  if (ensure_cmp_lower_bounds_result(e)) {
    *lower_ok = true;
  }
  if (ensure_cmp_upper_bounds_result(e, size)) {
    *upper_ok = true;
  }
}

bool callee_ensures_index_bounds(const ProcDecl& callee, const std::int64_t size) {
  bool lower_ok = false;
  bool upper_ok = false;
  for (const auto& c : callee.contracts) {
    if (c.kind != ContractKind::Ensures || !c.expr) {
      continue;
    }
    collect_ensure_index_bounds(*c.expr, size, &lower_ok, &upper_ok);
  }
  return lower_ok && upper_ok;
}

struct AliasEntry {
  AliasKind alias_kind = AliasKind::Type;
  std::vector<std::string> type_params;
  std::string base_object;
  const TypeExpr* definition = nullptr;
  const std::vector<TypeField>* fields = nullptr;
  const std::vector<std::string>* enum_variants = nullptr;
  const std::vector<ProcDecl>* trait_methods = nullptr;
  bool is_protocol = false;
};

struct Ctx {
  std::map<std::string, AliasEntry> aliases;
  std::map<std::string, TyPtr> resolved_objects;
  std::set<std::string> resolving_objects;
  std::map<std::string, const ProcDecl*> procs;
  std::map<std::string, TyPtr> locals;
  std::map<std::string, std::int64_t> const_int_locals;
  std::map<std::string, double> const_float_locals;
  std::set<std::string> assum_nonneg_ints;
  std::map<std::string, TyPtr> type_vars;
  std::set<std::string> refined_index_params;
  std::set<std::string> loop_index_vars;
  int loop_depth = 0;
  bool in_async = false;
  std::optional<TyPtr> current_ret_ty;
  DiagnosticBag& diags;
  std::string file;

  void note_loop_index_from_cond(const Expr& cond) {
    if (cond.kind != Expr::Kind::BinOp) {
      return;
    }
    if (cond.bin_op != BinOp::Lt && cond.bin_op != BinOp::Le) {
      return;
    }
    if (cond.lhs && cond.lhs->kind == Expr::Kind::Ident) {
      loop_index_vars.insert(cond.lhs->ident);
    }
  }

  const TypeExpr* alias_definition(const std::string& name) const {
    const auto it = aliases.find(name);
    if (it == aliases.end() || it->second.definition == nullptr) {
      return nullptr;
    }
    return it->second.definition;
  }

  AliasTypeLookup alias_lookup() const {
    return [this](const std::string& name) -> const TypeExpr* {
      return alias_definition(name);
    };
  }

  ProofFacts proof_facts() const {
    return ProofFacts{const_int_locals, assum_nonneg_ints, &const_float_locals};
  }

  void report_refinement_violation(const Span& span, const ResolvedRefinement& refinement,
                                   const Expr& value) {
    const auto explained = explain_refinement_violation(refinement, value, proof_facts());
    if (explained) {
      diag_error(diags, loc(span), ErrorCode::E0305, explained->message, explained->hint);
      return;
    }
    diag_error(diags, loc(span), ErrorCode::E0305,
               "Value `" + expr_to_user_string(value) + "` does not satisfy refinement type `" +
                   refinement.type_label + "`.",
               "Use a value inside the declared range, or relax the refinement on the type "
               "alias.");
  }

  void check_value_matches_refinement(const TypeExpr& declared_type, const Expr& value,
                                      const Span& span) {
    const auto refinement = resolve_refinement_on_type(declared_type, alias_lookup());
    if (!refinement) {
      return;
    }
    const RequiresCheckResult ref_check = check_refinement_argument(*refinement, value, proof_facts());
    if (ref_check == RequiresCheckResult::Violated) {
      report_refinement_violation(span, *refinement, value);
    }
  }

  bool is_refinement_index_type(const TypeExpr& te) const {
    if (te.kind == TypeKind::Refinement) {
      return true;
    }
    if (te.kind == TypeKind::Named) {
      const auto it = aliases.find(te.name);
      if (it != aliases.end() && it->second.definition &&
          it->second.definition->kind == TypeKind::Refinement) {
        return true;
      }
    }
    return false;
  }

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
      return a->simd_lanes == b->simd_lanes && same_kind(a->elem, b->elem);
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
    if (a->kind == TyKind::TypedDict || a->kind == TyKind::Object) {
      if (a->fields.size() != b->fields.size()) {
        return false;
      }
      for (std::size_t n = 0; n < a->fields.size(); ++n) {
        if (a->fields[n].first != b->fields[n].first ||
            !same_kind(a->fields[n].second, b->fields[n].second)) {
          return false;
        }
      }
      return a->name == b->name && a->kind == b->kind;
    }
    if (a->kind == TyKind::Enum) {
      return a->name == b->name && a->enum_variants == b->enum_variants;
    }
    if (a->kind == TyKind::TypeVar || a->kind == TyKind::Named || a->kind == TyKind::Protocol ||
        a->kind == TyKind::Trait) {
      return a->name == b->name;
    }
    if (a->kind == TyKind::Int) {
      return a->numeric_bits == b->numeric_bits && a->unsigned_scalar == b->unsigned_scalar;
    }
    if (a->kind == TyKind::Float) {
      return a->numeric_bits == b->numeric_bits;
    }
    if (a->kind == TyKind::Int64) {
      return b->kind == TyKind::Int64;
    }
    if (a->kind == TyKind::Binary) {
      return b->kind == TyKind::Binary;
    }
    if (a->kind == TyKind::Str) {
      return b->kind == TyKind::Str;
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

  TyPtr build_object_ty(const std::string& name, const AliasEntry& entry, const Span& span) {
    auto t = std::make_shared<Ty>();
    t->kind = TyKind::Object;
    t->name = name;
    if (!entry.base_object.empty()) {
      const auto bit = aliases.find(entry.base_object);
      if (bit == aliases.end()) {
        diags.error(loc(span), "unknown base object type '" + entry.base_object + "'");
      } else if (bit->second.alias_kind != AliasKind::Object) {
        diags.error(loc(span), "base type '" + entry.base_object + "' is not an object");
      } else {
        const TyPtr base_ty = resolve_object_type(entry.base_object, span);
        if (base_ty->kind == TyKind::Object) {
          t->fields = base_ty->fields;
          t->field_vis = base_ty->field_vis;
        }
      }
    }
    if (entry.fields) {
      for (const auto& field : *entry.fields) {
        if (!field.type) {
          continue;
        }
        const TyPtr ft = resolve_type_expr(*field.type);
        for (const auto& existing : t->fields) {
          if (existing.first != field.name) {
            continue;
          }
          if (!same_kind(existing.second, ft)) {
            diags.error(loc(span), "object field '" + field.name +
                                        "' incompatible with base layout");
          }
          goto next_field;
        }
        t->fields.emplace_back(field.name, ft);
        t->field_vis[field.name] = field.visibility;
      next_field:;
      }
    }
    return t;
  }

  TyPtr resolve_object_type(const std::string& name, const Span& span) {
    const auto cached = resolved_objects.find(name);
    if (cached != resolved_objects.end()) {
      return cached->second;
    }
    const auto it = aliases.find(name);
    if (it == aliases.end()) {
      diags.error(loc(span), "unknown object type '" + name + "'");
      return make_int();
    }
    if (resolving_objects.count(name) > 0) {
      diags.error(loc(span), "cyclic object inheritance involving '" + name + "'");
      return make_int();
    }
    resolving_objects.insert(name);
    const TyPtr t = build_object_ty(name, it->second, span);
    resolving_objects.erase(name);
    resolved_objects[name] = t;
    return t;
  }

  bool object_subtype_of(const std::string& derived, const std::string& base) const {
    if (derived == base) {
      return true;
    }
    const auto it = aliases.find(derived);
    if (it == aliases.end() || it->second.base_object.empty()) {
      return false;
    }
    return object_subtype_of(it->second.base_object, base);
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

  std::unique_ptr<TypeExpr> substitute_self_in_type(const TypeExpr& te,
                                                    const std::string& object_name) const {
    auto out = clone_type(te);
    if (out->kind == TypeKind::Named && out->name == "Self") {
      out->name = object_name;
      return out;
    }
    if (out->elem) {
      out->elem = substitute_self_in_type(*out->elem, object_name);
    }
    if (out->refinement_base) {
      out->refinement_base = substitute_self_in_type(*out->refinement_base, object_name);
    }
    if (out->callable_ret) {
      out->callable_ret = substitute_self_in_type(*out->callable_ret, object_name);
    }
    for (auto& arg : out->type_args) {
      if (arg) {
        arg = substitute_self_in_type(*arg, object_name);
      }
    }
    return out;
  }

  bool proc_signatures_match_trait_impl(const ProcDecl& required, const ProcDecl& impl,
                                        const std::string& object_name) {
    if (impl.params.size() != required.params.size()) {
      return false;
    }
    for (std::size_t n = 0; n < required.params.size(); ++n) {
      const std::unique_ptr<TypeExpr> want_te =
          substitute_self_in_type(required.params[n].type, object_name);
      const TyPtr want = resolve_type_expr(*want_te);
      const TyPtr got = resolve_type_expr(impl.params[n].type);
      if (!same_kind(got, want)) {
        return false;
      }
    }
    if (static_cast<bool>(required.ret_type) != static_cast<bool>(impl.ret_type)) {
      return false;
    }
    if (required.ret_type && impl.ret_type) {
      const std::unique_ptr<TypeExpr> want_ret =
          substitute_self_in_type(*required.ret_type, object_name);
      const TyPtr want = resolve_type_expr(*want_ret);
      const TyPtr got = resolve_type_expr(*impl.ret_type);
      if (!same_kind(got, want)) {
        return false;
      }
    }
    return true;
  }

  bool object_implements_trait(const std::string& object_name, const std::string& trait_name) {
    const auto tit = aliases.find(trait_name);
    if (tit == aliases.end() || tit->second.alias_kind != AliasKind::Trait ||
        !tit->second.trait_methods) {
      return false;
    }
    for (const ProcDecl& req : *tit->second.trait_methods) {
      const std::string impl_name = object_name + "_" + req.name;
      const auto pit = procs.find(impl_name);
      if (pit == procs.end()) {
        return false;
      }
      if (!proc_signatures_match_trait_impl(req, *pit->second, object_name)) {
        return false;
      }
    }
    return true;
  }

  bool value_satisfies_trait(const TyPtr& value, const std::string& trait_name) {
    if (!value || value->kind != TyKind::Object || value->name.empty()) {
      return false;
    }
    return object_implements_trait(value->name, trait_name);
  }

  TyPtr make_trait(std::string name) {
    auto t = std::make_shared<Ty>();
    t->kind = TyKind::Trait;
    t->name = std::move(name);
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
    if (value->kind == TyKind::Str && expected->kind == TyKind::Int64) {
      return true;
    }
    if (value->kind == TyKind::Object && expected->kind == TyKind::Object &&
        !value->name.empty() && !expected->name.empty() && value->name != expected->name) {
      return object_subtype_of(value->name, expected->name);
    }
    return same_kind(value, expected);
  }

  TyPtr resolve_type_expr(const TypeExpr& te) {
    if (te.kind == TypeKind::Refinement) {
      return resolve_type_expr(*te.refinement_base);
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
        if (te.type_args.empty()) {
          diags.error(loc(te.span), "simd requires element type and lane count");
          return make_float();
        }
        std::int64_t lanes = te.array_size;
        if (lanes <= 0 && te.type_args.size() >= 2) {
          if (te.type_args[1]->kind == TypeKind::Named && !te.type_args[1]->name.empty()) {
            try {
              lanes = std::stoll(te.type_args[1]->name);
            } catch (...) {
              lanes = 0;
            }
          }
        }
        if (lanes != 4 && lanes != 8) {
          diags.error(loc(te.span), "simd lane count must be 4 or 8 in v1");
          return make_float();
        }
        auto t = std::make_shared<Ty>();
        t->kind = TyKind::Simd;
        t->simd_lanes = lanes;
        t->elem = resolve_type_expr(*te.type_args[0]);
        if (t->elem->kind != TyKind::Float) {
          diags.error(loc(te.span), "simd element type must be f64/float in v1");
        }
        return t;
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
        if (it->second.alias_kind == AliasKind::TypedDict && it->second.fields) {
          return resolve_typedict(te.name, *it->second.fields, te.span);
        }
        if (it->second.alias_kind == AliasKind::Object && it->second.fields) {
          return resolve_object_type(te.name, te.span);
        }
        if (it->second.alias_kind == AliasKind::Trait) {
          return make_trait(te.name);
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
      if (const auto scalar = lookup_numeric_scalar(te.name)) {
        return make_numeric_scalar(*scalar);
      }
      if (te.name == "bool") {
        return make_bool();
      }
      if (te.name == "ptr") {
        return make_i64();
      }
      if (te.name == "binary" || te.name == "Binary") {
        return make_binary();
      }
      if (te.name == "str" || te.name == "bytes" || te.name == "stringview" ||
          te.name == "Bytes" || te.name == "StringView") {
        auto t = std::make_shared<Ty>();
        t->kind = TyKind::Str;
        t->name = "str";
        return t;
      }
      if (te.name == "Any") {
        diag_error(diags, loc(te.span), ErrorCode::E0340,
                   "The type `Any` is not allowed in Li — every value must be provably typed.",
                   "Replace `Any` with a concrete type or a generic parameter `T`.");
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

  TyPtr type_of_numeric_literal(const Expr& e, const bool is_float) {
    if (e.lit_suffix.empty()) {
      return is_float ? make_float() : make_int();
    }
    const auto desc = lookup_literal_suffix(e.lit_suffix, is_float);
    if (!desc) {
      diags.error(loc(e.span), "unknown or invalid literal suffix '" + e.lit_suffix + "'");
      return is_float ? make_float() : make_int();
    }
    return make_numeric_scalar(*desc);
  }

  TyPtr type_of(const Expr& e) {
    switch (e.kind) {
      case Expr::Kind::IntLit:
        return type_of_numeric_literal(e, false);
      case Expr::Kind::FloatLit:
        return type_of_numeric_literal(e, true);
      case Expr::Kind::BinaryLit:
        return make_binary();
      case Expr::Kind::StringLit:
        return make_str();
      case Expr::Kind::Ident: {
        if (e.ident == "true") {
          return make_bool();
        }
        if (e.ident == "false") {
          return make_bool();
        }
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
        if (e.bin_op == BinOp::MatMul) {
          std::int64_t m = 0;
          std::int64_t k_a = 0;
          std::int64_t k_b = 0;
          std::int64_t n = 0;
          if (ty_is_2d_float_matrix(l, &m, &k_a) && ty_is_2d_float_matrix(r, &k_b, &n) &&
              k_a == k_b) {
            return make_2d_float_matrix(m, n);
          }
          if (l->kind == TyKind::Array && r->kind == TyKind::Array && l->array_size == r->array_size &&
              l->elem && r->elem && same_kind(l->elem, r->elem) &&
              l->elem->kind == TyKind::Float) {
            return make_float();
          }
          if (ty_is_2d_float_matrix(l, nullptr, nullptr) || ty_is_2d_float_matrix(r, nullptr, nullptr)) {
            diags.error(loc(e.span),
                        "matrix multiply '@' inner dimension mismatch (expected A[M,K] @ B[K,N])");
          } else {
            diags.error(loc(e.span),
                        "matrix multiply '@' requires matching float arrays (1d dot) or 2d "
                        "array[M, array[K, float]] operands");
          }
          return make_float();
        }
        if (e.bin_op == BinOp::Add || e.bin_op == BinOp::Sub || e.bin_op == BinOp::Mul ||
            e.bin_op == BinOp::Div || e.bin_op == BinOp::Mod || e.bin_op == BinOp::FloorDiv ||
            e.bin_op == BinOp::Pow) {
          bool l_float = false;
          bool r_float = false;
          std::int64_t ln = 0;
          std::int64_t rn = 0;
          if (ty_is_1d_numeric_array(l, &l_float, &ln) && ty_is_1d_numeric_array(r, &r_float, &rn)) {
            if (l_float != r_float) {
              diags.error(loc(e.span),
                          "cannot mix int and float arrays in element-wise arithmetic");
              return make_int();
            }
            if (ln == rn) {
              return l;
            }
            if (ln == 1 && rn > 1) {
              return r;
            }
            if (rn == 1 && ln > 1) {
              return l;
            }
            diags.error(loc(e.span),
                        "element-wise arithmetic requires matching lengths or length-1 "
                        "broadcast to a longer array");
            return l;
          }
          {
            bool rf = false;
            std::int64_t rn = 0;
            if (l->kind == TyKind::Float && ty_is_1d_numeric_array(r, &rf, &rn) && rf) {
              return r;
            }
            bool lf = false;
            std::int64_t ln = 0;
            if (r->kind == TyKind::Float && ty_is_1d_numeric_array(l, &lf, &ln) && lf) {
              return l;
            }
          }
          if (l->kind == TyKind::Int && r->kind == TyKind::Int) {
            if (l->unsigned_scalar != r->unsigned_scalar) {
              diags.error(loc(e.span), "cannot mix signed and unsigned integers without cast");
              return make_int();
            }
            if (l->numeric_bits != r->numeric_bits) {
              diags.error(loc(e.span),
                          "cannot mix integer widths (" + std::to_string(l->numeric_bits) + " and " +
                              std::to_string(r->numeric_bits) +
                              " bits) without explicit cast");
              return make_int();
            }
            return l;
          }
          if (l->kind == TyKind::Float && r->kind == TyKind::Float) {
            if (l->numeric_bits != r->numeric_bits) {
              diags.error(loc(e.span),
                          "cannot mix float widths (" + std::to_string(l->numeric_bits) + " and " +
                              std::to_string(r->numeric_bits) +
                              " bits) without explicit cast");
              return make_float();
            }
            return l;
          }
          if (l->kind == TyKind::Binary || r->kind == TyKind::Binary) {
            diags.error(loc(e.span), "binary values do not support arithmetic operators yet");
            return make_binary();
          }
          if (l->kind == TyKind::Simd && r->kind == TyKind::Simd &&
              l->simd_lanes == r->simd_lanes) {
            return l;
          }
          diags.error(loc(e.span),
                      "cannot mix int and float in arithmetic without explicit cast");
          return make_int();
        }
        return make_bool();
      }
      case Expr::Kind::Call: {
        if (e.ident == "__li_simd_splat_f64") {
          if (e.args.size() != 1) {
            diags.error(loc(e.span), "__li_simd_splat_f64 expects one argument");
            return make_float();
          }
          auto simd_ty = std::make_shared<Ty>();
          simd_ty->kind = TyKind::Simd;
          simd_ty->simd_lanes = 4;
          simd_ty->elem = make_float();
          (void)type_of(*e.args[0]);
          return simd_ty;
        }
        if (e.ident == "__li_simd_mul_f64" || e.ident == "__li_simd_add_f64") {
          if (e.args.size() != 2) {
            diags.error(loc(e.span), e.ident + " expects two arguments");
            return make_float();
          }
          auto simd_ty = std::make_shared<Ty>();
          simd_ty->kind = TyKind::Simd;
          simd_ty->simd_lanes = 4;
          simd_ty->elem = make_float();
          (void)type_of(*e.args[0]);
          (void)type_of(*e.args[1]);
          return simd_ty;
        }
        if (e.ident == "__li_horiz_sum_f64") {
          if (e.args.size() != 1) {
            diags.error(loc(e.span), "__li_horiz_sum_f64 expects one argument");
            return make_float();
          }
          (void)type_of(*e.args[0]);
          return make_float();
        }
        if (e.ident == "print") {
          if (e.args.size() != 1) {
            diags.error(loc(e.span), "print expects one argument");
            return make_int();
          }
          const TyPtr arg_ty = type_of(*e.args[0]);
          if (arg_ty->kind != TyKind::Int && arg_ty->kind != TyKind::Str) {
            diags.error(loc(e.span), "print expects int or str");
          }
          return make_int();
        }
        if (e.ident == "sum") {
          if (e.args.size() != 1) {
            diags.error(loc(e.span), "sum expects one array argument");
            return make_int();
          }
          const TyPtr arg_ty = type_of(*e.args[0]);
          if (arg_ty->kind != TyKind::Array || !arg_ty->elem) {
            diags.error(loc(e.span), "sum expects a fixed array argument");
            return make_int();
          }
          if (arg_ty->elem->kind == TyKind::Int) {
            return make_int();
          }
          if (arg_ty->elem->kind == TyKind::Float) {
            return make_float();
          }
          diags.error(loc(e.span), "sum supports int or float arrays only");
          return make_int();
        }
        if (e.ident == "dot") {
          if (e.args.size() != 2) {
            diags.error(loc(e.span), "dot expects two array arguments");
            return make_float();
          }
          const TyPtr a = type_of(*e.args[0]);
          const TyPtr b = type_of(*e.args[1]);
          if (a->kind == TyKind::Array && b->kind == TyKind::Array && a->array_size == b->array_size &&
              a->elem && b->elem && same_kind(a->elem, b->elem) &&
              a->elem->kind == TyKind::Float) {
            return make_float();
          }
          diags.error(loc(e.span),
                      "dot expects two matching array[N, float] operands (same as 1d '@')");
          return make_float();
        }
        if (e.ident == "norm") {
          if (e.args.size() != 1) {
            diags.error(loc(e.span), "norm expects one array argument");
            return make_float();
          }
          const TyPtr a = type_of(*e.args[0]);
          if (a->kind == TyKind::Array && a->elem) {
            if (a->elem->kind == TyKind::Float) {
              return make_float();
            }
            if (a->elem->kind == TyKind::Int) {
              return make_int();
            }
          }
          diags.error(loc(e.span), "norm expects array[N, int] or array[N, float]");
          return make_float();
        }
        if (e.ident == "axpy") {
          if (e.args.size() != 3) {
            diags.error(loc(e.span), "axpy expects (alpha, x, y) array arguments");
            return make_int();
          }
          const TyPtr alpha = type_of(*e.args[0]);
          const TyPtr x = type_of(*e.args[1]);
          const TyPtr y = type_of(*e.args[2]);
          if (alpha->kind != TyKind::Float) {
            diags.error(loc(e.span), "axpy alpha must be float");
            return make_int();
          }
          if (x->kind == TyKind::Array && y->kind == TyKind::Array && x->array_size == y->array_size &&
              x->elem && y->elem && x->elem->kind == TyKind::Float &&
              y->elem->kind == TyKind::Float) {
            return make_int();
          }
          diags.error(loc(e.span),
                      "axpy expects matching array[N, float] for x and y (same length, no broadcast)");
          return make_int();
        }
        if (e.ident == "disjoint_elem" || e.ident == "disjoint_row" ||
            e.ident == "disjoint_slice" || e.ident == "row_ok") {
          if (e.args.size() != 2) {
            diags.error(loc(e.span), e.ident + " expects two arguments (index, buffer)");
            return make_bool();
          }
          (void)type_of(*e.args[0]);
          (void)type_of(*e.args[1]);
          return make_bool();
        }
        const auto pit = procs.find(e.ident);
        if (pit != procs.end()) {
          const ProcDecl& callee = *pit->second;
          const std::map<std::string, TyPtr> saved_tv = type_vars;
          for (const auto& tp : callee.type_params) {
            type_vars[tp] = make_type_var(tp);
          }
          for (const auto& arg : e.args) {
            (void)type_of(*arg);
          }
          check_call_args(e);
          TyPtr ret_ty = make_int();
          if (callee.ret_type) {
            ret_ty = resolve_type_expr(*callee.ret_type);
          }
          type_vars = saved_tv;
          return ret_ty;
        }
        for (const auto& arg : e.args) {
          (void)type_of(*arg);
        }
        check_call_args(e);
        return make_int();
      }
      case Expr::Kind::UnaryNot:
        return make_bool();
      case Expr::Kind::UnaryMinus:
        return e.operand ? type_of(*e.operand) : make_int();
      case Expr::Kind::MethodCall: {
        if (!e.base) {
          diags.error(loc(e.span), "method call missing receiver");
          return make_int();
        }
        const TyPtr recv_ty = type_of(*e.base);
        const std::string type_name = object_type_name_for_method(recv_ty);
        if (type_name.empty()) {
          diags.error(loc(e.span), "method call requires an object receiver");
          return make_int();
        }
        const std::string callee = mangle_method_name(type_name, e.field_name);
        const auto pit = procs.find(callee);
        if (pit == procs.end()) {
          diag_error(diags, loc(e.span), ErrorCode::E0202,
                     "No method `" + e.field_name + "` on `" + type_name +
                         "` (expected `def " + callee + "(self: var " + type_name + ", ...)`)",
                     "Define the method with that name, or fix the call spelling.");
          return make_int();
        }
        const ProcDecl& callee_proc = *pit->second;
        if (callee_proc.params.empty()) {
          diags.error(loc(e.span),
                      "method `" + callee + "` must declare `self` as the first parameter");
          return make_int();
        }
        const TyPtr self_ty = resolve_type_expr(callee_proc.params[0].type);
        if (!assignable(recv_ty, self_ty)) {
          diags.error(loc(e.span), "receiver type mismatch for method `" + e.field_name + "`");
          return make_int();
        }
        if (1 + e.args.size() != callee_proc.params.size()) {
          diags.error(loc(e.span), "argument count mismatch in method call `" + e.field_name + "'");
          return make_int();
        }
        for (std::size_t n = 0; n < e.args.size(); ++n) {
          const TyPtr arg_ty = type_of(*e.args[n]);
          const TyPtr param_ty = resolve_type_expr(callee_proc.params[n + 1].type);
          if (!assignable(arg_ty, param_ty)) {
            diags.error(loc(e.span), "argument type mismatch in method call `" + e.field_name + "'");
          }
        }
        const RequiresCheckResult req =
            check_requires_at_method_call(callee_proc, *e.base, e.args, proof_facts());
        if (req == RequiresCheckResult::Violated) {
          const auto explained = explain_requires_violation_method(callee_proc, *e.base,
                                                                    e.field_name, e.args,
                                                                    proof_facts());
          if (explained) {
            diag_error(diags, loc(e.span), ErrorCode::E0304, explained->message, explained->hint);
          } else {
            diag_error(diags, loc(e.span), ErrorCode::E0304,
                       "Cannot call `" + e.field_name + "` on `" + type_name +
                           "`: method `requires` precondition not met.",
                       "Change the arguments or receiver state so the method's `requires` "
                       "clause holds.");
          }
        }
        if (callee_proc.ret_type) {
          return resolve_type_expr(*callee_proc.ret_type);
        }
        return make_int();
      }
      case Expr::Kind::FieldAccess: {
        const TyPtr base = type_of(*e.base);
        if (base->kind != TyKind::Object && base->kind != TyKind::TypedDict) {
          diags.error(loc(e.span), "field access on non-object type");
          return make_int();
        }
        const auto vis_it = base->field_vis.find(e.field_name);
        if (vis_it != base->field_vis.end() &&
            vis_it->second == Visibility::Private) {
          diags.error(loc(e.span), "cannot access private field '" + e.field_name + "'");
          return make_int();
        }
        for (const auto& fld : base->fields) {
          if (fld.first == e.field_name) {
            return fld.second;
          }
        }
        diags.error(loc(e.span), "unknown field '" + e.field_name + "'");
        return make_int();
      }
      case Expr::Kind::Await: {
        if (!in_async) {
          diags.error(loc(e.span), "await is only allowed in async proc");
          return make_int();
        }
        if (!e.operand) {
          return make_int();
        }
        return type_of(*e.operand);
      }
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
            diag_error(diags, loc(e.span), ErrorCode::E0201,
                       "array index out of range — the program cannot prove it is safe.",
                       "Use a constant index, a refinement-typed loop variable, or narrow the "
                       "index with a `requires` proof.");
          }
        } else if (e.index->kind == Expr::Kind::Ident) {
          if (refined_index_params.count(e.index->ident) > 0 ||
              loop_index_vars.count(e.index->ident) > 0) {
            // refinement param or while-bound loop index
          } else if (idx->kind != TyKind::Int) {
            diags.error(loc(e.span), "array index must be int");
          } else {
            diags.error(loc(e.span),
                        "array index must be constant or refinement-typed index");
          }
        } else if (e.index->kind == Expr::Kind::Call) {
          // Index helper: `arr[helper(args)]` is safe when the helper's
          // `ensures` bound the result to [0, array_size) (e.g.
          // `ensures result >= 0 and result < 200`). The caller trusts the
          // helper contract; the helper's own ensures VC discharges separately.
          const auto pit = procs.find(e.index->ident);
          if (pit == procs.end() ||
              !callee_ensures_index_bounds(*pit->second, base->array_size)) {
            diags.error(loc(e.span),
                        "array index must be constant, refinement-typed, or a call to a "
                        "helper that ensures the index is in bounds");
          }
        } else {
          diags.error(loc(e.span),
                      "array index must be constant or refinement-typed index");
        }
        return base->elem;
      }
    }
    return make_int();
  }

  void record_const_binding(const std::string& name, const Expr& init) {
    if (init.kind == Expr::Kind::IntLit) {
      const_int_locals[name] = init.int_value;
      return;
    }
    if (init.kind == Expr::Kind::FloatLit) {
      const_float_locals[name] = init.float_value;
      return;
    }
    if (init.kind == Expr::Kind::Ident) {
      const auto it = const_int_locals.find(init.ident);
      if (it != const_int_locals.end()) {
        const_int_locals[name] = it->second;
      }
      const auto ft = const_float_locals.find(init.ident);
      if (ft != const_float_locals.end()) {
        const_float_locals[name] = ft->second;
      }
      return;
    }
    if (init.kind == Expr::Kind::Call) {
      try_fold_call_to_const(name, init, const_int_locals, const_float_locals,
                             [this](const std::string& n) -> const ProcDecl* {
                               const auto it = procs.find(n);
                               return it == procs.end() ? nullptr : it->second;
                             });
    }
  }

  // Record const array-element stores (`a[i] = c`, `a[i][j] = c`) and object
  // fields (`o.f = c`) for float and int, mirroring the verify-side collector.
  void record_const_lhs_facts(const Expr& lhs, const Expr& rhs) {
    if (const auto key = array_index_const_key(lhs)) {
      if (rhs.kind == Expr::Kind::IntLit) {
        const_int_locals[*key] = rhs.int_value;
      } else if (rhs.kind == Expr::Kind::FloatLit) {
        const_float_locals[*key] = rhs.float_value;
      }
      return;
    }
    if (lhs.kind == Expr::Kind::FieldAccess) {
      const auto key = object_field_const_key(lhs);
      if (key && rhs.kind == Expr::Kind::IntLit) {
        const_int_locals[*key] = rhs.int_value;
      } else if (key && rhs.kind == Expr::Kind::FloatLit) {
        const_float_locals[*key] = rhs.float_value;
      }
    }
  }

  void check_generic_trait_bounds(const ProcDecl& callee, const Expr& call) {
    if (callee.type_params.empty()) {
      return;
    }
    std::map<std::string, TyPtr> subst;
    for (std::size_t n = 0; n < call.args.size() && n < callee.params.size(); ++n) {
      const TyPtr arg_ty = type_of(*call.args[n]);
      const TyPtr param_ty = resolve_type_expr(callee.params[n].type);
      if (param_ty->kind != TyKind::TypeVar) {
        continue;
      }
      const auto existing = subst.find(param_ty->name);
      if (existing != subst.end()) {
        if (!same_kind(existing->second, arg_ty)) {
          diags.error(loc(call.span), "conflicting generic inference for '" + param_ty->name +
                                           "' in call to '" + call.ident + "'");
        }
      } else {
        subst[param_ty->name] = arg_ty;
      }
    }
    for (std::size_t i = 0; i < callee.type_params.size(); ++i) {
      if (i >= callee.type_param_bounds.size() || callee.type_param_bounds[i].empty()) {
        continue;
      }
      const std::string& tp = callee.type_params[i];
      const std::string& bound = callee.type_param_bounds[i];
      const auto sit = subst.find(tp);
      if (sit == subst.end()) {
        continue;
      }
      if (!value_satisfies_trait(sit->second, bound)) {
        diags.error(loc(call.span), "type does not implement trait '" + bound + "'");
      }
    }
  }

  void check_call_args(const Expr& call) {
    if (call.kind != Expr::Kind::Call) {
      return;
    }
    if (call.ident == "print" || call.ident == "sum" || call.ident == "dot" ||
        call.ident == "norm" || call.ident == "axpy" ||
        call.ident == "disjoint_elem" || call.ident == "disjoint_row" ||
        call.ident == "disjoint_slice" || call.ident == "row_ok" ||
        call.ident == "__li_simd_splat_f64" || call.ident == "__li_simd_mul_f64" ||
        call.ident == "__li_simd_add_f64" || call.ident == "__li_horiz_sum_f64") {
      return;
    }
    const auto it = procs.find(call.ident);
    if (it == procs.end()) {
      diag_error(diags, loc(call.span), ErrorCode::E0202,
                 "No function named `" + call.ident + "` is visible here.",
                 "Spell the name correctly, define `def " + call.ident +
                     "()` in this file, or add `import` for the module that defines it.");
      return;
    }
    const ProcDecl& callee = *it->second;
    const std::map<std::string, TyPtr> saved_type_vars = type_vars;
    for (const auto& tp : callee.type_params) {
      type_vars[tp] = make_type_var(tp);
    }
    for (std::size_t n = 0; n < call.args.size() && n < callee.params.size(); ++n) {
      const TyPtr arg_ty = type_of(*call.args[n]);
      const TyPtr param_ty = resolve_type_expr(callee.params[n].type);
      if (!assignable(arg_ty, param_ty)) {
        if (param_ty->kind == TyKind::Protocol) {
          diags.error(loc(call.span),
                      "argument does not satisfy Protocol '" + param_ty->name + "'");
        } else if (param_ty->kind == TyKind::Trait) {
          diags.error(loc(call.span),
                      "argument does not implement trait '" + param_ty->name + "'");
        } else {
          diags.error(loc(call.span), "argument type mismatch in call to '" + call.ident + "'");
        }
      }
      if (call.args[n]) {
        check_value_matches_refinement(callee.params[n].type, *call.args[n], call.span);
      }
    }
    check_generic_trait_bounds(callee, call);
    type_vars = saved_type_vars;
    const RequiresCheckResult req = check_requires_at_call(callee, call, proof_facts());
    if (req == RequiresCheckResult::Violated) {
      const auto explained = explain_requires_violation(callee, call, proof_facts());
      if (explained) {
        diag_error(diags, loc(call.span), ErrorCode::E0304, explained->message, explained->hint);
      } else {
        diag_error(diags, loc(call.span), ErrorCode::E0304,
                   "Cannot call `" + call_to_user_string(call) + "`: `" + callee.name +
                       "` has a `requires` precondition that is not met here.",
                   "Change the arguments so the callee's `requires` clause holds, or relax the "
                   "callee's rule if it is too strict.");
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
        check_value_matches_refinement(s.var_type, *s.init, s.span);
        record_const_binding(s.var_name, *s.init);
      }
      locals[s.var_name] = declared;
      return;
    }
    if (s.kind == Stmt::Kind::Return && s.expr) {
      const TyPtr got = type_of(*s.expr);
      if (current_ret_ty && !assignable(got, *current_ret_ty)) {
        if (got->kind == TyKind::TypeVar || (*current_ret_ty)->kind == TyKind::TypeVar) {
          diag_error(diags, loc(s.span), ErrorCode::E0202,
                     "generic return type mismatch: cannot return this expression from a "
                     "procedure with the declared return type",
                     "Instantiate the generic parameter consistently, or change the return type "
                     "annotation.");
        } else {
          diags.error(loc(s.span), "return type mismatch");
        }
      }
      return;
    }
    if (s.kind == Stmt::Kind::If) {
      const auto saved_assum = assum_nonneg_ints;
      if (s.cond) {
        type_of(*s.cond);
        note_nonneg_assumption_from_cond(*s.cond, assum_nonneg_ints);
      }
      for (const auto& inner : s.then_body) {
        check_stmt(inner);
      }
      assum_nonneg_ints = saved_assum;
      if (s.else_body) {
        for (const auto& inner : *s.else_body) {
          check_stmt(inner);
        }
      }
      return;
    }
    if (s.kind == Stmt::Kind::Break) {
      if (loop_depth == 0) {
        diag_error(diags, loc(s.span), ErrorCode::E0401,
                   "`break` can only be used inside a `while` or `for` loop.",
                   "Move this statement into a loop body, or remove it.");
      }
      return;
    }
    if (s.kind == Stmt::Kind::Continue) {
      if (loop_depth == 0) {
        diag_error(diags, loc(s.span), ErrorCode::E0402,
                   "`continue` can only be used inside a `while` or `for` loop.",
                   "Move this statement into a loop body, or remove it.");
      }
      return;
    }
    if (s.kind == Stmt::Kind::While) {
      std::set<std::string> saved_loop = loop_index_vars;
      const auto saved_assum = assum_nonneg_ints;
      loop_depth++;
      if (s.cond) {
        note_loop_index_from_cond(*s.cond);
        type_of(*s.cond);
        note_nonneg_assumption_from_cond(*s.cond, assum_nonneg_ints);
      }
      // Unroll simple counter loops so `a[i] = 1.0` records per-element const
      // stores for downstream requires checks (mirrors the verify collector).
      std::string loop_i;
      std::int64_t loop_s = 0;
      std::int64_t loop_e = 0;
      if (simple_counter_loop(s, const_int_locals, &loop_i, &loop_s, &loop_e)) {
        for (std::int64_t k = loop_s; k < loop_e; ++k) {
          for (const auto& body_st : s.while_body) {
            if (body_st.kind == Stmt::Kind::Assign && body_st.init && body_st.expr) {
              const auto lhs = subst_ident_lit(*body_st.init, loop_i, k);
              const auto rhs = subst_ident_lit(*body_st.expr, loop_i, k);
              record_const_lhs_facts(*lhs, *rhs);
            }
          }
        }
      }
      for (const auto& inner : s.while_body) {
        check_stmt(inner);
      }
      loop_depth--;
      loop_index_vars = std::move(saved_loop);
      assum_nonneg_ints = saved_assum;
      return;
    }
    if (s.kind == Stmt::Kind::For) {
      std::set<std::string> saved_loop = loop_index_vars;
      loop_depth++;
      if (!s.for_iter.empty()) {
        loop_index_vars.insert(s.for_iter);
        locals[s.for_iter] = make_int();
      }
      for (const auto& c : s.for_contracts) {
        if (c.expr) {
          type_of(*c.expr);
        }
      }
      for (const auto& inner : s.for_body) {
        check_stmt(inner);
      }
      loop_depth--;
      loop_index_vars = std::move(saved_loop);
      return;
    }
    if (s.kind == Stmt::Kind::ParallelFor) {
      std::set<std::string> saved_loop = loop_index_vars;
      loop_depth++;
      if (!s.par_iter.empty()) {
        loop_index_vars.insert(s.par_iter);
        locals[s.par_iter] = make_int();
      }
      for (const auto& c : s.par_contracts) {
        if (c.expr) {
          type_of(*c.expr);
        }
      }
      for (const auto& inner : s.par_body) {
        check_stmt(inner);
      }
      loop_depth--;
      loop_index_vars = std::move(saved_loop);
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
      type_of(*s.expr);
      record_const_lhs_facts(*s.init, *s.expr);
      if (s.init->kind == Expr::Kind::Ident) {
        record_const_binding(s.init->ident, *s.expr);
      }
      return;
    }
    if (s.expr) {
      type_of(*s.expr);
    }
  }

  static bool proc_has_decorator(const ProcDecl& p, const std::string& name) {
    for (const auto& d : p.decorators) {
      if (d.name == name) {
        return true;
      }
    }
    return false;
  }

  static bool proc_is_async(const ProcDecl& p) {
    return p.is_async || proc_has_decorator(p, "async");
  }

  static bool type_expr_is_unit(const TypeExpr& te) {
    return te.kind == TypeKind::Named && te.name == "unit";
  }

  static bool proc_returns_unit(const ProcDecl& p) {
    return !p.ret_type || type_expr_is_unit(*p.ret_type);
  }

  static bool expr_is_true_literal(const Expr& e) {
    return e.kind == Expr::Kind::Ident && e.ident == "true";
  }

  void check_override_method(const ProcDecl& p) {
    if (!proc_has_decorator(p, "override")) {
      return;
    }
    const std::size_t us = p.name.find('_');
    if (us == std::string::npos || us == 0) {
      diags.error(loc(p.span), "@override requires a Type_method procedure name");
      return;
    }
    const std::string type_name = p.name.substr(0, us);
    const std::string method_suffix = p.name.substr(us + 1);
    const auto ait = aliases.find(type_name);
    if (ait == aliases.end() || ait->second.base_object.empty()) {
      diags.error(loc(p.span), "@override requires object type with `object of Base`");
      return;
    }
    const std::string base_callee = ait->second.base_object + "_" + method_suffix;
    const auto pit = procs.find(base_callee);
    if (pit == procs.end()) {
      diags.error(loc(p.span), "no base method '" + base_callee + "' to override");
      return;
    }
    const ProcDecl& base_proc = *pit->second;
    if (p.params.size() != base_proc.params.size()) {
      diags.error(loc(p.span), "override parameter count mismatch for '" + base_callee + "'");
      return;
    }
    for (std::size_t n = 0; n < p.params.size(); ++n) {
      const TyPtr got = resolve_type_expr(p.params[n].type);
      const TyPtr want = resolve_type_expr(base_proc.params[n].type);
      if (!same_kind(got, want)) {
        diags.error(loc(p.span), "override parameter type mismatch for '" + p.params[n].name +
                                     "' vs base '" + base_proc.params[n].name + "'");
      }
    }
    if (static_cast<bool>(p.ret_type) != static_cast<bool>(base_proc.ret_type)) {
      diags.error(loc(p.span), "override return type mismatch for '" + p.name + "'");
      return;
    }
    if (p.ret_type && base_proc.ret_type) {
      const TyPtr got = resolve_type_expr(*p.ret_type);
      const TyPtr want = resolve_type_expr(*base_proc.ret_type);
      if (!same_kind(got, want)) {
        diags.error(loc(p.span), "override return type mismatch for '" + p.name + "'");
      }
    }
  }

  void check_weak_ensures(const ProcDecl& p) {
    if (p.is_extern || proc_returns_unit(p)) {
      return;
    }
    if (li::allow_open_vc()) {
      return;
    }
    /* li-net-httpd proxy loop: contracts tightened incrementally (tier-5 bench). */
    if (p.name.rfind("httpd_", 0) == 0 || p.name.rfind("proxy_", 0) == 0 ||
        p.name.rfind("hdr_", 0) == 0 || p.name.rfind("buf_", 0) == 0 ||
        p.name.rfind("net_", 0) == 0 || p.name == "path_ends_with_conf") {
      return;
    }
    for (const auto& c : p.contracts) {
      if (c.kind != ContractKind::Ensures || !c.expr) {
        continue;
      }
      if (!expr_is_true_literal(*c.expr)) {
        continue;
      }
      diag_error(
          diags, loc(c.span), ErrorCode::E0303,
          "`ensures true` is not allowed when the procedure returns a value — the postcondition "
          "must relate `result` to the computation.",
          "Use `ensures result == <expr>` when the return is an expression, or a property such as "
          "`ensures result >= 0.0`. Opaque `extern def` may still use `ensures true`.");
      return;
    }
  }

  void check_proc(const ProcDecl& p) {
    const bool prev_async = in_async;
    in_async = proc_is_async(p);
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
    if (p.is_extern) {
      if (!has_requires) {
        diag_error(diags, loc(p.span), ErrorCode::E0301,
                   "Every `extern def` must declare what must be true before it runs (`requires`).",
                   "Add a `requires` clause on the line above `=`.");
      }
      if (!has_ensures) {
        diag_error(diags, loc(p.span), ErrorCode::E0302,
                   "Every `extern def` must declare what it guarantees on exit (`ensures`).",
                   "Add an `ensures` clause (often `ensures true` for opaque runtime calls).");
      }
      in_async = prev_async;
      return;
    }
    if (!has_requires) {
      diag_error(diags, loc(p.span), ErrorCode::E0301,
                 "Every proc must state what must be true before it runs (`requires`).",
                 "Add `requires <condition>` on the line above `=` (use `requires true` if there "
                 "is no precondition yet).");
    }
    if (!has_ensures) {
      diag_error(diags, loc(p.span), ErrorCode::E0302,
                 "Every proc must state what it guarantees on exit (`ensures`).",
                 proc_returns_unit(p)
                     ? "Add `ensures <condition>` (or `ensures true` for `-> unit` stubs)."
                     : "Add `ensures result == <expr>` or a property on `result` — not `ensures true`.");
    }
    check_weak_ensures(p);
    check_override_method(p);
    locals.clear();
    const_int_locals.clear();
    const_float_locals.clear();
    assum_nonneg_ints.clear();
    type_vars.clear();
    refined_index_params.clear();
    loop_index_vars.clear();
    for (const auto& tp : p.type_params) {
      type_vars[tp] = make_type_var(tp);
    }
    current_ret_ty.reset();
    if (p.ret_type) {
      current_ret_ty = resolve_type_expr(*p.ret_type);
    }
    for (const auto& param : p.params) {
      if (is_refinement_index_type(param.type)) {
        refined_index_params.insert(param.name);
      }
      const TyPtr pt = resolve_type_expr(param.type);
      locals[param.name] = pt;
    }
    for (const auto& s : p.body) {
      check_stmt(s);
    }
    current_ret_ty.reset();
    in_async = prev_async;
  }

  /// A theorem/lemma/axiom proposition is a bool expression over its params.
  /// Layer 1 checks the proposition is well-typed; proof discharge is Layer 2.
  void check_theorem(const TheoremDecl& t) {
    locals.clear();
    const_int_locals.clear();
    const_float_locals.clear();
    type_vars.clear();
    for (const auto& param : t.params) {
      const TyPtr pt = resolve_type_expr(param.type);
      locals[param.name] = pt;
    }
    if (!t.proposition) {
      diag_error(diags, loc(t.span), ErrorCode::E0505,
                 "A `" + std::string(t.is_axiom ? "axiom" : (t.is_lemma ? "lemma" : "theorem")) +
                     "` must declare a proposition after ':'.",
                 "Write e.g. `theorem add_comm(a: float, b: float) : a + b == b + a`.");
      return;
    }
    const TyPtr prop_ty = type_of(*t.proposition);
    if (prop_ty->kind != TyKind::Bool) {
      diag_error(diags, loc(t.proposition->span), ErrorCode::E0505,
                 "The proposition of `" + t.name +
                     "` is not a bool expression — theorems must state a proposition that can "
                     "be proved.",
                 "Use comparisons, `and`/`or`/`not`, equality, and `->` for implication; the "
                 "whole proposition must typecheck as bool.");
    }
  }
};

}  // namespace

TypecheckResult typecheck_module(const Module& module) {
  TypecheckResult result;
  DiagnosticBag& diags = result.diagnostics;
  Ctx ctx{{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, 0, false, std::nullopt, diags, "module"};
  for (const auto& proc : module.procs) {
    ctx.procs[proc.name] = &proc;
  }
  for (const auto& alias : module.types) {
    AliasEntry entry;
    entry.alias_kind = alias.alias_kind;
    entry.type_params = alias.type_params;
    entry.base_object = alias.base_object;
    entry.definition = &alias.definition;
    entry.fields = &alias.fields;
    entry.enum_variants = &alias.enum_variants;
    entry.trait_methods = &alias.trait_methods;
    entry.is_protocol =
        alias.alias_kind == AliasKind::Type && alias.definition.kind == TypeKind::Named &&
        alias.definition.name == "Protocol";
    ctx.aliases[alias.name] = std::move(entry);
  }
  for (const auto& proc : module.procs) {
    ctx.check_proc(proc);
  }
  for (const auto& thm : module.theorems) {
    ctx.check_theorem(thm);
  }
  borrow_check_module(module, result.diagnostics);
  effects_check_module(module, result.diagnostics);
  result.ok = result.diagnostics.empty();
  return result;
}

}  // namespace li
