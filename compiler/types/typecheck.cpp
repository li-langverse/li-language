#include "li/typecheck.hpp"

#include "li/borrowck.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace li {
namespace {

enum class TyKind {
  Int, Int64, Float, Bool, Str, Array, List, Dict, Tuple, TypedDict, Enum, Named, TypeVar,
  Protocol, Callable
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
  bool tuple_variadic = false;
  std::vector<std::string> enum_variants;
};

TyPtr make_int() { return std::make_shared<Ty>(Ty{TyKind::Int}); }
TyPtr make_float() { return std::make_shared<Ty>(Ty{TyKind::Float}); }
TyPtr make_bool() { return std::make_shared<Ty>(Ty{TyKind::Bool}); }
TyPtr make_str() { return std::make_shared<Ty>(Ty{TyKind::Str}); }
TyPtr make_i64() { return std::make_shared<Ty>(Ty{TyKind::Int64}); }

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
      if (te.name == "ptr" || te.name == "int64" || te.name == "i64" || te.name == "long") {
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
            e.bin_op == BinOp::Div) {
          if (l->kind == TyKind::Int && r->kind == TyKind::Int) {
            return make_int();
          }
          if (l->kind == TyKind::Float && r->kind == TyKind::Float) {
            return make_float();
          }
          diags.error(loc(e.span),
                      "cannot mix int and float in arithmetic without explicit cast");
          return make_int();
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
        const auto pit = procs.find(e.ident);
        if (pit != procs.end()) {
          const ProcDecl& callee = *pit->second;
          for (const auto& arg : e.args) {
            (void)type_of(*arg);
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
        }
        return base->elem;
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
        type_of(*s.cond);
      }
      for (const auto& inner : s.while_body) {
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
    std::optional<TyPtr> ret_ty;
    if (p.ret_type) {
      ret_ty = resolve_type_expr(*p.ret_type);
    }
    for (const auto& param : p.params) {
      const TyPtr pt = resolve_type_expr(param.type);
      locals[param.name] = pt;
    }
    for (const auto& s : p.body) {
      if (s.kind == Stmt::Kind::Return && s.expr && ret_ty) {
        const TyPtr got = type_of(*s.expr);
        if (!assignable(got, *ret_ty)) {
          diags.error(loc(s.span), "return type mismatch for generic type parameter");
        }
      }
      check_stmt(s);
    }
  }
};

}  // namespace

TypecheckResult typecheck_module(const Module& module) {
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
  for (const auto& proc : module.procs) {
    ctx.check_proc(proc);
  }
  borrow_check_module(module, result.diagnostics);
  effects_check_module(module, result.diagnostics);
  result.ok = result.diagnostics.empty();
  return result;
}

}  // namespace li
