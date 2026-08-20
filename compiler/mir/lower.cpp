#include "li/mir.hpp"
#include "li/mir_runtime_link.hpp"
#include "li/numeric_types.hpp"
#include "li/prelude.hpp"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace li {

namespace {

int temp_counter = 0;
int par_counter = 0;

struct MatrixDims {
  std::int64_t rows = 0;
  std::int64_t cols = 0;
};

struct LowerArrayCtx {
  std::unordered_map<std::string, std::int64_t> float_array_sizes;
  std::unordered_map<std::string, std::int64_t> int_array_sizes;
  std::unordered_map<std::string, MatrixDims> matrix_dims;
  std::unordered_set<std::string>* float_array_names = nullptr;
  std::unordered_set<std::string>* matrix_names = nullptr;
};

LowerArrayCtx* g_arr_ctx = nullptr;
const std::unordered_map<std::string, const TypeExpr*>* g_object_locals = nullptr;
MirModule* g_mir_module = nullptr;

std::string fresh_temp() { return "__t" + std::to_string(temp_counter++); }

std::int64_t mir_vectorized_lanes_from_decorator(const Decorator& d) {
  if (d.name != "vectorized") return 0;
  for (const auto& arg : d.args) {
    if (arg.name == "lanes" && arg.value && arg.value->kind == Expr::Kind::IntLit) {
      return arg.value->int_value;
    }
  }
  return 4;
}

bool mir_decorator_disjoint_proven(const Decorator& d) {
  if (d.name != "parallel") return false;
  for (const auto& arg : d.args) {
    if (arg.name != "disjoint" || !arg.value) continue;
    const Expr& e = *arg.value;
    if (e.kind == Expr::Kind::Ident && (e.ident == "disjoint_elem" || e.ident == "disjoint_slice")) return true;
    if (e.kind == Expr::Kind::Call && (e.ident == "disjoint_elem" || e.ident == "disjoint_row" || e.ident == "disjoint_slice")) return true;
  }
  return false;
}

void copy_decorators(const std::vector<Decorator>& src, std::vector<MirDecorator>& dst) {
  for (const auto& d : src) {
    MirDecorator md;
    md.name = d.name;
    if (d.name == "vectorized") {
      md.vectorized = true;
      md.lanes = mir_vectorized_lanes_from_decorator(d);
    }
    if (d.name == "parallel") {
      md.parallel = true;
      md.disjoint_proven = mir_decorator_disjoint_proven(d);
    }
    for (const auto& arg : d.args) {
      if (arg.name == "lanes" && arg.value && arg.value->kind == Expr::Kind::IntLit) {
        md.lanes = arg.value->int_value;
      }
    }
    dst.push_back(std::move(md));
  }
}

void apply_fn_decorator_codegen_flags(MirFn& fn) {
  for (const auto& d : fn.decorators) {
    if (d.vectorized) {
      (void)d.lanes;
    }
    if (d.name == "no_vectorize") {
      fn.no_vectorize = true;
    }
  }
}

bool stmt_has_vectorized(const std::vector<Decorator>& decos) {
  for (const auto& d : decos) {
    if (d.name == "vectorized") {
      return true;
    }
  }
  return false;
}

bool is_float_type_name(const std::string& n) {
  if (const auto scalar = li::lookup_numeric_scalar(n)) {
    return scalar->kind == li::NumericScalarKind::Float;
  }
  return false;
}

bool is_simd_type(const TypeExpr& t) {
  return t.kind == TypeKind::TypeApp && t.name == "simd";
}

bool is_float_array_type(const TypeExpr& t) {
  return t.kind == TypeKind::Array && t.elem && is_float_type_name(t.elem->name);
}

bool is_2d_float_matrix_type(const TypeExpr& t, std::int64_t* rows, std::int64_t* cols) {
  if (t.kind != TypeKind::Array || !t.elem || t.elem->kind != TypeKind::Array) {
    return false;
  }
  if (!is_float_array_type(*t.elem)) {
    return false;
  }
  if (rows) {
    *rows = t.array_size;
  }
  if (cols) {
    *cols = t.elem->array_size;
  }
  return true;
}

bool matrix_slot_dims(const std::string& name, MatrixDims* out) {
  if (!g_arr_ctx) {
    return false;
  }
  const auto it = g_arr_ctx->matrix_dims.find(name);
  if (it == g_arr_ctx->matrix_dims.end()) {
    return false;
  }
  if (out) {
    *out = it->second;
  }
  return true;
}

bool parse_matrix_index_pair(const Expr& row_index, const Expr& col_index, MirInsn& ins) {
  if (row_index.kind == Expr::Kind::IntLit) {
    ins.index_is_literal = true;
    ins.int_value = row_index.int_value;
  } else if (row_index.kind == Expr::Kind::Ident) {
    ins.index_is_literal = false;
    ins.index_ident = row_index.ident;
  } else {
    return false;
  }
  if (col_index.kind == Expr::Kind::IntLit) {
    ins.rhs_is_literal = true;
    ins.rhs_int = col_index.int_value;
  } else if (col_index.kind == Expr::Kind::Ident) {
    ins.rhs_is_literal = false;
    ins.rhs_ident = col_index.ident;
  } else {
    return false;
  }
  return true;
}

std::int64_t simd_lanes_from_type(const TypeExpr& t) {
  if (t.array_size > 0) {
    return t.array_size;
  }
  return 4;
}

struct LoopLabels {
  std::string head;
  std::string exit;
};

const double* lookup_const_float(const std::unordered_map<std::string, double>& m,
                                 const std::string& name) {
  const auto it = m.find(name);
  if (it == m.end()) {
    return nullptr;
  }
  return &it->second;
}

struct LowerCtx {
  const Module* module = nullptr;
  MirModule* mir = nullptr;
  std::string proc_name;
  const ProcDecl* proc = nullptr;
  std::vector<LoopLabels>* loop_stack = nullptr;
  const std::unordered_map<std::string, const TypeExpr*>* object_locals = nullptr;
  std::unordered_map<std::string, double>* const_floats = nullptr;
};
std::string fresh_label(const std::string& prefix) {
  return prefix + std::to_string(temp_counter++);
}

bool is_string_type_name(const std::string& n) {
  return n == "str" || n == "string";
}

bool is_bytes_type_name(const std::string& n) {
  return n == "bytes";
}

/// Types passed as i8* in LLVM (string literals and byte buffers).
bool mir_ptr_param_type_name(const std::string& n) {
  return is_string_type_name(n) || is_bytes_type_name(n) || n == "StringView";
}

bool is_i64_type_name(const std::string& n) {
  return n == "ptr" || n == "int64" || n == "i64" || n == "long";
}

bool is_int_type_name(const std::string& n) {
  return n == "int" || n == "bool" || n == "unit";
}

bool std_extern_returns_int(std::string_view name) {
  return name == "bytes_len";
}

bool std_extern_returns_str(std::string_view name) {
  return name == "bytes_slice";
}

void push_label(std::vector<MirInsn>& out, const std::string& name) {
  MirInsn ins;
  ins.op = MirOp::Label;
  ins.label = name;
  out.push_back(std::move(ins));
}

void push_jump(std::vector<MirInsn>& out, const std::string& name) {
  MirInsn ins;
  ins.op = MirOp::Jump;
  ins.label = name;
  out.push_back(std::move(ins));
}

void push_branch_if_zero(std::vector<MirInsn>& out, const std::string& ident,
                         const std::string& label) {
  MirInsn ins;
  ins.op = MirOp::BranchIfZero;
  ins.ident = ident;
  ins.label = label;
  out.push_back(std::move(ins));
}

std::string mir_field_slot_for_expr(const Expr& e);

std::string lower_expr_to(const Expr& e, const Module& module, std::vector<MirInsn>& out,
                          std::unordered_set<std::string>& float_names,
                          std::unordered_set<std::string>& simd_names,
                          std::unordered_set<std::string>& i64_locals);

std::string lower_float_array_dot_f64(const std::string& lhs_ident, const std::string& rhs_ident,
                                      std::vector<MirInsn>& out,
                                      std::unordered_set<std::string>& float_names) {
  if (!g_arr_ctx || !g_arr_ctx->float_array_names) {
    return {};
  }
  const auto& names = *g_arr_ctx->float_array_names;
  const auto sz_a = g_arr_ctx->float_array_sizes.find(lhs_ident);
  const auto sz_b = g_arr_ctx->float_array_sizes.find(rhs_ident);
  if (names.count(lhs_ident) == 0 || names.count(rhs_ident) == 0 ||
      sz_a == g_arr_ctx->float_array_sizes.end() ||
      sz_b == g_arr_ctx->float_array_sizes.end() || sz_a->second != sz_b->second) {
    return {};
  }
  const std::string dest = fresh_temp();
  MirInsn dot;
  dot.op = MirOp::ArrayDotF64;
  dot.ident = dest;
  dot.lhs_ident = lhs_ident;
  dot.rhs_ident = rhs_ident;
  dot.int_value = sz_a->second;
  out.push_back(std::move(dot));
  float_names.insert(dest);
  return dest;
}

bool float_array_slot_n(const std::string& ident, std::int64_t* out_n) {
  if (!g_arr_ctx || !g_arr_ctx->float_array_names ||
      g_arr_ctx->float_array_names->count(ident) == 0) {
    return false;
  }
  const auto sz = g_arr_ctx->float_array_sizes.find(ident);
  if (sz == g_arr_ctx->float_array_sizes.end()) {
    return false;
  }
  if (out_n) {
    *out_n = sz->second;
  }
  return true;
}

bool emit_float_array_scale(const std::string& dest, const std::string& arr_ident,
                            bool scale_lit, double scale_val, const std::string& scale_ident,
                            std::vector<MirInsn>& out) {
  std::int64_t n = 0;
  if (!float_array_slot_n(arr_ident, &n)) {
    return false;
  }
  MirInsn ins;
  ins.op = MirOp::ArrayScaleF64;
  ins.ident = dest;
  ins.lhs_ident = arr_ident;
  ins.int_value = n;
  if (scale_lit) {
    ins.rhs_is_literal = true;
    ins.float_value = scale_val;
  } else {
    ins.rhs_is_literal = false;
    ins.rhs_ident = scale_ident;
  }
  out.push_back(std::move(ins));
  return true;
}

std::string lower_float_array_scale_expr(const std::string& arr_ident, bool scale_lit,
                                         double scale_val, const std::string& scale_ident,
                                         std::vector<MirInsn>& out) {
  std::int64_t n = 0;
  if (!float_array_slot_n(arr_ident, &n) || !g_arr_ctx || !g_arr_ctx->float_array_names) {
    return {};
  }
  const std::string dest = fresh_temp();
  MirInsn alloc;
  alloc.op = MirOp::ArrayAlloc;
  alloc.ident = dest;
  alloc.int_value = n;
  alloc.array_is_float = true;
  out.push_back(std::move(alloc));
  g_arr_ctx->float_array_names->insert(dest);
  g_arr_ctx->float_array_sizes[dest] = n;
  if (!emit_float_array_scale(dest, arr_ident, scale_lit, scale_val, scale_ident, out)) {
    return {};
  }
  return dest;
}

bool emit_float_array_axpy(bool alpha_lit, double alpha_val, const std::string& alpha_ident,
                           const std::string& x_ident, const std::string& y_ident,
                           std::vector<MirInsn>& out) {
  std::int64_t n = 0;
  if (!float_array_slot_n(x_ident, &n) || !float_array_slot_n(y_ident, nullptr) ||
      g_arr_ctx->float_array_sizes.at(x_ident) != g_arr_ctx->float_array_sizes.at(y_ident)) {
    return false;
  }
  MirInsn ins;
  ins.op = MirOp::ArrayAxpyF64;
  ins.lhs_ident = x_ident;
  ins.rhs_ident = y_ident;
  ins.int_value = n;
  if (alpha_lit) {
    ins.rhs_is_literal = true;
    ins.float_value = alpha_val;
  } else {
    ins.rhs_is_literal = false;
    ins.ident = alpha_ident;
  }
  out.push_back(std::move(ins));
  return true;
}

bool is_arith_binop(BinOp op) {
  return op == BinOp::Add || op == BinOp::Sub || op == BinOp::Mul || op == BinOp::Div ||
         op == BinOp::Mod || op == BinOp::FloorDiv || op == BinOp::Pow;
}

bool is_cmp_binop(BinOp op) {
  return op == BinOp::Lt || op == BinOp::Le || op == BinOp::Gt || op == BinOp::Ge ||
         op == BinOp::Eq || op == BinOp::Ne;
}

bool try_emit_fma_float_assign(const Expr& e, const std::string& dest,
                               std::vector<MirInsn>& out) {
  if (e.kind != Expr::Kind::BinOp || e.bin_op != BinOp::Add || !e.lhs || !e.rhs) {
    return false;
  }
  double addend = 0.0;
  if (e.rhs->kind == Expr::Kind::FloatLit) {
    addend = e.rhs->float_value;
  } else if (e.rhs->kind == Expr::Kind::IntLit) {
    addend = static_cast<double>(e.rhs->int_value);
  } else {
    return false;
  }
  const Expr& mul = *e.lhs;
  if (mul.kind != Expr::Kind::BinOp || mul.bin_op != BinOp::Mul || !mul.lhs || !mul.rhs) {
    return false;
  }
  std::string acc;
  std::string factor;
  if (mul.lhs->kind == Expr::Kind::Ident && mul.lhs->ident == dest &&
      mul.rhs->kind == Expr::Kind::Ident) {
    acc = dest;
    factor = mul.rhs->ident;
  } else if (mul.rhs->kind == Expr::Kind::Ident && mul.rhs->ident == dest &&
             mul.lhs->kind == Expr::Kind::Ident) {
    acc = dest;
    factor = mul.lhs->ident;
  } else {
    return false;
  }
  MirInsn ins;
  ins.op = MirOp::FmaFloatF64;
  ins.ident = dest;
  ins.lhs_ident = factor;
  ins.rhs_ident = acc;
  ins.float_value = addend;
  out.push_back(std::move(ins));
  return true;
}

bool is_array_elementwise_binop(BinOp op) {
  return op == BinOp::Add || op == BinOp::Sub || op == BinOp::Mul || op == BinOp::Div ||
         op == BinOp::Pow;
}

bool emit_array_elementwise_binop(const std::string& dest, const std::string& lhs_ident,
                                  const std::string& rhs_ident, BinOp op,
                                  std::vector<MirInsn>& out) {
  if (!g_arr_ctx || !is_array_elementwise_binop(op)) {
    return false;
  }
  if (g_arr_ctx->float_array_names) {
    const auto& names = *g_arr_ctx->float_array_names;
    const auto sz_a = g_arr_ctx->float_array_sizes.find(lhs_ident);
    const auto sz_b = g_arr_ctx->float_array_sizes.find(rhs_ident);
    if (names.count(lhs_ident) > 0 && names.count(rhs_ident) > 0 &&
        sz_a != g_arr_ctx->float_array_sizes.end() &&
        sz_b != g_arr_ctx->float_array_sizes.end()) {
      const std::int64_t na = sz_a->second;
      const std::int64_t nb = sz_b->second;
      if (na == nb || (na == 1 && nb > 1) || (nb == 1 && na > 1)) {
        MirInsn ins;
        ins.op = MirOp::ArrayBinOpF64;
        ins.ident = dest;
        ins.lhs_ident = lhs_ident;
        ins.rhs_ident = rhs_ident;
        ins.int_value = na >= nb ? na : nb;
        ins.bin_op = op;
        ins.array_broadcast_lhs_len1 = (na == 1 && nb > 1);
        ins.array_broadcast_rhs_len1 = (nb == 1 && na > 1);
        out.push_back(std::move(ins));
        return true;
      }
    }
  }
  const auto sz_a = g_arr_ctx->int_array_sizes.find(lhs_ident);
  const auto sz_b = g_arr_ctx->int_array_sizes.find(rhs_ident);
  if (sz_a != g_arr_ctx->int_array_sizes.end() && sz_b != g_arr_ctx->int_array_sizes.end()) {
    const std::int64_t na = sz_a->second;
    const std::int64_t nb = sz_b->second;
    if (na == nb || (na == 1 && nb > 1) || (nb == 1 && na > 1)) {
      MirInsn ins;
      ins.op = MirOp::ArrayBinOpI64;
      ins.ident = dest;
      ins.lhs_ident = lhs_ident;
      ins.rhs_ident = rhs_ident;
      ins.int_value = na >= nb ? na : nb;
      ins.bin_op = op;
      ins.array_broadcast_lhs_len1 = (na == 1 && nb > 1);
      ins.array_broadcast_rhs_len1 = (nb == 1 && na > 1);
      out.push_back(std::move(ins));
      return true;
    }
  }
  return false;
}

std::string lower_array_elementwise_binop_expr(const std::string& lhs_ident,
                                               const std::string& rhs_ident, BinOp op,
                                               std::vector<MirInsn>& out) {
  if (!g_arr_ctx || !is_array_elementwise_binop(op)) {
    return {};
  }
  if (g_arr_ctx->float_array_names) {
    const auto& names = *g_arr_ctx->float_array_names;
    const auto sz_a = g_arr_ctx->float_array_sizes.find(lhs_ident);
    const auto sz_b = g_arr_ctx->float_array_sizes.find(rhs_ident);
    if (names.count(lhs_ident) > 0 && names.count(rhs_ident) > 0 &&
        sz_a != g_arr_ctx->float_array_sizes.end() &&
        sz_b != g_arr_ctx->float_array_sizes.end()) {
      const std::int64_t na = sz_a->second;
      const std::int64_t nb = sz_b->second;
      if (na == nb || (na == 1 && nb > 1) || (nb == 1 && na > 1)) {
        const std::int64_t n_out = na >= nb ? na : nb;
        const std::string dest = fresh_temp();
        MirInsn alloc;
        alloc.op = MirOp::ArrayAlloc;
        alloc.ident = dest;
        alloc.int_value = n_out;
        alloc.array_is_float = true;
        out.push_back(std::move(alloc));
        g_arr_ctx->float_array_names->insert(dest);
        g_arr_ctx->float_array_sizes[dest] = n_out;
        emit_array_elementwise_binop(dest, lhs_ident, rhs_ident, op, out);
        return dest;
      }
    }
  }
  const auto ia = g_arr_ctx->int_array_sizes.find(lhs_ident);
  const auto ib = g_arr_ctx->int_array_sizes.find(rhs_ident);
  if (ia != g_arr_ctx->int_array_sizes.end() && ib != g_arr_ctx->int_array_sizes.end()) {
    const std::int64_t na = ia->second;
    const std::int64_t nb = ib->second;
    if (na == nb || (na == 1 && nb > 1) || (nb == 1 && na > 1)) {
      const std::int64_t n_out = na >= nb ? na : nb;
      const std::string dest = fresh_temp();
      MirInsn alloc;
      alloc.op = MirOp::ArrayAlloc;
      alloc.ident = dest;
      alloc.int_value = n_out;
      alloc.array_is_float = false;
      out.push_back(std::move(alloc));
      g_arr_ctx->int_array_sizes[dest] = n_out;
      emit_array_elementwise_binop(dest, lhs_ident, rhs_ident, op, out);
      return dest;
    }
  }
  return {};
}

bool is_float_expr(const Expr& e, const std::unordered_set<std::string>& float_names) {
  switch (e.kind) {
    case Expr::Kind::FloatLit:
      return true;
    case Expr::Kind::Ident:
      return float_names.count(e.ident) > 0;
    case Expr::Kind::FieldAccess: {
      const std::string s = mir_field_slot_for_expr(e);
      if (!s.empty() && float_names.count(s) > 0) {
        return true;
      }
      // Object array fields: `o.f[i]` reads a float element when the slot is
      // registered as a float array.
      std::int64_t n = 0;
      return !s.empty() && float_array_slot_n(s, &n);
    }
    case Expr::Kind::Index: {
      // `a[i]` reads a float element when `a` is a float array (or a float
      // matrix). Nested indices (`m[i][j]`) resolve through the base.
      if (!e.base) {
        return false;
      }
      if (e.base->kind == Expr::Kind::Ident) {
        std::int64_t n = 0;
        if (float_array_slot_n(e.base->ident, &n)) {
          return true;
        }
        if (g_arr_ctx && g_arr_ctx->matrix_names &&
            g_arr_ctx->matrix_names->count(e.base->ident) > 0) {
          return true;
        }
        return false;
      }
      if (e.base->kind == Expr::Kind::FieldAccess) {
        const std::string s = mir_field_slot_for_expr(*e.base);
        std::int64_t n = 0;
        return !s.empty() && float_array_slot_n(s, &n);
      }
      if (e.base->kind == Expr::Kind::Index) {
        // 2D float matrix: the row `m[i]` is itself a float array.
        return is_float_expr(*e.base, float_names);
      }
      return false;
    }
    case Expr::Kind::BinOp:
      if (!is_arith_binop(e.bin_op)) {
        return false;
      }
      return is_float_expr(*e.lhs, float_names) || is_float_expr(*e.rhs, float_names);
    default:
      return false;
  }
}

const ProcDecl* find_proc(const Module& module, const std::string& name) {
  const auto it = std::find_if(module.procs.begin(), module.procs.end(),
                               [&](const ProcDecl& p) { return p.name == name; });
  return it != module.procs.end() ? &*it : nullptr;
}

const TypeAlias* find_type_alias(const Module& module, const std::string& name) {
  for (const auto& t : module.types) {
    if (t.name == name) {
      return &t;
    }
  }
  return nullptr;
}

void for_each_object_field(const Module& module, const TypeAlias& ta,
                          const std::function<void(const TypeField&)>& fn) {
  if (!ta.base_object.empty()) {
    if (const TypeAlias* base = find_type_alias(module, ta.base_object)) {
      for_each_object_field(module, *base, fn);
    }
  }
  for (const auto& fld : ta.fields) {
    fn(fld);
  }
}

const TypeExpr* unwrap_refinement_type(const TypeExpr* ty) {
  while (ty && ty->kind == TypeKind::Refinement && ty->refinement_base) {
    ty = ty->refinement_base.get();
  }
  return ty;
}

const TypeAlias* object_alias_for_named_type(const Module& module, const TypeExpr& te) {
  if (te.kind != TypeKind::Named) {
    return nullptr;
  }
  const TypeAlias* ta = find_type_alias(module, te.name);
  if (!ta || ta->alias_kind != AliasKind::Object) {
    return nullptr;
  }
  if (ta->fields.empty() && ta->base_object.empty()) {
    return nullptr;
  }
  return ta;
}

std::string object_type_name_from_receiver(const Expr& recv) {
  const Expr* cur = &recv;
  while (cur && cur->kind == Expr::Kind::FieldAccess) {
    cur = cur->base.get();
  }
  if (!cur || cur->kind != Expr::Kind::Ident || !g_object_locals) {
    return {};
  }
  const auto it = g_object_locals->find(cur->ident);
  if (it == g_object_locals->end() || !it->second) {
    return {};
  }
  const TypeExpr* te = unwrap_refinement_type(it->second);
  if (te && te->kind == TypeKind::Named) {
    return te->name;
  }
  return {};
}

std::string mangle_method_callee(const std::string& type_name, const std::string& method) {
  return type_name + "_" + method;
}

void emit_scalar_object_slot(const TypeExpr& raw_field_ty, const std::string& slot,
                             std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                             std::unordered_set<std::string>& i64_locals) {
  const TypeExpr* ty = unwrap_refinement_type(&raw_field_ty);
  if (!ty) {
    return;
  }
  MirInsn ins;
  ins.ident = slot;
  if (ty->kind == TypeKind::Array) {
    ins.op = MirOp::ArrayAlloc;
    ins.int_value = ty->array_size;
    ins.array_is_float = ty->elem && is_float_type_name(ty->elem->name);
    out.push_back(std::move(ins));
    if (g_arr_ctx && g_arr_ctx->float_array_names) {
      if (ins.array_is_float) {
        g_arr_ctx->float_array_names->insert(slot);
        g_arr_ctx->float_array_sizes[slot] = ty->array_size;
      } else if (ty->elem && ty->elem->kind == TypeKind::Named && ty->elem->name == "int") {
        g_arr_ctx->int_array_sizes[slot] = ty->array_size;
      }
    }
    return;
  }
  if (is_float_type_name(ty->name)) {
    ins.op = MirOp::LocalAllocFloat;
    float_names.insert(slot);
  } else if (is_i64_type_name(ty->name) || is_string_type_name(ty->name)) {
    ins.op = MirOp::LocalAllocI64;
    i64_locals.insert(slot);
  } else {
    ins.op = MirOp::LocalAllocInt;
  }
  out.push_back(std::move(ins));
}

void emit_object_slots_r(const Module& module, const TypeExpr& te, const std::string& path_prefix,
                         std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                         std::unordered_set<std::string>& i64_locals) {
  const TypeAlias* ta = object_alias_for_named_type(module, te);
  if (!ta) {
    return;
  }
  for_each_object_field(module, *ta, [&](const TypeField& fld) {
    if (!fld.type) {
      return;
    }
    const std::string sub = path_prefix + "_" + fld.name;
    if (object_alias_for_named_type(module, *fld.type)) {
      emit_object_slots_r(module, *fld.type, sub, out, float_names, i64_locals);
    } else {
      emit_scalar_object_slot(*fld.type, sub, out, float_names, i64_locals);
    }
  });
}

void emit_copy_array_slots_r(const TypeExpr& arr_ty, const std::string& src_slot,
                             const std::string& dst_slot, std::vector<MirInsn>& out,
                             std::unordered_set<std::string>& float_names) {
  if (arr_ty.kind != TypeKind::Array || arr_ty.array_size <= 0 || !arr_ty.elem) {
    return;
  }
  const TypeExpr* elem = unwrap_refinement_type(arr_ty.elem.get());
  if (!elem || elem->kind != TypeKind::Named) {
    return;
  }
  const bool fa = is_float_type_name(elem->name);
  const bool ia = elem->name == "int";
  if (!fa && !ia) {
    return;
  }
  for (std::int64_t i = 0; i < arr_ty.array_size; ++i) {
    const std::string t = fresh_temp();
    MirInsn load;
    load.op = MirOp::ArrayLoadInt;
    load.ident = src_slot;
    load.index_is_literal = true;
    load.int_value = i;
    load.lhs_ident = t;
    out.push_back(std::move(load));
    if (fa) {
      float_names.insert(t);
    }
    MirInsn st;
    st.ident = dst_slot;
    st.index_is_literal = true;
    st.int_value = i;
    st.rhs_is_literal = false;
    st.rhs_ident = t;
    st.op = fa ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
    out.push_back(std::move(st));
  }
}

void emit_copy_object_slots_r(const Module& module, const TypeExpr& te, const std::string& src_prefix,
                               const std::string& dst_prefix, std::vector<MirInsn>& out,
                               std::unordered_set<std::string>& float_names,
                               std::unordered_set<std::string>& i64_locals) {
  const TypeAlias* ta = object_alias_for_named_type(module, te);
  if (!ta) {
    return;
  }
  for_each_object_field(module, *ta, [&](const TypeField& fld) {
    if (!fld.type) {
      return;
    }
    const std::string s_sub = src_prefix + "_" + fld.name;
    const std::string d_sub = dst_prefix + "_" + fld.name;
    if (object_alias_for_named_type(module, *fld.type)) {
      emit_copy_object_slots_r(module, *fld.type, s_sub, d_sub, out, float_names, i64_locals);
    } else {
      const TypeExpr* ut = unwrap_refinement_type(fld.type.get());
      if (!ut) {
        return;
      }
      if (ut->kind == TypeKind::Array) {
        emit_copy_array_slots_r(*ut, s_sub, d_sub, out, float_names);
        return;
      }
      MirInsn ins;
      ins.ident = d_sub;
      ins.rhs_is_literal = false;
      ins.rhs_ident = s_sub;
      if (is_float_type_name(ut->name)) {
        ins.op = MirOp::StoreFloat;
      } else if (is_i64_type_name(ut->name) || is_string_type_name(ut->name)) {
        ins.op = MirOp::StoreI64;
      } else {
        ins.op = MirOp::StoreInt;
      }
      out.push_back(std::move(ins));
    }
  });
}

void collect_object_local_types_r(const Module& module, const std::vector<Stmt>& stmts,
                                  std::unordered_map<std::string, const TypeExpr*>& out) {
  for (const auto& st : stmts) {
    switch (st.kind) {
      case Stmt::Kind::VarDecl:
        if (object_alias_for_named_type(module, st.var_type)) {
          out[st.var_name] = &st.var_type;
        }
        break;
      case Stmt::Kind::If:
        collect_object_local_types_r(module, st.then_body, out);
        if (st.else_body) {
          collect_object_local_types_r(module, *st.else_body, out);
        }
        break;
      case Stmt::Kind::While:
        collect_object_local_types_r(module, st.while_body, out);
        break;
      case Stmt::Kind::For:
        collect_object_local_types_r(module, st.for_body, out);
        break;
      case Stmt::Kind::ParallelFor:
        collect_object_local_types_r(module, st.par_body, out);
        break;
      default:
        break;
    }
  }
}

std::unordered_map<std::string, const TypeExpr*> collect_object_local_types(const Module& module,
                                                                            const ProcDecl& proc) {
  std::unordered_map<std::string, const TypeExpr*> out;
  for (const auto& p : proc.params) {
    if (object_alias_for_named_type(module, p.type)) {
      out[p.name] = &p.type;
    }
  }
  collect_object_local_types_r(module, proc.body, out);
  return out;
}

void collect_object_return_layout_r(const Module& module, const TypeExpr& te,
                                    const std::string& rel_path, std::vector<MirParam>& out) {
  const TypeAlias* ta = object_alias_for_named_type(module, te);
  if (!ta) {
    return;
  }
  for_each_object_field(module, *ta, [&](const TypeField& fld) {
    if (!fld.type) {
      return;
    }
    const std::string sub = rel_path.empty() ? fld.name : (rel_path + "_" + fld.name);
    if (object_alias_for_named_type(module, *fld.type)) {
      collect_object_return_layout_r(module, *fld.type, sub, out);
    } else {
      const TypeExpr* ut = unwrap_refinement_type(fld.type.get());
      if (!ut) {
        return;
      }
      if (ut->kind == TypeKind::Array && ut->array_size > 0 && ut->elem) {
        const TypeExpr* el = unwrap_refinement_type(ut->elem.get());
        if (el && el->kind == TypeKind::Named) {
          const bool iflt = is_float_type_name(el->name);
          const bool iint = el->name == "int";
          if (iflt || iint) {
            MirParam mp;
            mp.name = sub;
            mp.is_float = iflt;
            mp.fixed_array_elems = ut->array_size;
            out.push_back(std::move(mp));
          }
        }
        return;
      }
      if (ut->kind == TypeKind::Array) {
        return;
      }
      MirParam mp;
      mp.name = sub;
      mp.is_float = is_float_type_name(ut->name);
      mp.is_string = mir_ptr_param_type_name(ut->name);
      mp.is_i64 = is_i64_type_name(ut->name) || is_string_type_name(ut->name);
      out.push_back(std::move(mp));
    }
  });
}

std::string mir_field_slot_for_expr(const Expr& e) {
  const Expr* cur = &e;
  std::vector<std::string> fields_rev;
  while (cur && cur->kind == Expr::Kind::FieldAccess) {
    fields_rev.push_back(cur->field_name);
    cur = cur->base.get();
  }
  if (!cur || cur->kind != Expr::Kind::Ident) {
    return {};
  }
  std::string id = std::string("__li_o_") + cur->ident;
  for (auto it = fields_rev.rbegin(); it != fields_rev.rend(); ++it) {
    id += "_" + *it;
  }
  return id;
}

void append_mir_params_for_object_type(const Module& module, const TypeExpr& te,
                                       const std::string& path_prefix,
                                       std::vector<MirParam>& out_params, bool is_var) {
  const TypeAlias* ta = object_alias_for_named_type(module, te);
  if (!ta) {
    return;
  }
  for_each_object_field(module, *ta, [&](const TypeField& fld) {
    if (!fld.type) {
      return;
    }
    const std::string sub = path_prefix + "_" + fld.name;
    if (object_alias_for_named_type(module, *fld.type)) {
      append_mir_params_for_object_type(module, *fld.type, sub, out_params, is_var);
    } else {
      const TypeExpr* ut = unwrap_refinement_type(fld.type.get());
      if (!ut) {
        return;
      }
      if (ut->kind == TypeKind::Array && ut->array_size > 0 && ut->elem) {
        const TypeExpr* el = unwrap_refinement_type(ut->elem.get());
        if (el && el->kind == TypeKind::Named) {
          const bool iflt = is_float_type_name(el->name);
          const bool iint = el->name == "int";
          if (iflt || iint) {
            MirParam mp;
            mp.name = sub;
            mp.is_float = iflt;
            mp.fixed_array_elems = ut->array_size;
            mp.is_var = is_var;
            out_params.push_back(std::move(mp));
          }
        }
        return;
      }
      if (ut->kind == TypeKind::Array) {
        return;
      }
      MirParam mp;
      mp.name = sub;
      mp.is_float = is_float_type_name(ut->name);
      mp.is_string = mir_ptr_param_type_name(ut->name);
      mp.is_i64 = is_i64_type_name(ut->name) || is_string_type_name(ut->name);
      mp.is_var = is_var;
      out_params.push_back(std::move(mp));
    }
  });
}

void push_mir_args_for_object_value_r(const Module& module, const TypeExpr& te,
                                      const std::string& path_prefix,
                                      std::vector<MirArg>& args_out, bool is_var) {
  const TypeAlias* ta = object_alias_for_named_type(module, te);
  if (!ta) {
    return;
  }
  for_each_object_field(module, *ta, [&](const TypeField& fld) {
    if (!fld.type) {
      return;
    }
    const std::string sub = path_prefix + "_" + fld.name;
    if (object_alias_for_named_type(module, *fld.type)) {
      push_mir_args_for_object_value_r(module, *fld.type, sub, args_out, is_var);
    } else {
      MirArg ma;
      ma.ident = sub;
      ma.is_var_ref = is_var;
      const TypeExpr* ut = unwrap_refinement_type(fld.type.get());
      if (ut && ut->kind == TypeKind::Array && ut->array_size > 0) {
        ma.is_array_ident = true;
      }
      args_out.push_back(std::move(ma));
    }
  });
}

void push_mir_args_for_object_value(const Module& module, const TypeExpr& param_ty,
                                    const Expr& arg, std::vector<MirArg>& args_out,
                                    bool is_var, std::vector<MirInsn>& out,
                                    std::unordered_set<std::string>& float_names,
                                    std::unordered_set<std::string>& simd_names,
                                    std::unordered_set<std::string>& i64_locals) {
  if (arg.kind == Expr::Kind::Ident) {
    push_mir_args_for_object_value_r(module, param_ty, std::string("__li_o_") + arg.ident, args_out,
                                     is_var);
    return;
  }
  if (arg.kind == Expr::Kind::FieldAccess) {
    // FieldAccess on an object (e.g. `layout.viewport`): the caller's slots are
    // named `__li_o_<root>_<f1>_<f2>...`, so push those leaf slots directly.
    // Previously this fell through and the arg was silently dropped, under-
    // pushing the flattened by-value args (0 of N pushed).
    const std::string slot = mir_field_slot_for_expr(arg);
    if (!slot.empty()) {
      push_mir_args_for_object_value_r(module, param_ty, slot, args_out, is_var);
      return;
    }
    // Root isn't an Ident (e.g. `foo().viewport`): lower the whole expr first.
    const std::string dest = lower_expr_to(arg, module, out, float_names, simd_names, i64_locals);
    if (!dest.empty()) {
      push_mir_args_for_object_value_r(module, param_ty, dest, args_out, is_var);
    }
    return;
  }
  if (arg.kind == Expr::Kind::Call || arg.kind == Expr::Kind::MethodCall) {
    // A call (or method call) returning an object: lower it into its own
    // `__li_o___cr<N>_<field>` slots first, then push those leaf slots as the
    // flattened by-value args. Previously the arg was silently dropped, so the
    // call site under-pushed (e.g. 26 of 32 args for layout+selection).
    const std::string dest =
        lower_expr_to(arg, module, out, float_names, simd_names, i64_locals);
    if (!dest.empty()) {
      push_mir_args_for_object_value_r(module, param_ty, dest, args_out, is_var);
    }
    return;
  }
}

std::string object_root_ident(const Expr& e) {
  const Expr* cur = &e;
  while (cur && cur->kind == Expr::Kind::FieldAccess) {
    cur = cur->base.get();
  }
  if (cur && cur->kind == Expr::Kind::Ident) {
    return cur->ident;
  }
  return {};
}

bool first_param_is_var_object(const Module& module, const ProcDecl& callee, const TypeExpr** out_ty) {
  if (callee.params.empty()) {
    return false;
  }
  const Param& p = callee.params[0];
  if (!p.type.is_var || !object_alias_for_named_type(module, p.type)) {
    return false;
  }
  if (out_ty) {
    *out_ty = &p.type;
  }
  return true;
}

void push_mir_args_for_object_prefix(const Module& module, const TypeExpr& param_ty,
                                     const std::string& slot_prefix,
                                     std::vector<MirArg>& args_out, bool is_var) {
  push_mir_args_for_object_value_r(module, param_ty, slot_prefix, args_out, is_var);
}

std::string lower_callproc_with_optional_inout(
    const ProcDecl& callee, const std::string& callee_name, const Expr* receiver_or_first_arg,
    const std::vector<std::unique_ptr<Expr>>* extra_args, bool method_call, const Module& module,
    std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
    std::unordered_set<std::string>& simd_names, std::unordered_set<std::string>& i64_locals) {
  const TypeExpr* inout_ty = nullptr;
  const std::string recv_ident =
      receiver_or_first_arg ? object_root_ident(*receiver_or_first_arg) : std::string{};
  const bool inout =
      !recv_ident.empty() && first_param_is_var_object(module, callee, &inout_ty) && inout_ty;
  std::string wb_prefix;
  if (inout) {
    wb_prefix = std::string("__li_o_wb") + std::to_string(temp_counter++);
    emit_object_slots_r(module, *inout_ty, wb_prefix, out, float_names, i64_locals);
    emit_copy_object_slots_r(module, *inout_ty, std::string("__li_o_") + recv_ident, wb_prefix, out,
                             float_names, i64_locals);
  }
  MirInsn ins;
  ins.op = MirOp::CallProc;
  ins.callee = callee_name;
  for (std::size_t ai = 0; ai < callee.params.size(); ++ai) {
    const Param& fp = callee.params[ai];
    const Expr* arg = nullptr;
    if (method_call) {
      arg = (ai == 0) ? receiver_or_first_arg
                        : (extra_args && ai - 1 < extra_args->size() ? (*extra_args)[ai - 1].get()
                                                                      : nullptr);
    } else if (extra_args && ai < extra_args->size()) {
      arg = (*extra_args)[ai].get();
    }
    if (!arg) {
      continue;
    }
    if (object_alias_for_named_type(module, fp.type)) {
      if (inout && ai == 0) {
        push_mir_args_for_object_prefix(module, fp.type, wb_prefix, ins.args, true);
      } else {
        push_mir_args_for_object_value(module, fp.type, *arg, ins.args, fp.type.is_var, out,
                                       float_names, simd_names, i64_locals);
      }
      continue;
    }
    MirArg ma;
    if (arg->kind == Expr::Kind::StringLit) {
      ma.is_string = true;
      ma.str_value = arg->str_value;
    } else if (arg->kind == Expr::Kind::IntLit) {
      ma.is_literal = true;
      ma.int_value = arg->int_value;
    } else if (arg->kind == Expr::Kind::FloatLit) {
      ma.is_float_literal = true;
      ma.float_value = arg->float_value;
    } else if (arg->kind == Expr::Kind::Ident) {
      ma.ident = arg->ident;
      const TypeExpr* fpt = unwrap_refinement_type(&fp.type);
      if (fpt && fpt->kind == TypeKind::Array && fpt->array_size > 0) {
        ma.is_array_ident = true;
      }
    } else {
      ma.ident = lower_expr_to(*arg, module, out, float_names, simd_names, i64_locals);
    }
    ins.args.push_back(std::move(ma));
  }
  const bool callee_ret_obj =
      callee.ret_type && object_alias_for_named_type(module, *callee.ret_type);
  std::string dest;
  if (callee_ret_obj) {
    dest = std::string("__li_o___cr") + std::to_string(temp_counter++);
    emit_object_slots_r(module, *callee.ret_type, dest, out, float_names, i64_locals);
    collect_object_return_layout_r(module, *callee.ret_type, "", ins.object_layout);
  } else if (callee.ret_type && callee.ret_type->name == "unit") {
    dest.clear();
  } else {
    dest = fresh_temp();
  }
  ins.ident = dest;
  if (!callee_ret_obj && callee.ret_type && is_float_type_name(callee.ret_type->name)) {
    ins.ret_is_float = true;
    float_names.insert(dest);
  } else if (!callee_ret_obj && callee.ret_type &&
             (is_i64_type_name(callee.ret_type->name) ||
              mir_ptr_param_type_name(callee.ret_type->name))) {
    ins.ret_is_i64 = true;
    i64_locals.insert(dest);
  }
  out.push_back(std::move(ins));
  if (inout && inout_ty) {
    emit_copy_object_slots_r(module, *inout_ty, wb_prefix, std::string("__li_o_") + recv_ident, out,
                             float_names, i64_locals);
  }
  return dest.empty() ? std::string{} : dest;
}

void seed_float_params(const MirFn& fn, std::unordered_set<std::string>& float_names) {
  for (const auto& p : fn.params) {
    if (p.is_float && p.fixed_array_elems == 0) {
      float_names.insert(p.name);
    }
  }
}

void seed_float_array_params(const MirFn& fn) {
  for (const auto& p : fn.params) {
    // 1D float arrays only — 2D matrices live in matrix_names and are lowered
    // through the dedicated matmul/matrix-index paths.
    if (p.is_float && p.fixed_array_elems > 0 && !p.is_matrix && g_arr_ctx &&
        g_arr_ctx->float_array_names) {
      g_arr_ctx->float_array_names->insert(p.name);
      g_arr_ctx->float_array_sizes[p.name] = p.fixed_array_elems;
    }
  }
}

void seed_i64_params(const MirFn& fn, std::unordered_set<std::string>& i64_locals) {
  for (const auto& p : fn.params) {
    if ((p.is_i64 || p.is_string) && p.fixed_array_elems == 0) {
      i64_locals.insert(p.name);
    }
  }
}

std::string lower_expr_to(const Expr& e, const Module& module, std::vector<MirInsn>& out,
                          std::unordered_set<std::string>& float_names,
                          std::unordered_set<std::string>& simd_names,
                          std::unordered_set<std::string>& i64_locals) {
  switch (e.kind) {
    case Expr::Kind::IntLit: {
      const std::string dest = fresh_temp();
      MirInsn ins;
      ins.op = MirOp::StoreInt;
      ins.ident = dest;
      ins.rhs_is_literal = true;
      ins.rhs_int = e.int_value;
      out.push_back(std::move(ins));
      return dest;
    }
    case Expr::Kind::FloatLit: {
      const std::string dest = fresh_temp();
      MirInsn ins;
      ins.op = MirOp::StoreFloat;
      ins.ident = dest;
      ins.rhs_is_literal = true;
      ins.float_value = e.float_value;
      out.push_back(std::move(ins));
      float_names.insert(dest);
      return dest;
    }
    case Expr::Kind::Ident:
      return e.ident;
    case Expr::Kind::Await: {
      if (!e.operand) {
        return fresh_temp();
      }
      const std::string pending =
          lower_expr_to(*e.operand, module, out, float_names, simd_names, i64_locals);
      const std::string dest = fresh_temp();
      MirInsn await;
      await.op = MirOp::AsyncAwait;
      await.ident = dest;
      await.lhs_ident = pending;
      out.push_back(std::move(await));
      return dest;
    }
    case Expr::Kind::BinOp: {
      if (e.bin_op == BinOp::MatMul && e.lhs && e.lhs->kind == Expr::Kind::Ident && e.rhs &&
          e.rhs->kind == Expr::Kind::Ident && g_arr_ctx) {
        MatrixDims da;
        MatrixDims db;
        if (g_arr_ctx->matrix_names && g_arr_ctx->matrix_names->count(e.lhs->ident) > 0 &&
            g_arr_ctx->matrix_names->count(e.rhs->ident) > 0 &&
            matrix_slot_dims(e.lhs->ident, &da) && matrix_slot_dims(e.rhs->ident, &db) &&
            da.cols == db.rows) {
          const std::string dest = fresh_temp();
          MirInsn alloc;
          alloc.op = MirOp::ArrayAlloc;
          alloc.ident = dest;
          alloc.int_value = da.rows;
          alloc.rhs_int = db.cols;
          alloc.array_is_matrix = true;
          alloc.array_is_float = true;
          out.push_back(std::move(alloc));
          if (g_arr_ctx->matrix_names) {
            g_arr_ctx->matrix_names->insert(dest);
          }
          g_arr_ctx->matrix_dims[dest] = MatrixDims{da.rows, db.cols};
          MirInsn mm;
          mm.op = MirOp::ArrayMatMul2DF64;
          mm.ident = dest;
          mm.lhs_ident = e.lhs->ident;
          mm.rhs_ident = e.rhs->ident;
          mm.int_value = da.rows;
          mm.rhs_int = da.cols;
          mm.lhs_int = db.cols;
          out.push_back(std::move(mm));
          return dest;
        }
        const std::string dest =
            lower_float_array_dot_f64(e.lhs->ident, e.rhs->ident, out, float_names);
        if (!dest.empty()) {
          return dest;
        }
      }
      if (e.bin_op == BinOp::Mul && e.lhs && e.rhs) {
        auto try_scale = [&](const Expr& scale, const Expr& arr) -> std::string {
          if (arr.kind != Expr::Kind::Ident) {
            return {};
          }
          if (scale.kind == Expr::Kind::FloatLit) {
            return lower_float_array_scale_expr(arr.ident, true, scale.float_value, {}, out);
          }
          if (scale.kind == Expr::Kind::Ident) {
            return lower_float_array_scale_expr(arr.ident, false, 0.0, scale.ident, out);
          }
          return {};
        };
        if (std::string dest = try_scale(*e.lhs, *e.rhs); !dest.empty()) {
          float_names.insert(dest);
          return dest;
        }
        if (std::string dest = try_scale(*e.rhs, *e.lhs); !dest.empty()) {
          float_names.insert(dest);
          return dest;
        }
      }
      if (is_array_elementwise_binop(e.bin_op) && e.lhs && e.lhs->kind == Expr::Kind::Ident &&
          e.rhs && e.rhs->kind == Expr::Kind::Ident) {
        const std::string dest =
            lower_array_elementwise_binop_expr(e.lhs->ident, e.rhs->ident, e.bin_op, out);
        if (!dest.empty()) {
          return dest;
        }
      }
      const std::string dest = fresh_temp();
      MirInsn ins;
      const bool cmp = is_cmp_binop(e.bin_op);
      const bool flt = is_float_expr(e, float_names) ||
                       (cmp && (is_float_expr(*e.lhs, float_names) ||
                                is_float_expr(*e.rhs, float_names)));
      ins.op = flt ? MirOp::BinOpFloat : MirOp::BinOpInt;
      ins.ident = dest;
      ins.rhs_is_literal = false;
      ins.lhs_is_literal = false;
      if (flt) {
        ins.lhs_ident =
            lower_expr_to(*e.lhs, module, out, float_names, simd_names, i64_locals);
        ins.rhs_ident =
            lower_expr_to(*e.rhs, module, out, float_names, simd_names, i64_locals);
      } else {
        if (e.lhs && e.lhs->kind == Expr::Kind::IntLit) {
          ins.lhs_is_literal = true;
          ins.lhs_int = e.lhs->int_value;
        } else {
          ins.lhs_ident =
              lower_expr_to(*e.lhs, module, out, float_names, simd_names, i64_locals);
        }
        if (e.rhs && e.rhs->kind == Expr::Kind::IntLit) {
          ins.rhs_is_literal = true;
          ins.rhs_int = e.rhs->int_value;
        } else {
          ins.rhs_ident =
              lower_expr_to(*e.rhs, module, out, float_names, simd_names, i64_locals);
        }
      }
      ins.bin_op = e.bin_op;
      out.push_back(std::move(ins));
      if (flt && !cmp) {
        float_names.insert(dest);
      }
      return dest;
    }
    case Expr::Kind::Call: {
      if (e.ident == "__li_simd_splat_f64" && e.args.size() == 1) {
        const std::string dest = fresh_temp();
        const std::string scalar = lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        MirInsn ins;
        ins.op = MirOp::SimdSplatF64;
        ins.ident = dest;
        ins.rhs_ident = scalar;
        ins.simd_lanes = 4;
        out.push_back(std::move(ins));
        simd_names.insert(dest);
        return dest;
      }
      if (e.ident == "__li_simd_mul_f64" && e.args.size() == 2) {
        const std::string dest = fresh_temp();
        const std::string lhs = lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        const std::string rhs = lower_expr_to(*e.args[1], module, out, float_names, simd_names, i64_locals);
        MirInsn ins;
        ins.op = MirOp::SimdMulF64;
        ins.ident = dest;
        ins.lhs_ident = lhs;
        ins.rhs_ident = rhs;
        ins.simd_lanes = 4;
        out.push_back(std::move(ins));
        simd_names.insert(dest);
        return dest;
      }
      if (e.ident == "__li_simd_add_f64" && e.args.size() == 2) {
        const std::string dest = fresh_temp();
        const std::string lhs = lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        const std::string rhs = lower_expr_to(*e.args[1], module, out, float_names, simd_names, i64_locals);
        MirInsn ins;
        ins.op = MirOp::SimdAddF64;
        ins.ident = dest;
        ins.lhs_ident = lhs;
        ins.rhs_ident = rhs;
        ins.simd_lanes = 4;
        out.push_back(std::move(ins));
        simd_names.insert(dest);
        return dest;
      }
      if (e.ident == "axpy" && e.args.size() == 3 && e.args[1]->kind == Expr::Kind::Ident &&
          e.args[2]->kind == Expr::Kind::Ident && g_arr_ctx) {
        const std::string& x = e.args[1]->ident;
        const std::string& y = e.args[2]->ident;
        bool lit = false;
        double fv = 0.0;
        std::string alpha_id;
        if (e.args[0]->kind == Expr::Kind::FloatLit) {
          lit = true;
          fv = e.args[0]->float_value;
        } else if (e.args[0]->kind == Expr::Kind::Ident) {
          alpha_id = lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        } else {
          alpha_id = lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        }
        if (emit_float_array_axpy(lit, fv, alpha_id, x, y, out)) {
          return fresh_temp();
        }
      }
      if (e.ident == "dot" && e.args.size() == 2 && e.args[0]->kind == Expr::Kind::Ident &&
          e.args[1]->kind == Expr::Kind::Ident) {
        const std::string dest =
            lower_float_array_dot_f64(e.args[0]->ident, e.args[1]->ident, out, float_names);
        if (!dest.empty()) {
          return dest;
        }
      }
      if (e.ident == "norm" && e.args.size() == 1 && e.args[0]->kind == Expr::Kind::Ident &&
          g_arr_ctx) {
        const std::string& arr = e.args[0]->ident;
        if (g_arr_ctx->float_array_names &&
            g_arr_ctx->float_array_names->count(arr) > 0) {
          const std::string dotv = lower_float_array_dot_f64(arr, arr, out, float_names);
          if (!dotv.empty()) {
            const std::string dest = fresh_temp();
            MirInsn ins;
            ins.op = MirOp::CallExtern;
            ins.callee = "li_rt_sqrt";
            MirArg ma;
            ma.ident = dotv;
            ins.args.push_back(std::move(ma));
            ins.ident = dest;
            out.push_back(std::move(ins));
            float_names.insert(dest);
            return dest;
          }
        }
        const auto ia = g_arr_ctx->int_array_sizes.find(arr);
        if (ia != g_arr_ctx->int_array_sizes.end()) {
          const std::string sq =
              lower_array_elementwise_binop_expr(arr, arr, BinOp::Mul, out);
          if (!sq.empty()) {
            const std::string dest = fresh_temp();
            MirInsn ins;
            ins.op = MirOp::ArraySumI64;
            ins.ident = dest;
            ins.lhs_ident = sq;
            ins.int_value = ia->second;
            out.push_back(std::move(ins));
            return dest;
          }
        }
      }
      if (e.ident == "sum" && e.args.size() == 1 && g_arr_ctx) {
        const std::string arr =
            lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        const std::string dest = fresh_temp();
        if (g_arr_ctx->float_array_names &&
            g_arr_ctx->float_array_names->count(arr) > 0) {
          const auto sz = g_arr_ctx->float_array_sizes.find(arr);
          if (sz != g_arr_ctx->float_array_sizes.end()) {
            MirInsn ins;
            ins.op = MirOp::ArraySumF64;
            ins.ident = dest;
            ins.lhs_ident = arr;
            ins.int_value = sz->second;
            out.push_back(std::move(ins));
            float_names.insert(dest);
            return dest;
          }
        }
        {
          const auto sz = g_arr_ctx->int_array_sizes.find(arr);
          if (sz != g_arr_ctx->int_array_sizes.end()) {
            MirInsn ins;
            ins.op = MirOp::ArraySumI64;
            ins.ident = dest;
            ins.lhs_ident = arr;
            ins.int_value = sz->second;
            out.push_back(std::move(ins));
            return dest;
          }
        }
      }
      if (e.ident == "__li_horiz_sum_f64" && e.args.size() == 1) {
        const std::string dest = fresh_temp();
        const std::string vec = lower_expr_to(*e.args[0], module, out, float_names, simd_names, i64_locals);
        MirInsn ins;
        ins.op = MirOp::SimdHorizSumF64;
        ins.ident = dest;
        ins.lhs_ident = vec;
        ins.simd_lanes = 4;
        out.push_back(std::move(ins));
        float_names.insert(dest);
        return dest;
      }
      const ProcDecl* callee = find_proc(module, e.ident);
      if (callee && !callee->is_extern) {
        const std::string dest = lower_callproc_with_optional_inout(
            *callee, e.ident, e.args.empty() ? nullptr : e.args[0].get(), &e.args, false, module,
            out, float_names, simd_names, i64_locals);
        return dest;
      }
      MirInsn ins;
      ins.op = MirOp::CallExtern;
      ins.callee = e.ident;
      if (g_mir_module) {
        mir_note_runtime_callee(e.ident, *g_mir_module);
      }
      for (const auto& arg : e.args) {
        MirArg ma;
        if (arg->kind == Expr::Kind::StringLit) {
          ma.is_string = true;
          ma.str_value = arg->str_value;
        } else if (arg->kind == Expr::Kind::IntLit) {
          ma.is_literal = true;
          ma.int_value = arg->int_value;
        } else if (arg->kind == Expr::Kind::Ident) {
          ma.ident = arg->ident;
        } else {
          ma.ident = lower_expr_to(*arg, module, out, float_names, simd_names, i64_locals);
        }
        ins.args.push_back(std::move(ma));
      }
      const bool std_int_extern =
          !callee && is_std_module_symbol(e.ident) && std_extern_returns_int(e.ident);
      const bool std_str_extern =
          !callee && is_std_module_symbol(e.ident) && std_extern_returns_str(e.ident);
      const bool extern_with_value =
          (callee && callee->is_extern && callee->ret_type && callee->ret_type->name != "unit") ||
          std_int_extern || std_str_extern;
      if (extern_with_value) {
        const std::string dest = fresh_temp();
        ins.ident = dest;
        if (std_str_extern || (callee && callee->ret_type &&
                               (callee->ret_type->name == "ptr" || callee->ret_type->name == "str" ||
                                callee->ret_type->name == "string" || callee->ret_type->name == "bytes" ||
                                callee->ret_type->name == "StringView" ||
                                callee->ret_type->name == "int64" || callee->ret_type->name == "i64"))) {
          ins.is_i64 = true;
          ins.ret_is_i64 = true;
          i64_locals.insert(dest);
        } else if (callee && callee->ret_type && is_float_type_name(callee->ret_type->name)) {
          ins.ret_is_float = true;
          float_names.insert(dest);
        }
        out.push_back(std::move(ins));
        return dest;
      }
      out.push_back(std::move(ins));
      return fresh_temp();
    }
    case Expr::Kind::MethodCall: {
      if (!e.base) {
        return fresh_temp();
      }
      const std::string type_name = object_type_name_from_receiver(*e.base);
      if (type_name.empty()) {
        return fresh_temp();
      }
      const std::string callee = mangle_method_callee(type_name, e.field_name);
      const ProcDecl* callee_proc = find_proc(module, callee);
      if (!callee_proc || callee_proc->is_extern) {
        return fresh_temp();
      }
      const std::string dest = lower_callproc_with_optional_inout(
          *callee_proc, callee, e.base.get(), &e.args, true, module, out, float_names, simd_names,
          i64_locals);
      return dest;
    }
    case Expr::Kind::FieldAccess: {
      const std::string slot = mir_field_slot_for_expr(e);
      if (!slot.empty()) {
        return slot;
      }
      return fresh_temp();
    }
    case Expr::Kind::Index: {
      if (e.index && e.base && e.base->kind == Expr::Kind::Index && e.base->base &&
          e.base->index) {
        std::string mat_ident;
        if (e.base->base->kind == Expr::Kind::Ident) {
          mat_ident = e.base->base->ident;
        } else if (e.base->base->kind == Expr::Kind::FieldAccess) {
          mat_ident = mir_field_slot_for_expr(*e.base->base);
        }
        if (!mat_ident.empty() && g_arr_ctx && g_arr_ctx->matrix_names &&
            g_arr_ctx->matrix_names->count(mat_ident) > 0) {
          MirInsn load;
          load.op = MirOp::ArrayLoad2DF64;
          load.ident = mat_ident;
          if (!parse_matrix_index_pair(*e.base->index, *e.index, load)) {
            return fresh_temp();
          }
          const std::string dest = fresh_temp();
          load.lhs_ident = dest;
          out.push_back(std::move(load));
          float_names.insert(dest);
          return dest;
        }
      }
      if (e.index) {
        std::string arr_ident;
        if (e.base && e.base->kind == Expr::Kind::Ident) {
          arr_ident = e.base->ident;
        } else if (e.base && e.base->kind == Expr::Kind::FieldAccess) {
          arr_ident = mir_field_slot_for_expr(*e.base);
        }
        if (!arr_ident.empty()) {
          MirInsn load;
          load.op = MirOp::ArrayLoadInt;
          load.ident = arr_ident;
          if (e.index->kind == Expr::Kind::IntLit) {
            load.index_is_literal = true;
            load.int_value = e.index->int_value;
          } else if (e.index->kind == Expr::Kind::Ident) {
            load.index_is_literal = false;
            load.index_ident = e.index->ident;
          } else {
            // General index expression (`a[n[0]]`, `a[helper(x)]`): lower it to
            // an int temp so emit's ArrayLoadInt reads the right slot.
            load.index_is_literal = false;
            load.index_ident = lower_expr_to(*e.index, module, out, float_names,
                                             simd_names, i64_locals);
          }
          const std::string dest = fresh_temp();
          load.lhs_ident = dest;
          out.push_back(std::move(load));
          // Float array elements (including rows of float matrices) yield
          // float temps so downstream BinOp lowering keeps float semantics.
          if (is_float_expr(e, float_names)) {
            float_names.insert(dest);
          }
          return dest;
        }
      }
      return fresh_temp();
    }
    default:
      return fresh_temp();
  }
}

void lower_print_arg(const Expr& arg, const Module& module, std::vector<MirInsn>& out,
                     std::unordered_set<std::string>& float_names,
                     std::unordered_set<std::string>& simd_names,
                     std::unordered_set<std::string>& i64_locals) {
  MirInsn ins;
  if (arg.kind == Expr::Kind::IntLit) {
    ins.op = MirOp::EchoInt;
    ins.int_value = arg.int_value;
  } else if (arg.kind == Expr::Kind::Ident) {
    ins.op = MirOp::EchoInt;
    ins.ident = arg.ident;
  } else if (arg.kind == Expr::Kind::StringLit) {
    ins.op = MirOp::EchoString;
    ins.str_value = arg.str_value;
  } else {
    // General expression (call, binop, ...): lower it to a temp and echo the
    // temp so `print(f(x))` and `print(a + b)` work like any other argument.
    const std::string tmp =
        lower_expr_to(arg, module, out, float_names, simd_names, i64_locals);
    ins.op = MirOp::EchoInt;
    ins.ident = tmp;
  }
  out.push_back(std::move(ins));
}

void lower_return_expr(const Expr& e, const LowerCtx& ctx, bool returns_float,
                       const Module& module, std::vector<MirInsn>& out,
                       std::unordered_set<std::string>& float_names,
                       std::unordered_set<std::string>& simd_names,
                       std::unordered_set<std::string>& i64_locals) {
  const bool ret_obj =
      ctx.proc && ctx.proc->ret_type && object_alias_for_named_type(module, *ctx.proc->ret_type);
  MirInsn ins;
  if (e.kind == Expr::Kind::IntLit) {
    ins.op = MirOp::ReturnInt;
    ins.int_value = e.int_value;
  } else if (e.kind == Expr::Kind::FloatLit) {
    ins.op = MirOp::ReturnFloat;
    ins.float_value = e.float_value;
  } else if (e.kind == Expr::Kind::Ident) {
    if (ret_obj) {
      ins.op = MirOp::ReturnObject;
      ins.ident = std::string("__li_o_") + e.ident;
      collect_object_return_layout_r(module, *ctx.proc->ret_type, "", ins.object_layout);
    } else {
      ins.op = MirOp::ReturnIdent;
      ins.ident = e.ident;
      ins.ret_is_float = returns_float || float_names.count(e.ident) > 0;
      ins.ret_is_i64 =
          i64_locals.count(e.ident) > 0 ||
          (ctx.proc && ctx.proc->ret_type && is_i64_type_name(ctx.proc->ret_type->name));
    }
  } else if (e.kind == Expr::Kind::Call || e.kind == Expr::Kind::MethodCall ||
             e.kind == Expr::Kind::BinOp || e.kind == Expr::Kind::Index ||
             e.kind == Expr::Kind::FieldAccess) {
    const std::string tmp = lower_expr_to(e, module, out, float_names, simd_names, i64_locals);
    if (ret_obj) {
      ins.op = MirOp::ReturnObject;
      ins.ident = tmp;
      collect_object_return_layout_r(module, *ctx.proc->ret_type, "", ins.object_layout);
    } else {
      ins.op = MirOp::ReturnIdent;
      ins.ident = tmp;
      ins.ret_is_float = returns_float || is_float_expr(e, float_names);
      ins.ret_is_i64 =
          i64_locals.count(tmp) > 0 ||
          (ctx.proc && ctx.proc->ret_type && is_i64_type_name(ctx.proc->ret_type->name));
    }
  } else {
    ins.op = MirOp::ReturnVoid;
  }
  out.push_back(std::move(ins));
}

void append_implicit_return(std::vector<MirInsn>& body);

void lower_stmts(const std::vector<Stmt>& stmts, LowerCtx& ctx, bool returns_float,
                 std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                 std::unordered_set<std::string>& simd_names,
                 std::unordered_set<std::string>& float_array_names,
                 std::unordered_set<std::string>& i64_locals);

void lower_stmt(const Stmt& stmt, LowerCtx& ctx, bool returns_float, std::vector<MirInsn>& out,
                std::unordered_set<std::string>& float_names,
                std::unordered_set<std::string>& simd_names,
                std::unordered_set<std::string>& float_array_names,
                std::unordered_set<std::string>& i64_locals) {
  const Module& module = *ctx.module;
  switch (stmt.kind) {
    case Stmt::Kind::Return:
      if (!stmt.expr) {
        MirInsn ins;
        ins.op = MirOp::ReturnVoid;
        out.push_back(std::move(ins));
      } else {
        lower_return_expr(*stmt.expr, ctx, returns_float, module, out, float_names, simd_names,
                          i64_locals);
      }
      break;
    case Stmt::Kind::VarDecl: {
      if (is_simd_type(stmt.var_type)) {
        MirInsn ins;
        ins.op = MirOp::LocalAllocSimdF64;
        ins.ident = stmt.var_name;
        ins.simd_lanes = simd_lanes_from_type(stmt.var_type);
        out.push_back(std::move(ins));
        simd_names.insert(stmt.var_name);
      } else if (stmt.var_type.kind == TypeKind::Array && stmt.var_type.elem) {
        std::int64_t m_rows = 0;
        std::int64_t m_cols = 0;
        if (is_2d_float_matrix_type(stmt.var_type, &m_rows, &m_cols)) {
          MirInsn ins;
          ins.op = MirOp::ArrayAlloc;
          ins.ident = stmt.var_name;
          ins.int_value = m_rows;
          ins.rhs_int = m_cols;
          ins.array_is_matrix = true;
          ins.array_is_float = true;
          out.push_back(std::move(ins));
          if (g_arr_ctx) {
            if (g_arr_ctx->matrix_names) {
              g_arr_ctx->matrix_names->insert(stmt.var_name);
            }
            g_arr_ctx->matrix_dims[stmt.var_name] = MatrixDims{m_rows, m_cols};
          }
          if (stmt.init && stmt.init->kind == Expr::Kind::BinOp &&
              stmt.init->bin_op == BinOp::MatMul && stmt.init->lhs &&
              stmt.init->lhs->kind == Expr::Kind::Ident && stmt.init->rhs &&
              stmt.init->rhs->kind == Expr::Kind::Ident) {
            MatrixDims da;
            MatrixDims db;
            if (matrix_slot_dims(stmt.init->lhs->ident, &da) &&
                matrix_slot_dims(stmt.init->rhs->ident, &db) && da.cols == db.rows &&
                da.rows == m_rows && db.cols == m_cols) {
              MirInsn mm;
              mm.op = MirOp::ArrayMatMul2DF64;
              mm.ident = stmt.var_name;
              mm.lhs_ident = stmt.init->lhs->ident;
              mm.rhs_ident = stmt.init->rhs->ident;
              mm.int_value = m_rows;
              mm.rhs_int = da.cols;
              mm.lhs_int = m_cols;
              out.push_back(std::move(mm));
            }
          }
        } else {
          MirInsn ins;
          ins.op = MirOp::ArrayAlloc;
          ins.ident = stmt.var_name;
          ins.int_value = stmt.var_type.array_size;
          ins.array_is_float = is_float_array_type(stmt.var_type);
          out.push_back(std::move(ins));
          if (ins.array_is_float) {
            float_array_names.insert(stmt.var_name);
            if (g_arr_ctx) {
              g_arr_ctx->float_array_sizes[stmt.var_name] = stmt.var_type.array_size;
            }
          } else if (stmt.var_type.elem && stmt.var_type.elem->kind == TypeKind::Named &&
                     stmt.var_type.elem->name == "int" && g_arr_ctx) {
            g_arr_ctx->int_array_sizes[stmt.var_name] = stmt.var_type.array_size;
          }
          if (stmt.init && stmt.init->kind == Expr::Kind::BinOp &&
              is_array_elementwise_binop(stmt.init->bin_op) && stmt.init->lhs &&
              stmt.init->lhs->kind == Expr::Kind::Ident && stmt.init->rhs &&
              stmt.init->rhs->kind == Expr::Kind::Ident) {
            emit_array_elementwise_binop(stmt.var_name, stmt.init->lhs->ident,
                                         stmt.init->rhs->ident, stmt.init->bin_op, out);
          } else if (stmt.init && stmt.init->kind == Expr::Kind::BinOp &&
                     stmt.init->bin_op == BinOp::Mul && ins.array_is_float) {
            auto try_init_scale = [&](const Expr& scale, const Expr& arr) -> bool {
              if (arr.kind != Expr::Kind::Ident) {
                return false;
              }
              if (scale.kind == Expr::Kind::FloatLit) {
                return emit_float_array_scale(stmt.var_name, arr.ident, true, scale.float_value, {},
                                              out);
              }
              if (scale.kind == Expr::Kind::Ident) {
                return emit_float_array_scale(stmt.var_name, arr.ident, false, 0.0, scale.ident,
                                              out);
              }
              return false;
            };
            if (stmt.init->lhs && stmt.init->rhs &&
                (try_init_scale(*stmt.init->lhs, *stmt.init->rhs) ||
                 try_init_scale(*stmt.init->rhs, *stmt.init->lhs))) {
            } else if (stmt.init) {
              (void)lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
            }
          } else if (stmt.init) {
            (void)lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
          }
        }
      } else if (is_i64_type_name(stmt.var_type.name) ||
                 mir_ptr_param_type_name(stmt.var_type.name)) {
        MirInsn ins;
        ins.op = MirOp::LocalAllocI64;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        i64_locals.insert(stmt.var_name);
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreI64;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::StringLit) {
            store.rhs_is_string = true;
            store.str_value = stmt.init->str_value;
          } else if (stmt.init->kind == Expr::Kind::IntLit) {
            store.rhs_is_literal = true;
            store.rhs_int = stmt.init->int_value;
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp =
                lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      } else if (is_float_type_name(stmt.var_type.name)) {
        MirInsn ins;
        ins.op = MirOp::LocalAllocFloat;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        float_names.insert(stmt.var_name);
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreFloat;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::FloatLit) {
            store.rhs_is_literal = true;
            store.float_value = stmt.init->float_value;
            if (ctx.const_floats) {
              (*ctx.const_floats)[stmt.var_name] = stmt.init->float_value;
            }
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp =
                lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      } else if (object_alias_for_named_type(module, stmt.var_type)) {
        const std::string dst_base = std::string("__li_o_") + stmt.var_name;
        emit_object_slots_r(module, stmt.var_type, dst_base, out, float_names, i64_locals);
        if (stmt.init && stmt.init->kind == Expr::Kind::Ident) {
          emit_copy_object_slots_r(module, stmt.var_type, std::string("__li_o_") + stmt.init->ident,
                                   dst_base, out, float_names, i64_locals);
        } else if (stmt.init && stmt.init->kind == Expr::Kind::Call) {
          const ProcDecl* c = find_proc(module, stmt.init->ident);
          if (c && c->ret_type && object_alias_for_named_type(module, *c->ret_type)) {
            const std::string tmp =
                lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
            emit_copy_object_slots_r(module, stmt.var_type, tmp, dst_base, out, float_names,
                                     i64_locals);
          } else {
            (void)lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
          }
        } else if (stmt.init) {
          (void)lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
        }
      } else {
        MirInsn ins;
        ins.op = MirOp::LocalAllocInt;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreInt;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::IntLit) {
            store.rhs_is_literal = true;
            store.rhs_int = stmt.init->int_value;
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp =
                lower_expr_to(*stmt.init, module, out, float_names, simd_names, i64_locals);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      }
      break;
    }
    case Stmt::Kind::Assign:
      if (stmt.init && stmt.init->kind == Expr::Kind::Index && stmt.init->index && stmt.expr) {
        if (stmt.init->base && stmt.init->base->kind == Expr::Kind::Index &&
            stmt.init->base->index && stmt.init->base->base) {
          std::string mat_slot;
          if (stmt.init->base->base->kind == Expr::Kind::Ident) {
            mat_slot = stmt.init->base->base->ident;
          } else if (stmt.init->base->base->kind == Expr::Kind::FieldAccess) {
            mat_slot = mir_field_slot_for_expr(*stmt.init->base->base);
          }
          if (!mat_slot.empty() && g_arr_ctx && g_arr_ctx->matrix_names &&
              g_arr_ctx->matrix_names->count(mat_slot) > 0) {
            MirInsn ins;
            ins.op = MirOp::ArrayStore2DF64;
            ins.ident = mat_slot;
            if (!parse_matrix_index_pair(*stmt.init->base->index, *stmt.init->index, ins)) {
              break;
            }
            if (stmt.expr->kind == Expr::Kind::FloatLit) {
              ins.lhs_is_literal = true;
              ins.float_value = stmt.expr->float_value;
            } else if (stmt.expr->kind == Expr::Kind::Ident) {
              ins.lhs_is_literal = false;
              ins.lhs_ident = stmt.expr->ident;
            } else {
              ins.lhs_ident =
                  lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
              ins.lhs_is_literal = false;
            }
            out.push_back(std::move(ins));
            break;
          }
        }
        std::string arr_slot;
        if (stmt.init->base && stmt.init->base->kind == Expr::Kind::Ident) {
          arr_slot = stmt.init->base->ident;
        } else if (stmt.init->base && stmt.init->base->kind == Expr::Kind::FieldAccess) {
          arr_slot = mir_field_slot_for_expr(*stmt.init->base);
        }
        if (!arr_slot.empty()) {
          const bool fa = float_array_names.count(arr_slot) > 0;
          MirInsn ins;
          ins.op = fa ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
          ins.ident = arr_slot;
          if (stmt.init->index && stmt.init->index->kind == Expr::Kind::IntLit) {
            ins.index_is_literal = true;
            ins.int_value = stmt.init->index->int_value;
          } else if (stmt.init->index && stmt.init->index->kind == Expr::Kind::Ident) {
            ins.index_is_literal = false;
            ins.index_ident = stmt.init->index->ident;
          } else if (stmt.init->index) {
            // General index expression (`a[n[0]] = v`): lower it to an int temp
            // so emit's ArrayStoreInt writes the right slot.
            ins.index_is_literal = false;
            ins.index_ident =
                lower_expr_to(*stmt.init->index, module, out, float_names, simd_names,
                              i64_locals);
          }
          if (fa && stmt.expr->kind == Expr::Kind::FloatLit) {
            ins.rhs_is_literal = true;
            ins.float_value = stmt.expr->float_value;
          } else if (!fa && stmt.expr->kind == Expr::Kind::IntLit) {
            ins.rhs_is_literal = true;
            ins.rhs_int = stmt.expr->int_value;
          } else if (stmt.expr->kind == Expr::Kind::Ident) {
            ins.rhs_is_literal = false;
            ins.rhs_ident = stmt.expr->ident;
          } else {
            ins.rhs_ident =
                lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
            ins.rhs_is_literal = false;
          }
          out.push_back(std::move(ins));
        }
      } else if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr &&
                 stmt.expr->kind == Expr::Kind::BinOp &&
                 is_array_elementwise_binop(stmt.expr->bin_op) && stmt.expr->lhs &&
                 stmt.expr->lhs->kind == Expr::Kind::Ident && stmt.expr->rhs &&
                 stmt.expr->rhs->kind == Expr::Kind::Ident &&
                 emit_array_elementwise_binop(stmt.init->ident, stmt.expr->lhs->ident,
                                            stmt.expr->rhs->ident, stmt.expr->bin_op, out)) {
        break;
      } else if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr &&
                 stmt.expr->kind == Expr::Kind::BinOp && stmt.expr->bin_op == BinOp::MatMul &&
                 g_arr_ctx && g_arr_ctx->matrix_names &&
                 g_arr_ctx->matrix_names->count(stmt.init->ident) > 0 && stmt.expr->lhs &&
                 stmt.expr->lhs->kind == Expr::Kind::Ident && stmt.expr->rhs &&
                 stmt.expr->rhs->kind == Expr::Kind::Ident) {
        MatrixDims dc;
        MatrixDims da;
        MatrixDims db;
        if (matrix_slot_dims(stmt.init->ident, &dc) &&
            matrix_slot_dims(stmt.expr->lhs->ident, &da) &&
            matrix_slot_dims(stmt.expr->rhs->ident, &db) && da.cols == db.rows &&
            dc.rows == da.rows && dc.cols == db.cols) {
          MirInsn mm;
          mm.op = MirOp::ArrayMatMul2DF64;
          mm.ident = stmt.init->ident;
          mm.lhs_ident = stmt.expr->lhs->ident;
          mm.rhs_ident = stmt.expr->rhs->ident;
          mm.int_value = dc.rows;
          mm.rhs_int = da.cols;
          mm.lhs_int = dc.cols;
          out.push_back(std::move(mm));
          break;
        }
      } else if (stmt.init && stmt.init->kind == Expr::Kind::FieldAccess && stmt.expr) {
        const std::string slot = mir_field_slot_for_expr(*stmt.init);
        if (!slot.empty()) {
          const bool flt = float_names.count(slot) > 0;
          const bool i64s = i64_locals.count(slot) > 0;
          MirInsn ins;
          if (i64s) {
            ins.op = MirOp::StoreI64;
          } else {
            ins.op = flt ? MirOp::StoreFloat : MirOp::StoreInt;
          }
          ins.ident = slot;
          if (i64s && stmt.expr->kind == Expr::Kind::IntLit) {
            ins.rhs_is_literal = true;
            ins.rhs_int = stmt.expr->int_value;
          } else if (!flt && !i64s && stmt.expr->kind == Expr::Kind::IntLit) {
            ins.rhs_is_literal = true;
            ins.rhs_int = stmt.expr->int_value;
          } else if (flt && stmt.expr->kind == Expr::Kind::FloatLit) {
            ins.rhs_is_literal = true;
            ins.float_value = stmt.expr->float_value;
          } else if (stmt.expr->kind == Expr::Kind::Ident) {
            ins.rhs_is_literal = false;
            ins.rhs_ident = stmt.expr->ident;
          } else {
            ins.rhs_ident =
                lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
            ins.rhs_is_literal = false;
          }
          out.push_back(std::move(ins));
        }
      } else if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr) {
        if (ctx.object_locals) {
          const auto ol_it = ctx.object_locals->find(stmt.init->ident);
          if (ol_it != ctx.object_locals->end()) {
            const TypeExpr& oty = *ol_it->second;
            const std::string dst_base = std::string("__li_o_") + stmt.init->ident;
            if (stmt.expr->kind == Expr::Kind::Ident) {
              emit_copy_object_slots_r(module, oty, std::string("__li_o_") + stmt.expr->ident,
                                       dst_base, out, float_names, i64_locals);
              break;
            }
            if (stmt.expr->kind == Expr::Kind::Call) {
              const ProcDecl* c = find_proc(module, stmt.expr->ident);
              if (c && c->ret_type && object_alias_for_named_type(module, *c->ret_type)) {
                const std::string tmp =
                    lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
                emit_copy_object_slots_r(module, oty, tmp, dst_base, out, float_names, i64_locals);
                break;
              }
            }
            (void)lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
            break;
          }
        }
        if (simd_names.count(stmt.init->ident) > 0) {
          const std::string tmp =
              lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
          MirInsn ins;
          ins.op = MirOp::SimdCopyF64;
          ins.ident = stmt.init->ident;
          ins.rhs_ident = tmp;
          ins.simd_lanes = 4;
          out.push_back(std::move(ins));
          break;
        }
        const bool flt = float_names.count(stmt.init->ident) > 0;
        if (flt && stmt.expr->kind == Expr::Kind::BinOp &&
            try_emit_fma_float_assign(*stmt.expr, stmt.init->ident, out)) {
          break;
        }
        MirInsn ins;
        ins.op = flt ? MirOp::StoreFloat : MirOp::StoreInt;
        ins.ident = stmt.init->ident;
        if (stmt.expr->kind == Expr::Kind::IntLit && !flt) {
          ins.rhs_is_literal = true;
          ins.rhs_int = stmt.expr->int_value;
        } else if (stmt.expr->kind == Expr::Kind::FloatLit && flt) {
          ins.rhs_is_literal = true;
          ins.float_value = stmt.expr->float_value;
        } else if (stmt.expr->kind == Expr::Kind::Ident) {
          ins.rhs_is_literal = false;
          ins.rhs_ident = stmt.expr->ident;
        } else {
          ins.rhs_ident = lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
          ins.rhs_is_literal = false;
        }
        out.push_back(std::move(ins));
      }
      break;
    case Stmt::Kind::ParallelFor: {
      if (!ctx.mir) {
        break;
      }
      const std::string par_name =
          "__li_par_" + ctx.proc_name + "_" + std::to_string(par_counter++);
      MirFn par_fn;
      par_fn.name = par_name;
      copy_decorators(stmt.decorators, par_fn.decorators);
      MirParam ip;
      ip.name = stmt.par_iter;
      ip.is_i64 = true;
      par_fn.params.push_back(ip);
      std::unordered_set<std::string> par_floats;
      std::unordered_set<std::string> par_simd;
      std::unordered_set<std::string> par_float_arrays;
      std::unordered_set<std::string> par_i64s;
      LowerCtx par_ctx{ctx.module, ctx.mir, par_name, nullptr, nullptr, ctx.object_locals};
      lower_stmts(stmt.par_body, par_ctx, false, par_fn.body, par_floats, par_simd,
                  par_float_arrays, par_i64s);
      append_implicit_return(par_fn.body);
      ctx.mir->functions.push_back(std::move(par_fn));
      MirInsn call;
      call.op = MirOp::OmpParallelFor;
      call.callee = par_name;
      call.int_value = stmt.par_start;
      call.rhs_int = stmt.par_end;
      out.push_back(std::move(call));
      ctx.mir->uses_openmp = true;
      break;
    }
    case Stmt::Kind::If: {
      if (!stmt.cond) {
        break;
      }
      const std::string cond_tmp =
          lower_expr_to(*stmt.cond, module, out, float_names, simd_names, i64_locals);
      const std::string else_label = fresh_label("else_");
      const std::string merge_label = fresh_label("merge_");
      push_branch_if_zero(out, cond_tmp, else_label);
      lower_stmts(stmt.then_body, ctx, returns_float, out, float_names, simd_names,
                  float_array_names, i64_locals);
      if (stmt.else_body) {
        push_jump(out, merge_label);
        push_label(out, else_label);
        lower_stmts(*stmt.else_body, ctx, returns_float, out, float_names, simd_names,
                    float_array_names, i64_locals);
        push_label(out, merge_label);
      } else {
        push_label(out, else_label);
      }
      break;
    }
    case Stmt::Kind::For: {
      if (!ctx.loop_stack || stmt.for_iter.empty()) {
        break;
      }
      const std::string head_label = fresh_label("for_head_");
      const std::string exit_label = fresh_label("for_exit_");
      MirInsn iter_alloc;
      iter_alloc.op = MirOp::LocalAllocInt;
      iter_alloc.ident = stmt.for_iter;
      out.push_back(std::move(iter_alloc));
      const std::string end_lit = fresh_temp();
      MirInsn end_store;
      end_store.op = MirOp::StoreInt;
      end_store.ident = end_lit;
      end_store.rhs_is_literal = true;
      end_store.rhs_int = stmt.for_end;
      out.push_back(std::move(end_store));
      MirInsn init;
      init.op = MirOp::StoreInt;
      init.ident = stmt.for_iter;
      init.rhs_is_literal = true;
      init.rhs_int = stmt.for_start;
      out.push_back(std::move(init));
      ctx.loop_stack->push_back(LoopLabels{head_label, exit_label});
      push_label(out, head_label);
      const std::string diff = fresh_temp();
      MirInsn sub;
      sub.op = MirOp::BinOpInt;
      sub.ident = diff;
      sub.lhs_ident = end_lit;
      sub.rhs_ident = stmt.for_iter;
      sub.bin_op = BinOp::Sub;
      out.push_back(std::move(sub));
      push_branch_if_zero(out, diff, exit_label);
      if (stmt_has_vectorized(stmt.decorators)) {
        MirInsn simd_on;
        simd_on.op = MirOp::ArraySimdScope;
        simd_on.int_value = 1;
        out.push_back(std::move(simd_on));
      }
      lower_stmts(stmt.for_body, ctx, returns_float, out, float_names, simd_names,
                  float_array_names, i64_locals);
      if (stmt_has_vectorized(stmt.decorators)) {
        MirInsn simd_off;
        simd_off.op = MirOp::ArraySimdScope;
        simd_off.int_value = 0;
        out.push_back(std::move(simd_off));
      }
      const std::string one_lit = fresh_temp();
      MirInsn one_store;
      one_store.op = MirOp::StoreInt;
      one_store.ident = one_lit;
      one_store.rhs_is_literal = true;
      one_store.rhs_int = 1;
      out.push_back(std::move(one_store));
      MirInsn inc;
      inc.op = MirOp::BinOpInt;
      inc.ident = stmt.for_iter;
      inc.lhs_ident = stmt.for_iter;
      inc.rhs_ident = one_lit;
      inc.bin_op = BinOp::Add;
      out.push_back(std::move(inc));
      push_jump(out, head_label);
      push_label(out, exit_label);
      ctx.loop_stack->pop_back();
      break;
    }
    case Stmt::Kind::While: {
      if (!stmt.cond) {
        break;
      }
      auto parse_i_lt_limit = [](const Expr& cond, std::string& iter,
                                 std::int64_t& limit) -> bool {
        if (cond.kind != Expr::Kind::BinOp || cond.bin_op != BinOp::Lt || !cond.lhs ||
            !cond.rhs) {
          return false;
        }
        if (cond.lhs->kind != Expr::Kind::Ident || cond.rhs->kind != Expr::Kind::IntLit) {
          return false;
        }
        iter = cond.lhs->ident;
        limit = cond.rhs->int_value;
        return limit > 0;
      };
      auto is_horner_fma_step = [](const Stmt& s, const std::string& acc_name,
                                   std::string& factor) -> bool {
        if (s.kind != Stmt::Kind::Assign || !s.init || !s.expr ||
            s.init->kind != Expr::Kind::Ident || s.init->ident != acc_name) {
          return false;
        }
        if (s.expr->kind != Expr::Kind::BinOp || s.expr->bin_op != BinOp::Add || !s.expr->lhs ||
            !s.expr->rhs) {
          return false;
        }
        const Expr& mul = *s.expr->lhs;
        if (mul.kind != Expr::Kind::BinOp || mul.bin_op != BinOp::Mul || !mul.lhs || !mul.rhs) {
          return false;
        }
        if (mul.lhs->kind == Expr::Kind::Ident && mul.lhs->ident == acc_name &&
            mul.rhs->kind == Expr::Kind::Ident) {
          factor = mul.rhs->ident;
          return true;
        }
        if (mul.rhs->kind == Expr::Kind::Ident && mul.rhs->ident == acc_name &&
            mul.lhs->kind == Expr::Kind::Ident) {
          factor = mul.lhs->ident;
          return true;
        }
        return false;
      };
      std::string iter_name;
      std::int64_t trip = 0;
      constexpr int kHornerScalarSteps = 64;
      constexpr int kHornerPow4Steps = 4;
      constexpr int kHornerSuperSteps = kHornerScalarSteps / kHornerPow4Steps;
      if (parse_i_lt_limit(*stmt.cond, iter_name, trip) && trip >= kHornerScalarSteps &&
          (trip % kHornerScalarSteps) == 0 && stmt.while_body.size() == 2) {
        std::string acc_name;
        std::string factor;
        std::string step_factor;
        bool horner_ok = false;
        for (const auto& s : stmt.while_body) {
          if (s.kind == Stmt::Kind::Assign && s.init && s.init->kind == Expr::Kind::Ident &&
              is_horner_fma_step(s, s.init->ident, step_factor)) {
            acc_name = s.init->ident;
            factor = step_factor;
            horner_ok = true;
          }
        }
        if (horner_ok && float_names.count(acc_name) > 0) {
          const std::string head_label = fresh_label("while_head_");
          const std::string exit_label = fresh_label("while_exit_");
          if (ctx.loop_stack) {
            ctx.loop_stack->push_back(LoopLabels{head_label, exit_label});
          }
          push_label(out, head_label);
          const std::string cond_tmp =
              lower_expr_to(*stmt.cond, module, out, float_names, simd_names, i64_locals);
          push_branch_if_zero(out, cond_tmp, exit_label);
          const double* const_x =
              ctx.const_floats ? lookup_const_float(*ctx.const_floats, factor) : nullptr;
          if (const_x != nullptr) {
            MirInsn step;
            step.op = MirOp::HornerStepPow4;
            step.ident = acc_name;
            step.lhs_is_literal = true;
            step.float_value = *const_x;
            step.int_value = kHornerSuperSteps;
            out.push_back(std::move(step));
          } else {
            MirInsn block;
            block.op = MirOp::HornerFmaUnroll;
            block.ident = acc_name;
            block.lhs_ident = factor;
            block.float_value = 1.0;
            block.int_value = kHornerScalarSteps;
            out.push_back(std::move(block));
          }
          MirInsn inc;
          inc.op = MirOp::BinOpInt;
          inc.ident = iter_name;
          inc.lhs_ident = iter_name;
          inc.bin_op = BinOp::Add;
          inc.rhs_is_literal = true;
          inc.rhs_int = kHornerScalarSteps;
          out.push_back(std::move(inc));
          push_jump(out, head_label);
          push_label(out, exit_label);
          if (ctx.loop_stack) {
            ctx.loop_stack->pop_back();
          }
          break;
        }
      }
      const std::string head_label = fresh_label("while_head_");
      const std::string exit_label = fresh_label("while_exit_");
      if (ctx.loop_stack) {
        ctx.loop_stack->push_back(LoopLabels{head_label, exit_label});
      }
      push_label(out, head_label);
      const std::string cond_tmp =
          lower_expr_to(*stmt.cond, module, out, float_names, simd_names, i64_locals);
      push_branch_if_zero(out, cond_tmp, exit_label);
      lower_stmts(stmt.while_body, ctx, returns_float, out, float_names, simd_names,
                  float_array_names, i64_locals);
      push_jump(out, head_label);
      push_label(out, exit_label);
      if (ctx.loop_stack) {
        ctx.loop_stack->pop_back();
      }
      break;
    }
    case Stmt::Kind::Break:
      if (ctx.loop_stack && !ctx.loop_stack->empty()) {
        push_jump(out, ctx.loop_stack->back().exit);
      }
      break;
    case Stmt::Kind::Continue:
      if (ctx.loop_stack && !ctx.loop_stack->empty()) {
        push_jump(out, ctx.loop_stack->back().head);
      }
      break;
    case Stmt::Kind::Expr:
      if (stmt.expr &&
          (stmt.expr->kind == Expr::Kind::Call || stmt.expr->kind == Expr::Kind::MethodCall)) {
        if (stmt.expr->kind == Expr::Kind::Call && stmt.expr->ident == "print" &&
            !stmt.expr->args.empty()) {
          lower_print_arg(*stmt.expr->args[0], module, out, float_names, simd_names,
                          i64_locals);
        } else if (stmt.expr->kind == Expr::Kind::Call && stmt.expr->ident == "axpy" &&
                   stmt.expr->args.size() == 3 && stmt.expr->args[1]->kind == Expr::Kind::Ident &&
                   stmt.expr->args[2]->kind == Expr::Kind::Ident) {
          bool lit = stmt.expr->args[0]->kind == Expr::Kind::FloatLit;
          double fv = lit ? stmt.expr->args[0]->float_value : 0.0;
          std::string alpha_id;
          if (!lit) {
            alpha_id = lower_expr_to(*stmt.expr->args[0], module, out, float_names, simd_names,
                                     i64_locals);
          }
          (void)emit_float_array_axpy(lit, fv, alpha_id, stmt.expr->args[1]->ident,
                                      stmt.expr->args[2]->ident, out);
        } else {
          (void)lower_expr_to(*stmt.expr, module, out, float_names, simd_names, i64_locals);
        }
      }
      break;
    default:
      break;
  }
}

void lower_stmts(const std::vector<Stmt>& stmts, LowerCtx& ctx, bool returns_float,
                 std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                 std::unordered_set<std::string>& simd_names,
                 std::unordered_set<std::string>& float_array_names,
                 std::unordered_set<std::string>& i64_locals) {
  for (const auto& stmt : stmts) {
    lower_stmt(stmt, ctx, returns_float, out, float_names, simd_names, float_array_names,
               i64_locals);
    if (!out.empty() && (out.back().op == MirOp::ReturnVoid || out.back().op == MirOp::ReturnInt ||
                         out.back().op == MirOp::ReturnFloat || out.back().op == MirOp::ReturnIdent ||
                         out.back().op == MirOp::ReturnObject)) {
      return;
    }
  }
}

bool insn_terminates(MirOp op) {
  return op == MirOp::ReturnVoid || op == MirOp::ReturnInt || op == MirOp::ReturnFloat ||
         op == MirOp::ReturnIdent || op == MirOp::ReturnObject;
}

void append_implicit_return(std::vector<MirInsn>& body) {
  if (body.empty() || !insn_terminates(body.back().op)) {
    MirInsn ins;
    ins.op = MirOp::ReturnVoid;
    body.push_back(std::move(ins));
  }
}

}  // namespace

MirModule lower_to_mir(const Module& module) {
  temp_counter = 0;
  par_counter = 0;
  MirModule mir;
  for (const auto& proc : module.procs) {
    MirFn fn;
    fn.name = proc.name;
    fn.is_extern = proc.is_extern;
    fn.is_async = proc.is_async;
    copy_decorators(proc.decorators, fn.decorators);
    apply_fn_decorator_codegen_flags(fn);
    if (proc.ret_type) {
      fn.returns_float = is_float_type_name(proc.ret_type->name);
      fn.returns_i64 = mir_ptr_param_type_name(proc.ret_type->name) ||
                        is_i64_type_name(proc.ret_type->name) ||
                        proc.ret_type->name == "StringView";
      fn.returns_void = proc.ret_type->name == "unit";
      if (object_alias_for_named_type(module, *proc.ret_type)) {
        fn.returns_object = true;
        fn.returns_float = false;
        fn.returns_void = false;
        collect_object_return_layout_r(module, *proc.ret_type, "", fn.return_object_layout);
      }
    } else if (proc.is_extern) {
      fn.returns_void = true;
    }
    for (const auto& p : proc.params) {
      if (object_alias_for_named_type(module, p.type)) {
        append_mir_params_for_object_type(module, p.type, std::string("__li_o_") + p.name,
                                          fn.params, p.type.is_var);
      } else {
        MirParam mp;
        mp.name = p.name;
        const TypeExpr* ut = unwrap_refinement_type(&p.type);
        if (ut && ut->kind == TypeKind::Array && ut->array_size > 0 && ut->elem) {
          std::int64_t m_rows = 0;
          std::int64_t m_cols = 0;
          if (is_2d_float_matrix_type(*ut, &m_rows, &m_cols)) {
            mp.fixed_array_elems = m_rows;
            mp.matrix_cols = m_cols;
            mp.is_matrix = true;
            mp.is_float = true;
          } else {
            const TypeExpr* el = unwrap_refinement_type(ut->elem.get());
            if (el && el->kind == TypeKind::Named) {
              mp.fixed_array_elems = ut->array_size;
              mp.is_float = is_float_type_name(el->name);
              mp.is_var = p.type.is_var;
            }
          }
        } else {
          mp.is_float = is_float_type_name(p.type.name);
        }
        mp.is_string = mir_ptr_param_type_name(p.type.name);
        mp.is_i64 = is_i64_type_name(p.type.name) || is_string_type_name(p.type.name);
        fn.params.push_back(std::move(mp));
      }
    }
    if (!proc.is_extern) {
      const bool is_async_fn =
          proc.is_async ||
          std::any_of(proc.decorators.begin(), proc.decorators.end(),
                      [](const Decorator& d) { return d.name == "async"; });
      if (is_async_fn) {
        mir.uses_async = true;
        MirInsn enter;
        enter.op = MirOp::AsyncFrameEnter;
        fn.body.push_back(std::move(enter));
      }
      std::unordered_set<std::string> float_names;
      std::unordered_set<std::string> simd_names;
      std::unordered_set<std::string> float_array_names;
      std::unordered_set<std::string> matrix_array_names;
      std::unordered_set<std::string> i64_locals;
      LowerArrayCtx arr_ctx;
      arr_ctx.float_array_names = &float_array_names;
      arr_ctx.matrix_names = &matrix_array_names;
      g_arr_ctx = &arr_ctx;
      g_mir_module = &mir;
      seed_float_params(fn, float_names);
      seed_float_array_params(fn);
      seed_i64_params(fn, i64_locals);
      std::vector<LoopLabels> loop_stack;
      std::unordered_map<std::string, double> const_floats;
      std::unordered_map<std::string, const TypeExpr*> object_local_types =
          collect_object_local_types(module, proc);
      LowerCtx ctx{&module,       &mir,     proc.name, &proc,
                   &loop_stack,   &object_local_types, &const_floats};
      g_object_locals = &object_local_types;
      lower_stmts(proc.body, ctx, fn.returns_float, fn.body, float_names, simd_names,
                  float_array_names, i64_locals);
      g_object_locals = nullptr;
      g_mir_module = nullptr;
      if (is_async_fn) {
        MirInsn leave;
        leave.op = MirOp::AsyncFrameLeave;
        if (!fn.body.empty() && insn_terminates(fn.body.back().op)) {
          fn.body.insert(fn.body.end() - 1, std::move(leave));
        } else {
          fn.body.push_back(std::move(leave));
        }
      }
      if (!fn.body.empty() && !insn_terminates(fn.body.back().op) && fn.returns_object &&
          proc.ret_type) {
        const std::string z = std::string("__li_o___im") + std::to_string(temp_counter++);
        emit_object_slots_r(module, *proc.ret_type, z, fn.body, float_names, i64_locals);
        for (const auto& lf : fn.return_object_layout) {
          if (lf.fixed_array_elems > 0) {
            for (std::int64_t i = 0; i < lf.fixed_array_elems; ++i) {
              MirInsn st;
              st.ident = z + "_" + lf.name;
              st.index_is_literal = true;
              st.int_value = i;
              st.rhs_is_literal = true;
              if (lf.is_float) {
                st.op = MirOp::ArrayStoreFloat;
                st.float_value = 0.0;
              } else {
                st.op = MirOp::ArrayStoreInt;
                st.rhs_int = 0;
              }
              fn.body.push_back(std::move(st));
            }
          } else {
            MirInsn st;
            st.ident = z + "_" + lf.name;
            st.rhs_is_literal = true;
            if (lf.is_float) {
              st.op = MirOp::StoreFloat;
              st.float_value = 0.0;
            } else if (lf.is_i64 || lf.is_string) {
              st.op = MirOp::StoreI64;
              st.rhs_int = 0;
            } else {
              st.op = MirOp::StoreInt;
              st.rhs_int = 0;
            }
            fn.body.push_back(std::move(st));
          }
        }
        MirInsn ret;
        ret.op = MirOp::ReturnObject;
        ret.ident = z;
        ret.object_layout = fn.return_object_layout;
        fn.body.push_back(std::move(ret));
      } else {
        append_implicit_return(fn.body);
      }
      g_arr_ctx = nullptr;
    }
    if (!proc.is_extern && fn.body.empty()) {
      MirInsn ins;
      ins.op = MirOp::ReturnVoid;
      fn.body.push_back(std::move(ins));
    }
    mir.functions.push_back(std::move(fn));
  }
  return mir;
}

}  // namespace li
