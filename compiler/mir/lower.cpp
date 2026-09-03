#include "li/mir.hpp"
#include "li/mir_types.hpp"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace li {

namespace {

int temp_counter = 0;
std::vector<std::string> while_head_labels;
std::vector<std::string> while_exit_labels;
const ProcDecl* g_cur_proc = nullptr;
// Array-ident registry (per-proc, reset in lower_to_mir): name -> declared
// size for float AND int arrays, plus the set of int-array names. Seeded from
// params and VarDecls so elementwise binops / sum / norm / dot / axpy can emit
// the walker's array ops (ArrayBinOpF64/I64, ArrayScaleF64, ArraySumF64/I64,
// ArrayDotF64, ArrayAxpyF64) with the declared length in int_value.
std::unordered_map<std::string, std::int64_t> g_array_sizes;
// Local (var-declared) 2D float matrices: name -> (rows, cols). Mirrors the
// walker's matok registry, which only holds LOCAL matrices (matrix params are
// not seeded, so `return A @ B` / `A[i][j]` on params take the generic
// BinOpInt / no-emit paths).
std::unordered_map<std::string, std::pair<std::int64_t, std::int64_t>> g_matrices;
// Matrix params (name -> (rows, cols)): registered for the no-emit 2D-load
// quirk and matrix call args, but NOT treated as float names / array sizes.
std::unordered_map<std::string, std::pair<std::int64_t, std::int64_t>> g_matrix_params;
// Parallel-for state: per-module synthetic-fn counter + uses_openmp bit, and
// the per-proc list of synthesized __li_par_* functions (spliced before the
// enclosing FN, mirroring the walker's hold-buffer replay). g_in_parallel
// switches array stores to the walker's parallel form (INS 10 + value temp).
int g_par_counter = 0;
bool g_uses_openmp = false;
bool g_in_parallel = false;
std::vector<MirFn> g_par_fns;
struct ObjectField {
  std::string name;
  bool is_float = false;
  std::int64_t array_elems = 0;
  bool is_i64 = false;
};

// Object types: type name -> ordered leaf fields. Array fields retain their
// fixed element count so object return/call lowering can preserve the array ABI.
std::unordered_map<std::string, std::vector<ObjectField>> g_object_types;
// Object-typed locals (var name -> type name), seeded per-proc from VarDecls
// and params so Field reads/stores know how to mangle the slot.
std::unordered_map<std::string, std::string> g_object_vars;
// itok-equivalent: names of scalar pointer-width (str/bytes/StringView/ptr/i64)
// params and locals plus pointer-width call temps. The walker registers these
// in its itok table (mir_proc/mir_var_decl ty 2/3/4) and consults it for
// ReturnIdent's ret_is_i64 bit (mir_return ri via mir_name_i64).
std::unordered_set<std::string> g_i64_names;

bool is_array_ident(const std::string& n) {
  return g_array_sizes.count(n) > 0 || g_matrices.count(n) > 0 ||
         g_matrix_params.count(n) > 0;
}

std::string fresh_temp() { return "__t" + std::to_string(temp_counter++); }
std::string fresh_label(const std::string& prefix) {
  return prefix + std::to_string(temp_counter++);
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

bool tail_is_terminator(const std::vector<MirInsn>& out) {
  return !out.empty() && (out.back().op == MirOp::ReturnVoid ||
                          out.back().op == MirOp::ReturnInt ||
                          out.back().op == MirOp::ReturnFloat ||
                          out.back().op == MirOp::ReturnIdent ||
                          out.back().op == MirOp::ReturnObject ||
                          out.back().op == MirOp::Jump ||
                          out.back().op == MirOp::BranchIfZero);
}

// Emit a jump only if the current block hasn't already terminated (e.g. a body
// that ended in `return`). Without this, a dead Jump after a Ret would put two
// terminators in one basic block.
void push_jump_if_open(std::vector<MirInsn>& out, const std::string& label) {
  if (!tail_is_terminator(out)) {
    push_jump(out, label);
  }
}

bool is_arith_binop(BinOp op) {
  return op == BinOp::Add || op == BinOp::Sub || op == BinOp::Mul || op == BinOp::Div ||
         op == BinOp::Mod || op == BinOp::FloorDiv || op == BinOp::Pow;
}

// The walker selects BinOpFloat for comparisons too (mir_lower_expr: `if arith
// == 1 or cmp == 1: flt = mir_desc_float(...)`), so float-ness propagates
// through Le/Lt/Ge/Gt/Eq/Ne operands.
bool is_float_ctx_binop(BinOp op) {
  return is_arith_binop(op) || op == BinOp::Le || op == BinOp::Lt || op == BinOp::Ge ||
         op == BinOp::Gt || op == BinOp::Eq || op == BinOp::Ne;
}

bool is_float_expr(const Expr& e, const std::unordered_set<std::string>& float_names,
                   const std::unordered_set<std::string>& float_arrays) {
  switch (e.kind) {
    case Expr::Kind::FloatLit:
      return true;
    case Expr::Kind::Ident:
      return float_names.count(e.ident) > 0;
    case Expr::Kind::Field: {
      // Field slot is float iff the object's field type is float.
      std::string base;
      std::string field;
      if (e.base && e.base->kind == Expr::Kind::Ident) {
        base = e.base->ident;
      }
      if (e.index && e.index->kind == Expr::Kind::Ident) {
        field = e.index->ident;
      }
      const auto it = g_object_vars.find(base);
      if (it == g_object_vars.end()) {
        return false;
      }
      const auto ty = g_object_types.find(it->second);
      if (ty == g_object_types.end()) {
        return false;
      }
      for (const auto& f : ty->second) {
        if (f.name == field) {
          return f.is_float;
        }
      }
      return false;
    }
    case Expr::Kind::Index:
      // x[i] over a float array param/local is a float element.
      if (e.base && e.base->kind == Expr::Kind::Ident) {
        return float_arrays.count(e.base->ident) > 0;
      }
      // A[i][j] over a LOCAL float matrix is a float element (walker
      // mir_desc_float k==5 -> mir_name_matrix_local); matrix params are NOT
      // float-registered, so `return A[0][0] * B[0][0]` on params lowers as
      // BinOpInt (matching the walker's no-emit temp path).
      if (e.base && e.base->kind == Expr::Kind::Index && e.base->base &&
          e.base->base->kind == Expr::Kind::Ident) {
        return g_matrices.count(e.base->base->ident) > 0;
      }
      return false;
    case Expr::Kind::BinOp:
      if (!is_float_ctx_binop(e.bin_op)) {
        return false;
      }
      return is_float_expr(*e.lhs, float_names, float_arrays) ||
             is_float_expr(*e.rhs, float_names, float_arrays);
    case Expr::Kind::UnaryMinus:
      if (e.operand) {
        return is_float_expr(*e.operand, float_names, float_arrays);
      }
      return false;
    default:
      return false;
  }
}

const ProcDecl* find_proc(const Module& module, const std::string& name) {
  const auto it = std::find_if(module.procs.begin(), module.procs.end(),
                               [&](const ProcDecl& p) { return p.name == name; });
  return it != module.procs.end() ? &*it : nullptr;
}

void seed_float_params(const MirFn& fn, std::unordered_set<std::string>& float_names,
                       std::unordered_set<std::string>& float_arrays) {
  for (const auto& p : fn.params) {
    if (p.is_matrix) {
      // Matrix params keep the PARAM-line is_float=1 bit but are NOT seeded
      // into float_names/float_arrays/g_array_sizes: the walker registers
      // matrix params in neither ftok nor fatok/matok, so `A @ B` on params
      // falls to the generic BinOpInt path and `A[i][j]` on params is a
      // no-emit temp.
      g_matrix_params[p.name] = {p.array_size, p.matrix_cols};
      continue;
    }
    if (p.is_float) {
      float_names.insert(p.name);
    }
    if (p.is_array) {
      g_array_sizes[p.name] = p.array_size;
      if (p.is_float) {
        float_arrays.insert(p.name);
      }
    }
  }
}

// Elementwise array binop emission. Mirrors the walker's VarDecl/Assign paths
// (bootstrap/lic/main.li mir_stmt / mir_assign): `var c = a + b` / `c = a + b`
// on same-shape float or int arrays lowers to ArrayBinOpF64 (INS 20) / I64
// (INS 21) directly into the destination ident (no extra temp). Broadcast
// flags brl/brr are set when one operand is array[1].
bool emit_array_binop_into(const Expr& binop, const std::string& dest,
                           const std::unordered_set<std::string>& float_arrays,
                           std::vector<MirInsn>& out) {
  if (binop.kind != Expr::Kind::BinOp || !binop.lhs || !binop.rhs) {
    return false;
  }
  if (!is_arith_binop(binop.bin_op)) {
    return false;
  }
  if (binop.lhs->kind != Expr::Kind::Ident || binop.rhs->kind != Expr::Kind::Ident) {
    return false;
  }
  const auto itl = g_array_sizes.find(binop.lhs->ident);
  const auto itr = g_array_sizes.find(binop.rhs->ident);
  if (itl == g_array_sizes.end() || itr == g_array_sizes.end()) {
    return false;
  }
  const std::int64_t szl = itl->second;
  const std::int64_t szr = itr->second;
  if (szl <= 0 || szr <= 0) {
    return false;
  }
  const bool broadcast_ok =
      szl == szr || (szl == 1 && szr > 1) || (szr == 1 && szl > 1);
  if (!broadcast_ok) {
    return false;
  }
  const bool flt = float_arrays.count(binop.lhs->ident) > 0 ||
                   float_arrays.count(binop.rhs->ident) > 0;
  MirInsn ins;
  ins.op = flt ? MirOp::ArrayBinOpF64 : MirOp::ArrayBinOpI64;
  ins.ident = dest;
  ins.int_value = std::max(szl, szr);
  ins.lhs_ident = binop.lhs->ident;
  ins.rhs_ident = binop.rhs->ident;
  ins.bin_op = binop.bin_op;
  if (szl == 1 && szr > 1) {
    ins.array_broadcast_lhs_len1 = true;
  }
  if (szr == 1 && szl > 1) {
    ins.array_broadcast_rhs_len1 = true;
  }
  out.push_back(std::move(ins));
  return true;
}

// Float scale `2.0 * x` / `x * 2.0` on a float array lowers to ArrayScaleF64
// (INS 22) directly into the destination ident: int_value = size, float_value
// = the literal, lhs_ident = the array, rhs_is_literal = 1.
bool emit_array_scale_into(const Expr& binop, const std::string& dest,
                           const std::unordered_set<std::string>& float_arrays,
                           std::vector<MirInsn>& out) {
  if (binop.kind != Expr::Kind::BinOp || binop.bin_op != BinOp::Mul || !binop.lhs ||
      !binop.rhs) {
    return false;
  }
  const Expr* lit = nullptr;
  const Expr* arr = nullptr;
  if (binop.lhs->kind == Expr::Kind::FloatLit &&
      binop.rhs->kind == Expr::Kind::Ident &&
      float_arrays.count(binop.rhs->ident) > 0) {
    lit = binop.lhs.get();
    arr = binop.rhs.get();
  } else if (binop.rhs->kind == Expr::Kind::FloatLit &&
             binop.lhs->kind == Expr::Kind::Ident &&
             float_arrays.count(binop.lhs->ident) > 0) {
    lit = binop.rhs.get();
    arr = binop.lhs.get();
  } else {
    return false;
  }
  const auto it = g_array_sizes.find(arr->ident);
  if (it == g_array_sizes.end() || it->second <= 0) {
    return false;
  }
  MirInsn ins;
  ins.op = MirOp::ArrayScaleF64;
  ins.ident = dest;
  ins.int_value = it->second;
  ins.float_value = lit->float_value;
  ins.lhs_ident = arr->ident;
  ins.rhs_is_literal = true;
  out.push_back(std::move(ins));
  return true;
}

std::string lower_expr_to(const Expr& e, const Module& module, std::vector<MirInsn>& out,
                          std::unordered_set<std::string>& float_names,
                          std::unordered_set<std::string>& float_arrays) {
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
    case Expr::Kind::Field: {
      // `obj.field` reads lower to the object's per-field scalar slot
      // __li_o_<var>_<field> (walker mir_obj_field_read mangles the slot).
      // No INS is emitted; the slot name is used directly as an operand.
      std::string base;
      std::string field;
      if (e.base && e.base->kind == Expr::Kind::Ident) {
        base = e.base->ident;
      }
      if (e.index && e.index->kind == Expr::Kind::Ident) {
        field = e.index->ident;
      }
      return "__li_o_" + base + "_" + field;
    }
    case Expr::Kind::UnaryMinus: {
      // Walker mir_lower_expr k==11: `-x` = `0 - x`. Int operands fold the
      // literal zero into the BinOpInt (lhs_is_literal=1, lhs_int=0); float
      // operands materialize a StoreFloat(0.0) zero temp first. Dest counter
      // is allocated before the operand.
      const std::string dest = fresh_temp();
      if (!e.operand) {
        MirInsn ins;
        ins.op = MirOp::BinOpInt;
        ins.ident = dest;
        ins.bin_op = BinOp::Sub;
        ins.lhs_is_literal = true;
        ins.lhs_int = 0;
        ins.rhs_is_literal = false;
        ins.rhs_ident = dest;
        out.push_back(std::move(ins));
        return dest;
      }
      const bool flt = is_float_expr(*e.operand, float_names, float_arrays);
      MirInsn ins;
      ins.ident = dest;
      ins.bin_op = BinOp::Sub;
      if (flt) {
        const std::string zero = fresh_temp();
        MirInsn z;
        z.op = MirOp::StoreFloat;
        z.ident = zero;
        z.float_value = 0.0;
        out.push_back(std::move(z));
        float_names.insert(zero);
        ins.op = MirOp::BinOpFloat;
        ins.lhs_ident = zero;
        ins.lhs_is_literal = false;
        ins.rhs_ident = lower_expr_to(*e.operand, module, out, float_names, float_arrays);
        ins.rhs_is_literal = false;
        float_names.insert(dest);
      } else {
        ins.op = MirOp::BinOpInt;
        ins.lhs_is_literal = true;
        ins.lhs_int = 0;
        ins.rhs_ident = lower_expr_to(*e.operand, module, out, float_names, float_arrays);
        ins.rhs_is_literal = false;
      }
      out.push_back(std::move(ins));
      return dest;
    }
    case Expr::Kind::BinOp: {
      // `A @ B` over two LOCAL matrices (expr position, e.g. inside a chained
      // `C = (A @ B) @ D`): ArrayAlloc temp (ident=temp, rhs_int=rows(lhs),
      // matrix flags) + ArrayMatMul2DF64 temp. The temp is NOT re-registered
      // as a matrix, so the outer `temp @ D` falls to the generic BinOpInt
      // path below (walker mir_mat_dims only knows registered idents).
      if (e.bin_op == BinOp::MatMul && e.lhs && e.rhs &&
          e.lhs->kind == Expr::Kind::Ident && e.rhs->kind == Expr::Kind::Ident) {
        const auto la = g_matrices.find(e.lhs->ident);
        const auto ra = g_matrices.find(e.rhs->ident);
        if (la != g_matrices.end() && ra != g_matrices.end() &&
            la->second.second == ra->second.first) {
          const std::string dest = fresh_temp();
          MirInsn alloc;
          alloc.op = MirOp::ArrayAlloc;
          alloc.ident = dest;
          alloc.int_value = la->second.first;
          alloc.rhs_int = la->second.first;
          alloc.array_is_float = true;
          alloc.array_is_matrix = true;
          out.push_back(std::move(alloc));
          MirInsn mm;
          mm.op = MirOp::ArrayMatMul2DF64;
          mm.ident = dest;
          mm.int_value = la->second.first;
          mm.lhs_ident = e.lhs->ident;
          mm.rhs_ident = e.rhs->ident;
          mm.rhs_int = la->second.second;
          mm.lhs_int = ra->second.second;
          out.push_back(std::move(mm));
          return dest;
        }
      }
      // `x @ y` over float-array params or locals lowers to ArrayDotF64
      // (walker INS 14) with the declared array length in int_value.
      if (e.bin_op == BinOp::MatMul && e.lhs && e.rhs &&
          e.lhs->kind == Expr::Kind::Ident && e.rhs->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.lhs->ident) > 0 && g_array_sizes.count(e.rhs->ident) > 0 &&
          g_array_sizes[e.lhs->ident] == g_array_sizes[e.rhs->ident]) {
        // Equal-length 1D float arrays only (walker requires szr == szl; a
        // length mismatch falls to the generic BinOpInt MatMul path).
        MirInsn ins;
        ins.op = MirOp::ArrayDotF64;
        ins.int_value = g_array_sizes[e.lhs->ident];
        ins.lhs_ident = e.lhs->ident;
        ins.rhs_ident = e.rhs->ident;
        const std::string dest = fresh_temp();
        ins.ident = dest;
        out.push_back(std::move(ins));
        float_names.insert(dest);
        return dest;
      }
      // Elementwise array binop in expression position: `sum(a + b)` lowers
      // the binop to ArrayAlloc temp + ArrayBinOpF64 into the temp (walker
      // mir_lower_expr k==7 array path). VarDecl/Assign positions take the
      // direct-into-var form via emit_array_binop_into instead.
      if (is_arith_binop(e.bin_op) && e.lhs && e.rhs &&
          e.lhs->kind == Expr::Kind::Ident && e.rhs->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.lhs->ident) > 0 && g_array_sizes.count(e.rhs->ident) > 0) {
        const std::int64_t szl = g_array_sizes[e.lhs->ident];
        const std::int64_t szr = g_array_sizes[e.rhs->ident];
        const bool broadcast_ok =
            szl == szr || (szl == 1 && szr > 1) || (szr == 1 && szl > 1);
        if (broadcast_ok) {
          const std::int64_t esz = std::max(szl, szr);
          // Walker emits the expr-context ArrayAlloc with an EMPTY ident cell
          // (fea is all-zero from mir_fdef; only the binop names the temp).
          MirInsn a;
          a.op = MirOp::ArrayAlloc;
          a.int_value = esz;
          out.push_back(std::move(a));
          const std::string alloc = fresh_temp();
          const bool flt = float_arrays.count(e.lhs->ident) > 0 ||
                           float_arrays.count(e.rhs->ident) > 0;
          MirInsn ins;
          ins.op = flt ? MirOp::ArrayBinOpF64 : MirOp::ArrayBinOpI64;
          ins.ident = alloc;
          ins.int_value = esz;
          ins.lhs_ident = e.lhs->ident;
          ins.rhs_ident = e.rhs->ident;
          ins.bin_op = e.bin_op;
          if (szl == 1 && szr > 1) {
            ins.array_broadcast_lhs_len1 = true;
          }
          if (szr == 1 && szl > 1) {
            ins.array_broadcast_rhs_len1 = true;
          }
          out.push_back(std::move(ins));
          return alloc;
        }
      }
      // Float scale `2.0 * x` in expression position -> ArrayAlloc (empty
      // ident, matching the walker) + ArrayScaleF64 into the temp.
      if (e.bin_op == BinOp::Mul && e.lhs && e.rhs) {
        const Expr* lit = nullptr;
        const Expr* arr = nullptr;
        if (e.lhs->kind == Expr::Kind::FloatLit && e.rhs->kind == Expr::Kind::Ident &&
            float_arrays.count(e.rhs->ident) > 0) {
          lit = e.lhs.get();
          arr = e.rhs.get();
        } else if (e.rhs->kind == Expr::Kind::FloatLit &&
                   e.lhs->kind == Expr::Kind::Ident &&
                   float_arrays.count(e.lhs->ident) > 0) {
          lit = e.rhs.get();
          arr = e.lhs.get();
        }
        if (lit && arr && g_array_sizes.count(arr->ident) > 0) {
          const std::string dest = fresh_temp();
          MirInsn alloc;
          alloc.op = MirOp::ArrayAlloc;
          alloc.int_value = g_array_sizes[arr->ident];
          out.push_back(std::move(alloc));
          MirInsn ins;
          ins.op = MirOp::ArrayScaleF64;
          ins.ident = dest;
          ins.int_value = g_array_sizes[arr->ident];
          ins.float_value = lit->float_value;
          ins.lhs_ident = arr->ident;
          ins.rhs_is_literal = true;
          out.push_back(std::move(ins));
          float_names.insert(dest);
          return dest;
        }
      }
      // Walker contract (bootstrap/lic/main.li mir_lower_expr k==7): the dest
      // counter is allocated BEFORE the operands, and int literals fold into
      // lhs_is_literal/rhs_is_literal instead of materializing a StoreInt temp.
      // Float literals do NOT fold — they materialize as temps (walker lowers
      // both float operands through mir_lower_expr).
      const std::string dest = fresh_temp();
      MirInsn ins;
      const bool flt = is_float_expr(e, float_names, float_arrays);
      ins.op = flt ? MirOp::BinOpFloat : MirOp::BinOpInt;
      ins.ident = dest;
      ins.bin_op = e.bin_op;
      const auto lower_operand = [&](const Expr& opnd) -> std::string {
        // Float binops never fold literals (walker lowers both operands); an
        // int literal in a float context materializes as a StoreFloat temp,
        // which is exactly the walker's `-x` float zero-temp dance.
        if (flt && opnd.kind == Expr::Kind::IntLit) {
          const std::string d = fresh_temp();
          MirInsn s;
          s.op = MirOp::StoreFloat;
          s.ident = d;
          s.float_value = static_cast<double>(opnd.int_value);
          out.push_back(std::move(s));
          float_names.insert(d);
          return d;
        }
        return lower_expr_to(opnd, module, out, float_names, float_arrays);
      };
      if (!flt && e.lhs->kind == Expr::Kind::IntLit) {
        ins.lhs_is_literal = true;
        ins.lhs_int = e.lhs->int_value;
      } else {
        // Walker sets f[29]=0 for non-literal operands (mir_fdef defaults it 0).
        ins.lhs_is_literal = false;
        ins.lhs_ident = lower_operand(*e.lhs);
      }
      if (!flt && e.rhs->kind == Expr::Kind::IntLit) {
        ins.rhs_is_literal = true;
        ins.rhs_int = e.rhs->int_value;
      } else {
        // Walker sets f[26]=0 explicitly (struct default here is true).
        ins.rhs_is_literal = false;
        ins.rhs_ident = lower_operand(*e.rhs);
      }
      out.push_back(std::move(ins));
      if (flt) {
        float_names.insert(dest);
      }
      return dest;
    }
    case Expr::Kind::Call: {
      // SIMD builtins (walker mir_lower_expr simd_op): __li_simd_splat_f64
      // (37) / mul (38) / add (39) / horiz_sum (40). Dest is a fresh temp
      // with simd_lanes=4; splat puts its arg in the rhs slot, mul/add put
      // arg0 in lhs and arg1 in rhs, horiz_sum puts its arg in lhs. Plain
      // idents pass through by name; other args lower to temps first.
      int simd_op = 0;
      if (e.ident == "__li_simd_splat_f64") {
        simd_op = 37;
      } else if (e.ident == "__li_simd_mul_f64") {
        simd_op = 38;
      } else if (e.ident == "__li_simd_add_f64") {
        simd_op = 39;
      } else if (e.ident == "__li_horiz_sum_f64") {
        simd_op = 40;
      }
      if (simd_op != 0) {
        const std::string dest = fresh_temp();
        MirInsn ins;
        ins.op = simd_op == 37   ? MirOp::SimdSplatF64
                 : simd_op == 38 ? MirOp::SimdMulF64
                 : simd_op == 39 ? MirOp::SimdAddF64
                                 : MirOp::SimdHorizSumF64;
        ins.ident = dest;
        ins.simd_lanes = 4;
        for (std::size_t ai = 0; ai < e.args.size(); ++ai) {
          // Plain idents pass through by name; any other expr is lowered to
          // its own materialization insn and leaves an EMPTY span in the simd
          // insn (walker: lowered args get sd_s == sd_e, so no name prints).
          if (e.args[ai]->kind != Expr::Kind::Ident) {
            (void)lower_expr_to(*e.args[ai], module, out, float_names,
                                float_arrays);
            continue;
          }
          const std::string& name = e.args[ai]->ident;
          if (simd_op == 37 || ai == 1) {
            ins.rhs_ident = name;
          } else {
            ins.lhs_ident = name;
          }
        }
        out.push_back(std::move(ins));
        return dest;
      }
      // Builtin array kernels (self-hosted walker reference, bootstrap/lic/
      // main.li mir_lower_expr k==4): sum/norm/dot/axpy on array operands.
      if (e.ident == "sum" && e.args.size() == 1 &&
          e.args[0]->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.args[0]->ident) > 0) {
        const bool flt = float_arrays.count(e.args[0]->ident) > 0;
        const std::string dest = fresh_temp();
        MirInsn ins;
        ins.op = flt ? MirOp::ArraySumF64 : MirOp::ArraySumI64;
        ins.int_value = g_array_sizes[e.args[0]->ident];
        ins.ident = dest;
        ins.lhs_ident = e.args[0]->ident;
        out.push_back(std::move(ins));
        if (flt) {
          float_names.insert(dest);
        }
        return dest;
      }
      // sum(a * b) -> ArrayAlloc temp + ArrayScaleF64 + ArraySumF64
      // (walker sum(Mul_expr) path; lhs=b, rhs=a, rhs_is_literal=0).
      if (e.ident == "sum" && e.args.size() == 1 &&
          e.args[0]->kind == Expr::Kind::BinOp &&
          e.args[0]->bin_op == BinOp::Mul && e.args[0]->lhs && e.args[0]->rhs &&
          e.args[0]->lhs->kind == Expr::Kind::Ident &&
          e.args[0]->rhs->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.args[0]->lhs->ident) > 0 &&
          g_array_sizes.count(e.args[0]->rhs->ident) > 0) {
        const auto& mul = *e.args[0];
        const std::int64_t szl = g_array_sizes[mul.lhs->ident];
        const std::int64_t szr = g_array_sizes[mul.rhs->ident];
        if (szl == szr && szl > 0) {
          const std::string tmp = fresh_temp();
          MirInsn alloc;
          alloc.op = MirOp::ArrayAlloc;
          alloc.int_value = szl;
          alloc.ident = tmp;
          alloc.array_is_float = true;
          out.push_back(std::move(alloc));
          MirInsn scale;
          scale.op = MirOp::ArrayScaleF64;
          scale.int_value = szl;
          scale.ident = tmp;
          scale.lhs_ident = mul.rhs->ident;
          scale.rhs_ident = mul.lhs->ident;
          scale.rhs_is_literal = false;
          out.push_back(std::move(scale));
          const std::string dest = fresh_temp();
          MirInsn sum;
          sum.op = MirOp::ArraySumF64;
          sum.int_value = szl;
          sum.ident = dest;
          sum.lhs_ident = tmp;
          out.push_back(std::move(sum));
          float_names.insert(dest);
          return dest;
        }
      }
      // norm(a) on an int array -> ArrayAlloc + ArrayBinOpI64(a,a,Mul) +
      // ArraySumI64; on a float array -> ArrayDotF64(a,a) + CallExtern sqrt.
      if (e.ident == "norm" && e.args.size() == 1 &&
          e.args[0]->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.args[0]->ident) > 0) {
        const bool flt = float_arrays.count(e.args[0]->ident) > 0;
        const std::int64_t sz = g_array_sizes[e.args[0]->ident];
        if (flt) {
          const std::string dot = fresh_temp();
          MirInsn d;
          d.op = MirOp::ArrayDotF64;
          d.int_value = sz;
          d.ident = dot;
          d.lhs_ident = e.args[0]->ident;
          d.rhs_ident = e.args[0]->ident;
          out.push_back(std::move(d));
          float_names.insert(dot);
          const std::string dest = fresh_temp();
          MirInsn call;
          call.op = MirOp::CallExtern;
          call.callee = "li_rt_sqrt";
          call.ident = dest;
          MirArg ma;
          ma.ident = dot;
          call.args.push_back(std::move(ma));
          out.push_back(std::move(call));
          float_names.insert(dest);
          return dest;
        }
        const std::string tmp = fresh_temp();
        MirInsn alloc;
        alloc.op = MirOp::ArrayAlloc;
        alloc.int_value = sz;
        alloc.ident = tmp;
        out.push_back(std::move(alloc));
        MirInsn sq;
        sq.op = MirOp::ArrayBinOpI64;
        sq.int_value = sz;
        sq.ident = tmp;
        sq.lhs_ident = e.args[0]->ident;
        sq.rhs_ident = e.args[0]->ident;
        sq.bin_op = BinOp::Mul;
        out.push_back(std::move(sq));
        const std::string dest = fresh_temp();
        MirInsn sum;
        sum.op = MirOp::ArraySumI64;
        sum.int_value = sz;
        sum.ident = dest;
        sum.lhs_ident = tmp;
        out.push_back(std::move(sum));
        return dest;
      }
      // dot(a, b) -> ArrayDotF64 with the declared length. Equal lengths only
      // (walker requires szr == szl; a mismatch falls through to the generic
      // CallExtern `dot` path).
      if (e.ident == "dot" && e.args.size() == 2 &&
          e.args[0]->kind == Expr::Kind::Ident && e.args[1]->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.args[0]->ident) > 0 &&
          g_array_sizes.count(e.args[1]->ident) > 0 &&
          g_array_sizes[e.args[0]->ident] == g_array_sizes[e.args[1]->ident]) {
        const std::string dest = fresh_temp();
        MirInsn ins;
        ins.op = MirOp::ArrayDotF64;
        ins.int_value = g_array_sizes[e.args[0]->ident];
        ins.ident = dest;
        ins.lhs_ident = e.args[0]->ident;
        ins.rhs_ident = e.args[1]->ident;
        out.push_back(std::move(ins));
        float_names.insert(dest);
        return dest;
      }
      // axpy(alpha, x, y) -> ArrayAxpyF64 (no dest ident; alpha literal in
      // float_value, lhs=x, rhs=y, rhs_is_literal=1). Walker axpy path.
      if (e.ident == "axpy" && e.args.size() == 3 &&
          e.args[1]->kind == Expr::Kind::Ident && e.args[2]->kind == Expr::Kind::Ident &&
          g_array_sizes.count(e.args[1]->ident) > 0 &&
          g_array_sizes.count(e.args[2]->ident) > 0 &&
          e.args[0]->kind == Expr::Kind::FloatLit) {
        MirInsn ins;
        ins.op = MirOp::ArrayAxpyF64;
        ins.int_value = g_array_sizes[e.args[1]->ident];
        ins.float_value = e.args[0]->float_value;
        ins.lhs_ident = e.args[1]->ident;
        ins.rhs_ident = e.args[2]->ident;
        ins.rhs_is_literal = true;
        out.push_back(std::move(ins));
        // Walker consumes no counter for axpy (result name is the previous
        // tc, never emitted), so return a name without advancing the counter.
        return "__t" + std::to_string(temp_counter);
      }
      const ProcDecl* callee = find_proc(module, e.ident);
      if (callee && !callee->is_extern) {
        MirInsn ins;
        ins.op = MirOp::CallProc;
        ins.callee = e.ident;
        for (std::size_t ai = 0; ai < e.args.size(); ++ai) {
          const Expr& arg = *e.args[ai];
          const bool object_arg =
              ai < callee->params.size() &&
              callee->params[ai].type.kind == TypeKind::Named &&
              g_object_types.count(callee->params[ai].type.name) > 0 &&
              arg.kind == Expr::Kind::Ident && g_object_vars.count(arg.ident) > 0;
          if (object_arg) {
            for (const auto& field : g_object_types[callee->params[ai].type.name]) {
              MirArg field_arg;
              field_arg.ident = "__li_o_" + arg.ident + "_" + field.name;
              ins.args.push_back(std::move(field_arg));
            }
            continue;
          }
          MirArg ma;
          if (arg.kind == Expr::Kind::IntLit) {
            ma.is_literal = true;
            ma.int_value = arg.int_value;
          } else if (arg.kind == Expr::Kind::FloatLit) {
            // CallProc passes float literals inline (walker ARG
            // is_float_literal), unlike CallExtern which materializes temps.
            ma.is_float_literal = true;
            ma.float_value = arg.float_value;
          } else if (arg.kind == Expr::Kind::StringLit) {
            ma.is_string = true;
            ma.str_value = arg.str_value;
          } else if (arg.kind == Expr::Kind::Ident) {
            ma.ident = arg.ident;
            // Array/matrix args pass by address (walker ARG is_array_ident).
            if (is_array_ident(arg.ident)) {
              ma.is_array_ident = true;
            }
          } else {
            ma.ident = lower_expr_to(arg, module, out, float_names, float_arrays);
          }
          ins.args.push_back(std::move(ma));
        }
        if (callee->ret_type && callee->ret_type->kind == TypeKind::Named &&
            g_object_types.count(callee->ret_type->name) > 0) {
          for (const auto& field : g_object_types[callee->ret_type->name]) {
            MirParam layout;
            layout.name = field.name;
            layout.is_float = field.is_float;
            layout.is_i64 = field.is_i64;
            layout.is_array = field.array_elems > 0;
            layout.array_size = static_cast<int>(field.array_elems);
            layout.fixed_array_elems = field.array_elems;
            ins.object_layout.push_back(std::move(layout));
          }
        }
        if (callee->ret_type && callee->ret_type->name == "unit") {
          // Walker rd==0 (unit): INS 8 with no dest ident and no counter
          // consumed (mir_lower_expr mir_mk_name code 2 empty).
          out.push_back(std::move(ins));
          return "";
        }
        const std::string dest = fresh_temp();
        ins.ident = dest;
        if (callee->ret_type && is_float_type_name(callee->ret_type->name)) {
          ins.ret_is_float = true;
          float_names.insert(dest);
        } else if (callee->ret_type &&
                   (callee->ret_type->kind == TypeKind::Array ||
                    is_ptr_width_type_name(callee->ret_type->name))) {
          // Walker rd 3/4/5 (array, str/bytes/ptr/i64) set ret_is_i64;
          // int (rd 1) and objects (rd 6) keep 0, so Named alone must not
          // imply the bit.
          ins.ret_is_i64 = true;
        }
        out.push_back(std::move(ins));
        return dest;
      }
      MirInsn ins;
      ins.op = MirOp::CallExtern;
      ins.callee = e.ident;
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
          ma.ident = lower_expr_to(*arg, module, out, float_names, float_arrays);
        }
        ins.args.push_back(std::move(ma));
      }
      if (callee && callee->is_extern && callee->ret_type &&
          callee->ret_type->name != "unit") {
        const std::string dest = fresh_temp();
        ins.ident = dest;
        if (is_ptr_width_type_name(callee->ret_type->name)) {
          // Walker ret descriptor: rd 3/4 (str/bytes/ptr/i64) extern calls
          // set both is_i64 (f31) and ret_is_i64 (f20); int (rd 1) stays 0.
          ins.is_i64 = true;
          ins.ret_is_i64 = true;
        } else if (is_float_type_name(callee->ret_type->name)) {
          ins.ret_is_float = true;
          float_names.insert(dest);
        }
        out.push_back(std::move(ins));
        return dest;
      }
      out.push_back(std::move(ins));
      return fresh_temp();
    }
    case Expr::Kind::StringLit: {
      // String literal in expr position (e.g. `a[i] = "str"` on a ptr
      // array): materialize the string global into an i64 temp (walker INS 27
      // StoreI64 with rhs_is_string=1), then the store references the temp.
      const std::string dest = fresh_temp();
      MirInsn ins;
      ins.op = MirOp::StoreI64;
      ins.ident = dest;
      ins.rhs_is_literal = true;
      ins.rhs_is_string = true;
      ins.str_value = e.str_value;
      out.push_back(std::move(ins));
      return dest;
    }
    case Expr::Kind::Index: {
      // 2D index A[row][col]: base is itself an Index over a matrix ident.
      if (e.base && e.base->kind == Expr::Kind::Index && e.base->base &&
          e.base->base->kind == Expr::Kind::Ident && e.index) {
        const std::string& mat = e.base->base->ident;
        const auto lit = g_matrices.find(mat);
        if (lit != g_matrices.end()) {
          // Local matrix -> ArrayLoad2DF64 (walker f2d load): int_value=row,
          // rhs_int=col, dest in lhs_ident, index_is_literal/rhs_is_literal
          // for literal row/col.
          MirInsn load;
          load.op = MirOp::ArrayLoad2DF64;
          load.ident = mat;
          if (e.base->index && e.base->index->kind == Expr::Kind::IntLit) {
            load.index_is_literal = true;
            load.int_value = e.base->index->int_value;
          } else if (e.base->index && e.base->index->kind == Expr::Kind::Ident) {
            load.index_is_literal = false;
            load.index_ident = e.base->index->ident;
          }
          if (e.index->kind == Expr::Kind::IntLit) {
            load.rhs_is_literal = true;
            load.rhs_int = e.index->int_value;
          } else if (e.index->kind == Expr::Kind::Ident) {
            load.rhs_is_literal = false;
            load.rhs_ident = e.index->ident;
          }
          const std::string dest = fresh_temp();
          load.lhs_ident = dest;
          out.push_back(std::move(load));
          return dest;
        }
        // Matrix param -> the walker allocates a temp but emits NOTHING
        // (mir_lower_expr `var cp = tc[0]` no-emit quirk), so the result is
        // referenced but never defined.
        if (g_matrix_params.count(mat) > 0) {
          return fresh_temp();
        }
      }
      if (e.base && e.base->kind == Expr::Kind::Field && e.index &&
          e.base->base && e.base->base->kind == Expr::Kind::Ident &&
          e.base->index && e.base->index->kind == Expr::Kind::Ident) {
        const std::string base = e.base->base->ident;
        const std::string field = e.base->index->ident;
        const auto owner = g_object_vars.find(base);
        if (owner != g_object_vars.end()) {
          const auto fields = g_object_types.find(owner->second);
          if (fields != g_object_types.end()) {
            for (const auto& f : fields->second) {
              if (f.name != field || f.array_elems <= 0) {
                continue;
              }
              MirInsn load;
              load.op = f.is_float ? MirOp::ArrayLoadFloat : MirOp::ArrayLoadInt;
              load.ident = "__li_o_" + base + "_" + field;
              if (e.index->kind == Expr::Kind::IntLit) {
                load.index_is_literal = true;
                load.int_value = e.index->int_value;
              } else if (e.index->kind == Expr::Kind::Ident) {
                load.index_is_literal = false;
                load.index_ident = e.index->ident;
              } else {
                load.index_is_literal = false;
                load.index_ident = lower_expr_to(*e.index, module, out,
                                                 float_names, float_arrays);
              }
              const std::string dest = fresh_temp();
              load.lhs_ident = dest;
              out.push_back(std::move(load));
              return dest;
            }
          }
        }
      }
      if (e.base && e.base->kind == Expr::Kind::Ident && e.index) {
        MirInsn load;
        load.op = float_arrays.count(e.base->ident) > 0 ? MirOp::ArrayLoadFloat
                                                        : MirOp::ArrayLoadInt;
        load.ident = e.base->ident;
        if (e.index->kind == Expr::Kind::IntLit) {
          load.index_is_literal = true;
          load.int_value = e.index->int_value;
        } else if (e.index->kind == Expr::Kind::Ident) {
          load.index_is_literal = false;
          load.index_ident = e.index->ident;
        } else {
          // General index expr (e.g. board[cell_index(5, 7)]): lower it to a
          // temp first (walker lowers idx before allocating the load dest).
          load.index_is_literal = false;
          load.index_ident = lower_expr_to(*e.index, module, out, float_names, float_arrays);
        }
        const std::string dest = fresh_temp();
        load.lhs_ident = dest;
        out.push_back(std::move(load));
        return dest;
      }
      return fresh_temp();
    }
    default:
      return fresh_temp();
  }
}

void lower_echo_arg(const Expr& arg, const Module& module, std::vector<MirInsn>& out,
                    std::unordered_set<std::string>& float_names,
                    std::unordered_set<std::string>& float_arrays) {
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
    // General arg (e.g. `print(a[i])`): lower to a temp, then echo it
    // (walker lower_echo_arg falls back to mir_lower_expr + EchoInt).
    ins.op = MirOp::EchoInt;
    ins.ident = lower_expr_to(arg, module, out, float_names, float_arrays);
  }
  out.push_back(std::move(ins));
}

void lower_return_expr(const Expr& e, bool returns_float, bool returns_i64,
                       const Module& module, std::vector<MirInsn>& out,
                       std::unordered_set<std::string>& float_names,
                       std::unordered_set<std::string>& float_arrays) {
  MirInsn ins;
  if (e.kind == Expr::Kind::IntLit) {
    ins.op = MirOp::ReturnInt;
    ins.int_value = e.int_value;
  } else if (e.kind == Expr::Kind::FloatLit) {
    ins.op = MirOp::ReturnFloat;
    ins.float_value = e.float_value;
  } else if (e.kind == Expr::Kind::Ident) {
    const auto ovit = g_object_vars.find(e.ident);
    if (ovit != g_object_vars.end()) {
      // Returning an object var -> ReturnObject (INS 4) packs the per-field
      // slots __li_o_<name>_<field>; OBJ layout lines follow (walker
      // mir_return objs: emit INS 4 + OBJ per field).
      ins.op = MirOp::ReturnObject;
      ins.ident = "__li_o_" + e.ident;
      for (const auto& f : g_object_types[ovit->second]) {
        MirParam fp;
        fp.name = f.name;
        fp.is_float = f.is_float;
        fp.is_i64 = f.is_i64;
        fp.is_array = f.array_elems > 0;
        fp.array_size = static_cast<int>(f.array_elems);
        fp.fixed_array_elems = f.array_elems;
        ins.object_layout.push_back(std::move(fp));
      }
    } else {
      ins.op = MirOp::ReturnIdent;
      ins.ident = e.ident;
      ins.ret_is_float = returns_float || float_names.count(e.ident) > 0;
      // Walker mir_return ri: the enclosing proc's reti64 bit OR the name's
      // itok membership (mir_name_i64 over the str/bytes/ptr/i64 registry).
      ins.ret_is_i64 = returns_i64 || g_i64_names.count(e.ident) > 0;
    }
  } else if (e.kind == Expr::Kind::Field) {
    // `return o.field` -> INS 3 with the flattened slot name (walker
    // mir_return rk==9: mir_ins_mangled op 3, fld_src_fallback=1, flag2=1
    // -> rhs_is_literal=1; all ret bits 0).
    std::string base;
    std::string field;
    if (e.base && e.base->kind == Expr::Kind::Ident) {
      base = e.base->ident;
    }
    if (e.index && e.index->kind == Expr::Kind::Ident) {
      field = e.index->ident;
    }
    ins.op = MirOp::ReturnIdent;
    ins.ident = "__li_o_" + base + "_" + field;
    ins.rhs_is_literal = true;
  } else if (e.kind == Expr::Kind::Await) {
    // `return await x` lowers to a bare ReturnVoid (walker mir_return kk==34:
    // async calls are lowered at the LLVM level, not emitted as MIR).
    ins.op = MirOp::ReturnVoid;
  } else if (e.kind == Expr::Kind::Call || e.kind == Expr::Kind::BinOp ||
             e.kind == Expr::Kind::Index || e.kind == Expr::Kind::UnaryMinus) {
    const std::string tmp = lower_expr_to(e, module, out, float_names, float_arrays);
    ins.op = MirOp::ReturnIdent;
    ins.ident = tmp;
    ins.ret_is_float = returns_float || is_float_expr(e, float_names, float_arrays);
    // Pointer-width returns (str/ptr/i64/array calls) set the i64 bit on the
    // ReturnIdent too (walker mir_return rd 3/4/5 -> reti64).
    if (e.kind == Expr::Kind::Call && !ins.ret_is_float) {
      const auto callee = std::find_if(module.procs.begin(), module.procs.end(),
                                       [&](const ProcDecl& p) { return p.name == e.ident; });
      if (callee != module.procs.end() && callee->ret_type) {
        if (returns_i64 || is_ptr_width_type_name(callee->ret_type->name) ||
            callee->ret_type->kind == TypeKind::Array) {
          ins.ret_is_i64 = true;
        }
      }
    }
  } else {
    ins.op = MirOp::ReturnVoid;
  }
  out.push_back(std::move(ins));
}

void lower_stmts(const std::vector<Stmt>& stmts, const Module& module, bool returns_float,
                 bool returns_i64, std::vector<MirInsn>& out,
                 std::unordered_set<std::string>& float_names,
                 std::unordered_set<std::string>& float_arrays);

void lower_stmt(const Stmt& stmt, const Module& module, bool returns_float, bool returns_i64,
                std::vector<MirInsn>& out, std::unordered_set<std::string>& float_names,
                std::unordered_set<std::string>& float_arrays) {
  switch (stmt.kind) {
    case Stmt::Kind::Return:
      if (!stmt.expr) {
        MirInsn ins;
        ins.op = MirOp::ReturnVoid;
        out.push_back(std::move(ins));
      } else {
        lower_return_expr(*stmt.expr, returns_float, returns_i64, module, out, float_names,
                          float_arrays);
      }
      break;
    case Stmt::Kind::VarDecl: {
      // Object VarDecl `var o: T`: allocate one scalar slot per field as
      // __li_o_<var>_<field> (LocalAllocInt for int fields, LocalAllocFloat
      // for float fields). `var b: T = a` copies every field slot of a.
      if (stmt.var_type.kind == TypeKind::Named &&
          g_object_types.count(stmt.var_type.name) > 0) {
        const auto& fields = g_object_types[stmt.var_type.name];
        g_object_vars[stmt.var_name] = stmt.var_type.name;
        for (const auto& f : fields) {
          const std::string field_ident = "__li_o_" + stmt.var_name + "_" + f.name;
          MirInsn alloc;
          alloc.op = f.array_elems > 0 ? MirOp::ArrayAlloc
                                       : f.is_float ? MirOp::LocalAllocFloat
                                                    : MirOp::LocalAllocInt;
          alloc.ident = field_ident;
          alloc.int_value = f.array_elems;
          // Walker contract: only the ArrayAlloc line (INS 9) carries the
          // float-array bit (f[32]); scalar float allocs (INS 35) leave it 0.
          alloc.array_is_float = f.array_elems > 0 && f.is_float;
          alloc.array_is_i64 = f.is_i64;
          if (f.array_elems > 0) {
            g_array_sizes[field_ident] = f.array_elems;
            if (f.is_float) {
              float_arrays.insert(field_ident);
            } else {
            }
          }
          out.push_back(std::move(alloc));
          if (f.is_float && f.array_elems == 0) {
            float_names.insert(field_ident);
          }
        }
        if (stmt.init && stmt.init->kind == Expr::Kind::Ident &&
            g_object_vars.count(stmt.init->ident) > 0) {
          // Whole-object copy: store each source field slot into the new one.
          for (const auto& f : fields) {
            const std::string dst = "__li_o_" + stmt.var_name + "_" + f.name;
            const std::string src = "__li_o_" + stmt.init->ident + "_" + f.name;
            if (f.array_elems > 0) {
              for (std::int64_t n = 0; n < f.array_elems; ++n) {
                MirInsn load;
                load.op = f.is_float ? MirOp::ArrayLoadFloat : MirOp::ArrayLoadInt;
                load.ident = src;
                load.index_is_literal = true;
                load.int_value = n;
                const std::string value = "__t" + std::to_string(temp_counter++);
                load.lhs_ident = value;
                out.push_back(std::move(load));
                MirInsn store;
                store.op = f.is_float ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
                store.ident = dst;
                store.index_is_literal = true;
                store.int_value = n;
                store.rhs_ident = value;
                store.rhs_is_literal = false;
                out.push_back(std::move(store));
              }
            } else {
              MirInsn st;
              st.op = f.is_float ? MirOp::StoreFloat : MirOp::StoreInt;
              st.ident = dst;
              st.rhs_is_literal = false;
              st.rhs_ident = src;
              out.push_back(std::move(st));
            }
          }
        } else if (stmt.init && stmt.init->kind == Expr::Kind::Call) {
          // Object-returning calls use the same flattened field slots as a
          // local object. Lower the call once, then copy each returned leaf.
          const std::string tmp =
              lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
          for (const auto& f : fields) {
            const std::string dst = "__li_o_" + stmt.var_name + "_" + f.name;
            const std::string src = tmp + "_" + f.name;
            if (f.array_elems > 0) {
              for (std::int64_t n = 0; n < f.array_elems; ++n) {
                MirInsn load;
                load.op = f.is_float ? MirOp::ArrayLoadFloat : MirOp::ArrayLoadInt;
                load.ident = src;
                load.index_is_literal = true;
                load.int_value = n;
                const std::string value = "__t" + std::to_string(temp_counter++);
                load.lhs_ident = value;
                out.push_back(std::move(load));
                MirInsn store;
                store.op = f.is_float ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
                store.ident = dst;
                store.index_is_literal = true;
                store.int_value = n;
                store.rhs_ident = value;
                store.rhs_is_literal = false;
                out.push_back(std::move(store));
              }
            } else {
              MirInsn st;
              st.op = f.is_float ? MirOp::StoreFloat : MirOp::StoreInt;
              st.ident = dst;
              st.rhs_ident = src;
              st.rhs_is_literal = false;
              out.push_back(std::move(st));
            }
          }
        }
        break;
      }
      if (stmt.var_type.kind == TypeKind::Array && stmt.var_type.elem &&
          stmt.var_type.elem->kind == TypeKind::Array && stmt.var_type.elem->elem) {
        // Matrix VarDecl `var A: array[M, array[K, float]]`: the walker emits
        // ArrayAlloc with int_value=M (rows), rhs_int=K (cols),
        // array_is_float=1 + array_is_matrix=1, and DROPS any init (the
        // matmul/call init is never lowered; only the alloc + registry entry
        // remain).
        MirInsn ins;
        ins.op = MirOp::ArrayAlloc;
        ins.ident = stmt.var_name;
        ins.int_value = stmt.var_type.array_size;
        ins.rhs_int = stmt.var_type.elem->array_size;
        ins.array_is_float = true;
        ins.array_is_matrix = true;
        g_matrices[stmt.var_name] = {stmt.var_type.array_size,
                                     stmt.var_type.elem->array_size};
        out.push_back(std::move(ins));
      } else if (stmt.var_type.kind == TypeKind::Array && stmt.var_type.elem) {
        MirInsn ins;
        ins.op = MirOp::ArrayAlloc;
        ins.ident = stmt.var_name;
        ins.int_value = stmt.var_type.array_size;
        // Walker sets the array_is_float flag on ArrayAlloc for float arrays
        // and uses it to pick ArrayStoreFloat / BinOpFloat on elements.
        if (is_float_type_name(stmt.var_type.elem->name)) {
          ins.array_is_float = true;
          float_arrays.insert(stmt.var_name);
        } else {
        }
        g_array_sizes[stmt.var_name] = stmt.var_type.array_size;
        out.push_back(std::move(ins));
        // Walker direct-into-var array init: `var c = a + b` -> ArrayBinOpF64
        // into c (no temp); `var y = 2.0 * x` -> ArrayScaleF64 into y. Any
        // other init (e.g. an invalid-broadcast `var c: array[4,int] = a*b`
        // with array[2]*array[4]) lowers via the generic expr path into a
        // discarded temp (walker mir_var_decl fallthrough) and stores nothing.
        if (stmt.init) {
          if (emit_array_binop_into(*stmt.init, stmt.var_name, float_arrays, out)) {
            break;
          }
          if (emit_array_scale_into(*stmt.init, stmt.var_name, float_arrays, out)) {
            break;
          }
          (void)lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
        }
      } else if (is_ptr_width_type_name(stmt.var_type.name)) {
        // Walker var-decl ty 2/3/4 (str/string, bytes/StringView, ptr/i64):
        // LocalAllocI64, register in itok, StoreI64 init.
        MirInsn ins;
        ins.op = MirOp::LocalAllocI64;
        ins.ident = stmt.var_name;
        out.push_back(std::move(ins));
        g_i64_names.insert(stmt.var_name);
        if (stmt.init) {
          MirInsn store;
          store.op = MirOp::StoreI64;
          store.ident = stmt.var_name;
          if (stmt.init->kind == Expr::Kind::StringLit) {
            store.rhs_is_literal = true;
            store.rhs_is_string = true;
            store.str_value = stmt.init->str_value;
          } else if (stmt.init->kind == Expr::Kind::IntLit) {
            store.rhs_is_literal = true;
            store.rhs_int = stmt.init->int_value;
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp = lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      } else if (stmt.var_type.kind == TypeKind::TypeApp &&
                 stmt.var_type.name == "simd") {
        // Walker ty==8 (simd[f64, N]) var-decl: INS 36 LocalAllocSimdF64
        // with the lane count in the final field; the init expression is
        // dropped entirely (mir_var_decl ty 8 has no init emission).
        MirInsn ins;
        ins.op = MirOp::LocalAllocSimdF64;
        ins.ident = stmt.var_name;
        ins.simd_lanes = stmt.var_type.array_size;
        out.push_back(std::move(ins));
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
          } else if (stmt.init->kind == Expr::Kind::Ident) {
            store.rhs_is_literal = false;
            store.rhs_ident = stmt.init->ident;
          } else {
            const std::string tmp = lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
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
            const std::string tmp = lower_expr_to(*stmt.init, module, out, float_names, float_arrays);
            store.rhs_is_literal = false;
            store.rhs_ident = tmp;
          }
          out.push_back(std::move(store));
        }
      }
      break;
    }
    case Stmt::Kind::Assign:
      // `o.field[i] = value` uses the flattened array field slot.
      if (stmt.init && stmt.init->kind == Expr::Kind::Index && stmt.init->base &&
          stmt.init->base->kind == Expr::Kind::Field && stmt.expr &&
          stmt.init->base->base && stmt.init->base->base->kind == Expr::Kind::Ident &&
          stmt.init->base->index && stmt.init->base->index->kind == Expr::Kind::Ident) {
        const std::string base = stmt.init->base->base->ident;
        const std::string field = stmt.init->base->index->ident;
        const auto owner = g_object_vars.find(base);
        if (owner != g_object_vars.end()) {
          const auto fields = g_object_types.find(owner->second);
          if (fields != g_object_types.end()) {
            for (const auto& f : fields->second) {
              if (f.name != field || f.array_elems <= 0) {
                continue;
              }
              MirInsn ins;
              ins.op = f.is_float ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
              ins.ident = "__li_o_" + base + "_" + field;
              if (stmt.init->index->kind == Expr::Kind::IntLit) {
                ins.index_is_literal = true;
                ins.int_value = stmt.init->index->int_value;
              } else if (stmt.init->index->kind == Expr::Kind::Ident) {
                ins.index_is_literal = false;
                ins.index_ident = stmt.init->index->ident;
              } else {
                ins.index_is_literal = false;
                ins.index_ident = lower_expr_to(*stmt.init->index, module, out,
                                                float_names, float_arrays);
              }
              if (f.is_float && stmt.expr->kind == Expr::Kind::FloatLit) {
                ins.rhs_is_literal = true;
                ins.float_value = stmt.expr->float_value;
              } else if (!f.is_float && stmt.expr->kind == Expr::Kind::IntLit) {
                ins.rhs_is_literal = true;
                ins.rhs_int = stmt.expr->int_value;
              } else if (stmt.expr->kind == Expr::Kind::Ident) {
                ins.rhs_is_literal = false;
                ins.rhs_ident = stmt.expr->ident;
              } else {
                ins.rhs_is_literal = false;
                ins.rhs_ident = lower_expr_to(*stmt.expr, module, out,
                                              float_names, float_arrays);
              }
              out.push_back(std::move(ins));
              break;
            }
          }
        }
        break;
      }
      // `o.field = <rhs>` -> StoreInt/StoreFloat into the field slot
      // __li_o_<var>_<field> (walker mir_obj_field_store).
      if (stmt.init && stmt.init->kind == Expr::Kind::Field && stmt.expr) {
        const Expr* field_expr = stmt.init.get();
        if (field_expr->base && field_expr->base->kind == Expr::Kind::Ident &&
            field_expr->index && field_expr->index->kind == Expr::Kind::Ident) {
          const std::string& base = field_expr->base->ident;
          const std::string field = field_expr->index->ident;
          const auto vit = g_object_vars.find(base);
          if (vit != g_object_vars.end()) {
            const auto& fields = g_object_types[vit->second];
            bool is_float = false;
            for (const auto& f : fields) {
              if (f.name == field) {
                is_float = f.is_float;
                break;
              }
            }
            MirInsn ins;
            ins.op = is_float ? MirOp::StoreFloat : MirOp::StoreInt;
            ins.ident = "__li_o_" + base + "_" + field;
            if (is_float && stmt.expr->kind == Expr::Kind::FloatLit) {
              ins.rhs_is_literal = true;
              ins.float_value = stmt.expr->float_value;
            } else if (!is_float && stmt.expr->kind == Expr::Kind::IntLit) {
              ins.rhs_is_literal = true;
              ins.rhs_int = stmt.expr->int_value;
            } else if (stmt.expr->kind == Expr::Kind::Ident) {
              ins.rhs_is_literal = false;
              ins.rhs_ident = stmt.expr->ident;
            } else {
              ins.rhs_is_literal = false;
              ins.rhs_ident =
                  lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
            }
            out.push_back(std::move(ins));
            break;
          }
        }
      }
      // `C = A @ B` where A/B/C are LOCAL matrices -> ArrayMatMul2DF64 into C
      // (walker mir_stmt eopk==76 both-ident path). Only fires when both
      // operands are plain idents registered in matok; otherwise the RHS goes
      // through lower_expr_to (chain: inner ArrayAlloc+MatMul, outer BinOpInt).
      if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr &&
          stmt.expr->kind == Expr::Kind::BinOp && stmt.expr->bin_op == BinOp::MatMul &&
          stmt.expr->lhs && stmt.expr->rhs &&
          stmt.expr->lhs->kind == Expr::Kind::Ident &&
          stmt.expr->rhs->kind == Expr::Kind::Ident) {
        const auto la = g_matrices.find(stmt.expr->lhs->ident);
        const auto ra = g_matrices.find(stmt.expr->rhs->ident);
        if (la != g_matrices.end() && ra != g_matrices.end() &&
            la->second.second == ra->second.first) {
          MirInsn ins;
          ins.op = MirOp::ArrayMatMul2DF64;
          ins.int_value = la->second.first;   // rows of lhs
          ins.ident = stmt.init->ident;
          ins.lhs_ident = stmt.expr->lhs->ident;
          ins.rhs_ident = stmt.expr->rhs->ident;
          ins.rhs_int = la->second.second;    // cols of lhs
          ins.lhs_int = ra->second.second;    // cols of rhs
          out.push_back(std::move(ins));
          break;
        }
      }
      // `A[row][col] = value` on a LOCAL matrix -> ArrayStore2DF64.
      if (stmt.init && stmt.init->kind == Expr::Kind::Index && stmt.init->base &&
          stmt.init->base->kind == Expr::Kind::Index && stmt.init->base->base &&
          stmt.init->base->base->kind == Expr::Kind::Ident && stmt.expr) {
        const std::string& mat = stmt.init->base->base->ident;
        if (g_matrices.count(mat) > 0) {
          MirInsn ins;
          ins.op = MirOp::ArrayStore2DF64;
          ins.ident = mat;
          if (stmt.init->base->index &&
              stmt.init->base->index->kind == Expr::Kind::IntLit) {
            ins.index_is_literal = true;
            ins.int_value = stmt.init->base->index->int_value;
          } else if (stmt.init->base->index &&
                     stmt.init->base->index->kind == Expr::Kind::Ident) {
            ins.index_is_literal = false;
            ins.index_ident = stmt.init->base->index->ident;
          }
          if (stmt.init->index && stmt.init->index->kind == Expr::Kind::IntLit) {
            ins.rhs_is_literal = true;
            ins.rhs_int = stmt.init->index->int_value;
          } else if (stmt.init->index && stmt.init->index->kind == Expr::Kind::Ident) {
            ins.rhs_is_literal = false;
            ins.rhs_ident = stmt.init->index->ident;
          }
          if (stmt.expr->kind == Expr::Kind::FloatLit) {
            ins.lhs_is_literal = true;
            ins.float_value = stmt.expr->float_value;
          } else if (stmt.expr->kind == Expr::Kind::IntLit) {
            ins.lhs_is_literal = true;
            ins.rhs_int = 0;  // placeholder, unreachable for float matrices
            ins.lhs_int = stmt.expr->int_value;
          } else if (stmt.expr->kind == Expr::Kind::Ident) {
            ins.lhs_is_literal = false;
            ins.lhs_ident = stmt.expr->ident;
          } else {
            ins.lhs_ident = lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
            ins.lhs_is_literal = false;
          }
          out.push_back(std::move(ins));
          break;
        }
      }
      if (stmt.init && stmt.init->kind == Expr::Kind::Index && stmt.init->base &&
          stmt.init->base->kind == Expr::Kind::Ident && stmt.expr) {
        const bool arr_flt = float_arrays.count(stmt.init->base->ident) > 0;
        // Parallel-body context (walker ftmp 4089): float literals materialize
        // into a fresh __tN (INS 28 StoreFloat) and the store references the
        // temp by name as ArrayStoreInt (INS 10), NOT ArrayStoreFloat.
        if (g_in_parallel && arr_flt && stmt.expr->kind == Expr::Kind::FloatLit) {
          const std::string tmp = fresh_temp();
          MirInsn sf;
          sf.op = MirOp::StoreFloat;
          sf.ident = tmp;
          sf.float_value = stmt.expr->float_value;
          out.push_back(std::move(sf));
          MirInsn pstore;
          pstore.op = MirOp::ArrayStoreInt;
          pstore.ident = stmt.init->base->ident;
          if (stmt.init->index && stmt.init->index->kind == Expr::Kind::IntLit) {
            pstore.index_is_literal = true;
            pstore.int_value = stmt.init->index->int_value;
          } else if (stmt.init->index && stmt.init->index->kind == Expr::Kind::Ident) {
            pstore.index_is_literal = false;
            pstore.index_ident = stmt.init->index->ident;
          } else {
          pstore.index_is_literal = false;
          pstore.index_ident =
              lower_expr_to(*stmt.init->index, module, out, float_names, float_arrays);
        }
        // Value temp goes in the RHS name cell (walker f[12..14] code 5).
        pstore.rhs_ident = tmp;
        pstore.rhs_is_literal = false;
        out.push_back(std::move(pstore));
          break;
        }
        MirInsn ins;
        // Walker emits ArrayStoreFloat (INS 12) for float arrays, folding a
        // float literal into int_value with rhs_is_literal=1 (no temp).
        ins.op = arr_flt ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
        ins.ident = stmt.init->base->ident;
        if (stmt.init->index && stmt.init->index->kind == Expr::Kind::IntLit) {
          ins.index_is_literal = true;
          ins.int_value = stmt.init->index->int_value;
        } else if (stmt.init->index && stmt.init->index->kind == Expr::Kind::Ident) {
          ins.index_is_literal = false;
          ins.index_ident = stmt.init->index->ident;
        } else if (stmt.init->index) {
          // General index expr (e.g. board[cell_index(5, 7)] = 1): lower it
          // to a temp before the store (walker mir_stmt index lowering).
          ins.index_is_literal = false;
          ins.index_ident = lower_expr_to(*stmt.init->index, module, out, float_names, float_arrays);
        }
        if (arr_flt && stmt.expr->kind == Expr::Kind::FloatLit) {
          // Walker stores the literal in the float field (INS 12 0 <f>).
          ins.rhs_is_literal = true;
          ins.float_value = stmt.expr->float_value;
        } else if (stmt.expr->kind == Expr::Kind::IntLit && !arr_flt) {
          ins.rhs_is_literal = true;
          ins.rhs_int = stmt.expr->int_value;
        } else if (stmt.expr->kind == Expr::Kind::Ident) {
          ins.rhs_is_literal = false;
          ins.rhs_ident = stmt.expr->ident;
        } else {
          ins.rhs_ident = lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
          ins.rhs_is_literal = false;
        }
        out.push_back(std::move(ins));
      } else if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr &&
                 g_object_vars.count(stmt.init->ident) > 0 &&
                 stmt.expr->kind == Expr::Kind::Ident &&
                 g_object_vars.count(stmt.expr->ident) > 0) {
        const auto& fields = g_object_types[g_object_vars[stmt.init->ident]];
        for (const auto& f : fields) {
          const std::string dst = "__li_o_" + stmt.init->ident + "_" + f.name;
          const std::string src = "__li_o_" + stmt.expr->ident + "_" + f.name;
          if (f.array_elems > 0) {
            for (std::int64_t n = 0; n < f.array_elems; ++n) {
              MirInsn load;
              load.op = f.is_float ? MirOp::ArrayLoadFloat : MirOp::ArrayLoadInt;
              load.ident = src;
              load.index_is_literal = true;
              load.int_value = n;
              const std::string value = "__t" + std::to_string(temp_counter++);
              load.lhs_ident = value;
              out.push_back(std::move(load));
              MirInsn store;
              store.op = f.is_float ? MirOp::ArrayStoreFloat : MirOp::ArrayStoreInt;
              store.ident = dst;
              store.index_is_literal = true;
              store.int_value = n;
              store.rhs_ident = value;
              store.rhs_is_literal = false;
              out.push_back(std::move(store));
            }
          } else {
            MirInsn st;
            st.op = f.is_float ? MirOp::StoreFloat : MirOp::StoreInt;
            st.ident = dst;
            st.rhs_ident = src;
            st.rhs_is_literal = false;
            out.push_back(std::move(st));
          }
        }
        break;
      }
      if (stmt.init && stmt.init->kind == Expr::Kind::Ident && stmt.expr) {
        // Array/matrix assignment `c = <rhs>`. Array-op RHS lowers directly
        // into c (ArrayBinOpF64/I64, ArrayScaleF64); any other RHS (scalar
        // binop, chained matmul `C = (A@B) @ D`, call, ...) lowers via
        // lower_expr_to and stores the temp into c (walker StoreInt for a
        // matrix dest, StoreFloat/StoreInt by float-ness otherwise).
        if (is_array_ident(stmt.init->ident)) {
          if (emit_array_binop_into(*stmt.expr, stmt.init->ident, float_arrays, out)) {
            break;
          }
          if (emit_array_scale_into(*stmt.expr, stmt.init->ident, float_arrays, out)) {
            break;
          }
        }
        const bool flt = float_names.count(stmt.init->ident) > 0;
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
          ins.rhs_ident = lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
          ins.rhs_is_literal = false;
        }
        out.push_back(std::move(ins));
      }
      break;
    case Stmt::Kind::If: {
      if (!stmt.cond) {
        break;
      }
      const std::string cond_tmp = lower_expr_to(*stmt.cond, module, out, float_names, float_arrays);
      const std::string else_label = fresh_label("else_");
      const std::string merge_label = fresh_label("merge_");
      push_branch_if_zero(out, cond_tmp, else_label);
      lower_stmts(stmt.then_body, module, returns_float, returns_i64, out, float_names,
                  float_arrays);
      if (stmt.else_body) {
        push_jump_if_open(out, merge_label);
        push_label(out, else_label);
        lower_stmts(*stmt.else_body, module, returns_float, returns_i64, out, float_names,
                    float_arrays);
        push_label(out, merge_label);
      } else {
        push_label(out, else_label);
      }
      break;
    }
    case Stmt::Kind::Continue: {
      if (!while_head_labels.empty()) {
        push_jump(out, while_head_labels.back());
      }
      break;
    }
    case Stmt::Kind::Break: {
      if (!while_exit_labels.empty()) {
        push_jump(out, while_exit_labels.back());
      }
      break;
    }
    case Stmt::Kind::ParallelFor: {
      // `parallel for j in 0..<8 = body` lowers into a synthetic
      // __li_par_<proc>_<n> FN (one i64 PARAM, body + implicit ReturnVoid,
      // fresh label stack, shared temp counter) plus an OmpParallelFor (INS
      // 42) dispatch: int_value = start, rhs_int = end, callee = par-fn name.
      const std::string par_name =
          "__li_par_" + (g_cur_proc ? g_cur_proc->name : "main") + "_" +
          std::to_string(g_par_counter++);
      MirFn par_fn;
      par_fn.name = par_name;
      // Walker par FN header is all zeros (returns_void NOT set).
      MirParam p;
      p.name = stmt.par_index;
      p.is_i64 = true;
      par_fn.params.push_back(std::move(p));
      g_in_parallel = true;
      while_head_labels.clear();
      while_exit_labels.clear();
      lower_stmts(stmt.par_body, module, false, false, par_fn.body, float_names, float_arrays);
      g_in_parallel = false;
      MirInsn ret;
      ret.op = MirOp::ReturnVoid;
      par_fn.body.push_back(std::move(ret));
      g_par_fns.push_back(std::move(par_fn));
      g_uses_openmp = true;
      MirInsn disp;
      disp.op = MirOp::OmpParallelFor;
      disp.callee = par_name;
      disp.int_value = stmt.par_start;
      disp.rhs_int = stmt.par_end;
      out.push_back(std::move(disp));
      break;
    }
    case Stmt::Kind::For: {
      // `for <i> in <start>..<<end>` lowers exactly like the walker's
      // mir_for: alloc the iterator, seed `__t{el} = end` and `i = start`,
      // then a countdown head test `__t{df} = __t{el} - i` with
      // BranchIfZero to the exit label. A statement-level @vectorized
      // decorator wraps the body in ArraySimdScope (INS 49) enter/exit.
      const std::string head_label = fresh_label("for_head_");
      const std::string exit_label = fresh_label("for_exit_");
      while_head_labels.push_back(head_label);
      while_exit_labels.push_back(exit_label);
      MirInsn alloc;
      alloc.op = MirOp::LocalAllocInt;
      alloc.ident = stmt.for_index;
      out.push_back(std::move(alloc));
      const std::string el = fresh_temp();
      MirInsn store_end;
      store_end.op = MirOp::StoreInt;
      store_end.ident = el;
      store_end.rhs_is_literal = true;
      store_end.rhs_int = stmt.for_end;
      out.push_back(std::move(store_end));
      MirInsn store_start;
      store_start.op = MirOp::StoreInt;
      store_start.ident = stmt.for_index;
      store_start.rhs_is_literal = true;
      store_start.rhs_int = stmt.for_start;
      out.push_back(std::move(store_start));
      push_label(out, head_label);
      const std::string df = fresh_temp();
      MirInsn test;
      test.op = MirOp::BinOpInt;
      test.ident = df;
      test.lhs_ident = el;
      test.rhs_ident = stmt.for_index;
      test.bin_op = BinOp::Sub;
      out.push_back(std::move(test));
      push_branch_if_zero(out, df, exit_label);
      if (stmt.for_vectorized) {
        MirInsn enter;
        enter.op = MirOp::ArraySimdScope;
        enter.int_value = 1;
        out.push_back(std::move(enter));
      }
      lower_stmts(stmt.for_body, module, returns_float, returns_i64, out, float_names,
                  float_arrays);
      if (stmt.for_vectorized) {
        MirInsn exit;
        exit.op = MirOp::ArraySimdScope;
        exit.int_value = 0;
        out.push_back(std::move(exit));
      }
      const std::string on = fresh_temp();
      MirInsn store_one;
      store_one.op = MirOp::StoreInt;
      store_one.ident = on;
      store_one.rhs_is_literal = true;
      store_one.rhs_int = 1;
      out.push_back(std::move(store_one));
      MirInsn inc;
      inc.op = MirOp::BinOpInt;
      inc.ident = stmt.for_index;
      inc.lhs_ident = stmt.for_index;
      inc.rhs_ident = on;
      inc.bin_op = BinOp::Add;
      out.push_back(std::move(inc));
      push_jump_if_open(out, head_label);
      push_label(out, exit_label);
      while_head_labels.pop_back();
      while_exit_labels.pop_back();
      break;
    }
    case Stmt::Kind::While: {
      if (!stmt.cond) {
        break;
      }
      const std::string head_label = fresh_label("while_head_");
      const std::string exit_label = fresh_label("while_exit_");
      while_head_labels.push_back(head_label);
      while_exit_labels.push_back(exit_label);
      push_label(out, head_label);
      const std::string cond_tmp = lower_expr_to(*stmt.cond, module, out, float_names, float_arrays);
      push_branch_if_zero(out, cond_tmp, exit_label);
      lower_stmts(stmt.while_body, module, returns_float, returns_i64, out, float_names,
                  float_arrays);
      push_jump_if_open(out, head_label);
      push_label(out, exit_label);
      while_head_labels.pop_back();
      while_exit_labels.pop_back();
      break;
    }
    case Stmt::Kind::Expr:
      if (stmt.expr && stmt.expr->kind == Expr::Kind::Call) {
        // The walker treats print() exactly like echo(): EchoInt for int
        // args, EchoString for string literals (mir_stmt print dispatch).
        if ((stmt.expr->ident == "echo" || stmt.expr->ident == "print") &&
            !stmt.expr->args.empty()) {
          lower_echo_arg(*stmt.expr->args[0], module, out, float_names, float_arrays);
        } else if (stmt.expr->ident == "print_int" && !stmt.expr->args.empty()) {
          // print_int() is a prelude builtin — lower to CallExtern li_rt_print_int
          MirInsn ins;
          ins.op = MirOp::CallExtern;
          ins.callee = "li_rt_print_int";
          MirArg ma;
          if (stmt.expr->args[0]->kind == Expr::Kind::IntLit) {
            ma.is_literal = true;
            ma.int_value = stmt.expr->args[0]->int_value;
          } else if (stmt.expr->args[0]->kind == Expr::Kind::Ident) {
            ma.ident = stmt.expr->args[0]->ident;
          } else {
            ma.ident = lower_expr_to(*stmt.expr->args[0], module, out, float_names, float_arrays);
          }
          ins.args.push_back(std::move(ma));
          out.push_back(std::move(ins));
        } else {
          (void)lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
        }
      } else if (stmt.expr) {
        // `discard <expr>`: the walker swallows only the keyword and lowers the
        // value as a bare side-effecting expression statement (e.g. a string
        // literal materializes a StoreI64 temp, INS 27).
        const std::string tmp = lower_expr_to(*stmt.expr, module, out, float_names, float_arrays);
        (void)tmp;
      }
      break;
    default:
      break;
  }
}

void lower_stmts(const std::vector<Stmt>& stmts, const Module& module, bool returns_float,
                 bool returns_i64, std::vector<MirInsn>& out,
                 std::unordered_set<std::string>& float_names,
                 std::unordered_set<std::string>& float_arrays) {
  for (const auto& stmt : stmts) {
    lower_stmt(stmt, module, returns_float, returns_i64, out, float_names, float_arrays);
    if (!out.empty() &&
        (out.back().op == MirOp::ReturnVoid || out.back().op == MirOp::ReturnInt ||
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

bool str_prefix(const std::string& s, const std::string& p) {
  return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

// Mirror the walker's mir_scan_rt_flags (bootstrap/lic/main.li): scan every
// extern call site and set the MIR module runtime-link bits from the callee
// name's prefix. Only resolved extern names count; imported CallProc bodies
// must NOT set these flags. net_ping / path_ends_with_conf are exact matches.
void scan_runtime_flags(const Module& module, MirModule& mir) {
  std::unordered_set<std::string> externs;
  for (const auto& p : module.procs) {
    if (p.is_extern) {
      externs.insert(p.name);
    }
  }
  const auto classify = [&](const std::string& nm) {
    if (externs.count(nm) == 0) {
      return;
    }
    if (str_prefix(nm, "li_log_") || str_prefix(nm, "li_rt_log_")) {
      mir.needs_rt_log = true;
    }
    if (str_prefix(nm, "httpd_") || str_prefix(nm, "li_rt_httpd_") ||
        str_prefix(nm, "proxy_") || str_prefix(nm, "li_rt_proxy_")) {
      mir.needs_rt_httpd = true;
    }
    if (str_prefix(nm, "net_") || str_prefix(nm, "tcp_") || str_prefix(nm, "epoll_") ||
        str_prefix(nm, "hdr_") || str_prefix(nm, "buf_") || str_prefix(nm, "bytes_") ||
        str_prefix(nm, "li_rt_net") || str_prefix(nm, "li_async_") ||
        str_prefix(nm, "tcp_echo_") || nm == "path_ends_with_conf" || nm == "net_ping") {
      mir.needs_rt_net = true;
    }
  };
  std::function<void(const Expr&)> walk_expr = [&](const Expr& e) {
    if (e.kind == Expr::Kind::Call) {
      classify(e.ident);
      for (const auto& a : e.args) {
        walk_expr(*a);
      }
    } else if (e.kind == Expr::Kind::BinOp) {
      if (e.lhs) {
        walk_expr(*e.lhs);
      }
      if (e.rhs) {
        walk_expr(*e.rhs);
      }
    } else if (e.kind == Expr::Kind::Index) {
      if (e.base) {
        walk_expr(*e.base);
      }
      if (e.index) {
        walk_expr(*e.index);
      }
    } else if (e.kind == Expr::Kind::UnaryNot || e.kind == Expr::Kind::UnaryMinus) {
      if (e.operand) {
        walk_expr(*e.operand);
      }
    }
  };
  // Mirror the walker's token-level `ident(` scan (mir_scan_rt_flags): every
  // expression-bearing position of every statement is a potential extern call
  // site — return/assign/borrow RHS (s.expr), var-decl inits (s.init),
  // if/while conds (s.cond), and for/parallel bodies.
  std::function<void(const Stmt&)> walk_stmt = [&](const Stmt& s) {
    if (s.expr) {
      walk_expr(*s.expr);
    }
    if (s.init) {
      walk_expr(*s.init);
    }
    if (s.cond) {
      walk_expr(*s.cond);
    }
    if (s.kind == Stmt::Kind::If) {
      for (const auto& inner : s.then_body) {
        walk_stmt(inner);
      }
      if (s.else_body) {
        for (const auto& inner : *s.else_body) {
          walk_stmt(inner);
        }
      }
    } else if (s.kind == Stmt::Kind::While) {
      for (const auto& inner : s.while_body) {
        walk_stmt(inner);
      }
    } else if (s.kind == Stmt::Kind::ParallelFor) {
      for (const auto& inner : s.par_body) {
        walk_stmt(inner);
      }
    } else if (s.kind == Stmt::Kind::For) {
      for (const auto& inner : s.for_body) {
        walk_stmt(inner);
      }
    }
  };
  for (const auto& proc : module.procs) {
    if (!proc.is_extern) {
      for (const auto& s : proc.body) {
        walk_stmt(s);
      }
    }
  }
}

MirModule lower_to_mir(const Module& module) {
  temp_counter = 0;
  g_par_counter = 0;
  g_uses_openmp = false;
  g_in_parallel = false;
  MirModule mir;
  g_object_types.clear();
  // Collect every object alias first so nested object fields resolve
  // regardless of declaration/import order (module.types holds the merged
  // main + imported aliases in walker order). Nested object fields flatten
  // to their leaves with compound names (parent_leaf), matching the walker's
  // DFS leaf expansion (mir_obj_alloc_fields / mir_obj_retparams / OBJ).
  // Only a field whose type is a registered object alias recurses; array
  // fields (even of object elements) stay single array slots, exactly like
  // the walker, which never recurses arrays.
  std::unordered_map<std::string, const TypeAlias*> obj_aliases;
  for (const auto& alias : module.types) {
    if (alias.alias_kind == AliasKind::Object) {
      obj_aliases[alias.name] = &alias;
    }
  }
  std::function<void(const TypeAlias&, const std::string&, std::vector<std::string>&,
                     std::vector<ObjectField>&)>
      flatten_alias = [&](const TypeAlias& alias, const std::string& prefix,
                          std::vector<std::string>& path,
                          std::vector<ObjectField>& out) {
        for (const auto& f : alias.fields) {
          if (f.type && f.type->kind == TypeKind::Named) {
            const auto sub = obj_aliases.find(f.type->name);
            if (sub != obj_aliases.end() &&
                std::find(path.begin(), path.end(), f.type->name) == path.end()) {
              const std::string next =
                  prefix.empty() ? f.name : prefix + "_" + f.name;
              path.push_back(f.type->name);
              flatten_alias(*sub->second, next, path, out);
              path.pop_back();
              continue;
            }
          }
          ObjectField leaf;
          leaf.name = prefix.empty() ? f.name : prefix + "_" + f.name;
          if (f.type && f.type->kind == TypeKind::Array && f.type->elem) {
            leaf.array_elems = f.type->array_size;
            leaf.is_float = is_float_type_name(f.type->elem->name);
            leaf.is_i64 = is_pi2_i64_type_name(f.type->elem->name);
          } else if (f.type && f.type->kind == TypeKind::Named) {
            leaf.is_float = is_float_type_name(f.type->name);
            leaf.is_i64 = is_pi2_i64_type_name(f.type->name);
          }
          out.push_back(std::move(leaf));
        }
      };
  for (const auto& alias : module.types) {
    if (alias.alias_kind == AliasKind::Object) {
      std::vector<ObjectField> fields;
      std::vector<std::string> path{alias.name};
      flatten_alias(alias, "", path, fields);
      g_object_types[alias.name] = std::move(fields);
    }
  }
  scan_runtime_flags(module, mir);
  for (const auto& proc : module.procs) {
    MirFn fn;
    fn.name = proc.name;
    fn.is_extern = proc.is_extern;
    // Walker DEC side-channel (ftmp 4092..4095, 4086..4088): the FN line's
    // no_vectorize/async bits come from @no_vectorize / @async; a DEC line is
    // emitted for the first decorator (or for @cpu + @parallel separately).
    fn.is_async = fn.is_async || proc.is_async;
    for (const auto& d : proc.decorators) {
      fn.no_vectorize = fn.no_vectorize || d.no_vectorize;
      fn.is_async = fn.is_async || d.is_async;
    }
    if (!proc.decorators.empty()) {
      bool has_parallel = false;
      bool has_cpu = false;
      bool has_disjoint = false;
      for (const auto& d : proc.decorators) {
        has_parallel = has_parallel || d.parallel;
        has_cpu = has_cpu || d.cpu;
        has_disjoint = has_disjoint || d.disjoint;
      }
      if (has_parallel) {
        if (has_cpu) {
          MirDecorator cd;
          cd.name = "cpu";
          fn.decorators.push_back(std::move(cd));
        }
        MirDecorator pd;
        pd.name = "parallel";
        pd.parallel = true;
        pd.disjoint_proven = has_disjoint;
        fn.decorators.push_back(std::move(pd));
      } else {
        MirDecorator md;
        md.name = proc.decorators.front().name;
        md.lanes = proc.decorators.front().lanes;
        md.vectorized = proc.decorators.front().vectorized;
        fn.decorators.push_back(std::move(md));
      }
    }
    if (proc.ret_type) {
      fn.returns_float = is_float_type_name(proc.ret_type->name);
      fn.returns_void = proc.ret_type->name == "unit";
      // Walker reti64: mir_proc sets the bit for ret == 3 or ret == 4, i.e.
      // str/string (rd 2), bytes/StringView (rd 3) and ptr/int64/i64/long
      // (rd 4) — the pointer-width set; arrays (rd 5) and ints (rd 1) do not.
      fn.returns_i64 = is_ptr_width_type_name(proc.ret_type->name);
      // Object-typed return -> FN returns_object bit + one RETPARAM per field
      // (walker mir_proc: type stored as vt(0) object -> retobj=1; the return
      // slot is packed field-by-field and emitted as RETURN + OBJ layout).
      if (proc.ret_type->kind == TypeKind::Named &&
          g_object_types.count(proc.ret_type->name) > 0) {
        fn.returns_object = true;
        for (const auto& f : g_object_types[proc.ret_type->name]) {
          MirParam rp;
          rp.name = f.name;
          rp.is_float = f.is_float;
          rp.is_i64 = f.is_i64;
          rp.is_array = f.array_elems > 0;
          rp.array_size = static_cast<int>(f.array_elems);
          rp.fixed_array_elems = f.array_elems;
          fn.return_object_layout.push_back(std::move(rp));
        }
      }
    } else if (proc.is_extern) {
      fn.returns_void = true;
    }
    std::vector<std::pair<std::string, std::string>> obj_params;
    std::vector<std::string> i64_params;
    for (const auto& p : proc.params) {
      // Object-typed param -> flatten to one PARAM slot per field named
      // __li_o_<param>_<field> (walker mir_proc: object param type vt(0) is
      // expanded; body reads mangle field slots directly). is_float per field.
      if (p.type.kind == TypeKind::Named &&
          g_object_types.count(p.type.name) > 0) {
        for (const auto& f : g_object_types[p.type.name]) {
          MirParam op;
          op.name = "__li_o_" + p.name + "_" + f.name;
          op.is_float = f.is_float;
          op.is_i64 = f.is_i64;
          op.is_array = f.array_elems > 0;
          op.array_size = static_cast<int>(f.array_elems);
          op.fixed_array_elems = f.array_elems;
          // Walker is_var on flattened object PARAM lines mirrors the
          // param's `var` bit (mir_obj_param_line_r last field).
          op.is_var = p.type.is_var;
          fn.params.push_back(std::move(op));
        }
        obj_params.emplace_back(p.name, p.type.name);
        continue;
      }
      MirParam mp;
      mp.name = p.name;
      mp.is_float = is_float_type_name(p.type.name);
      // Walker mir_param_line ps: ty == 2 or ty == 3 — str/string AND
      // bytes/StringView all set the is_string slot.
      mp.is_string = is_str_bytes_type_name(p.type.name);
      if (p.type.kind == TypeKind::Array && p.type.elem) {
        // Walker PARAM layout (mir_param_line): array[N, float] -> is_float=1,
        // fixed_array_elems=N, is_i64=0; array[N, ptr|str|i64] elements are
        // pointer-width (is_i64=1).
        mp.is_array = true;
        mp.array_size = p.type.array_size;
        mp.fixed_array_elems = p.type.array_size;
        if (p.type.elem->kind == TypeKind::Array && p.type.elem->elem) {
          // array[M, array[K, float]] matrix param: is_float=1 (inner elem),
          // fixed_array_elems=M (rows), is_matrix=1, matrix_cols=K.
          mp.is_matrix = true;
          mp.matrix_cols = p.type.elem->array_size;
          mp.is_float = is_float_type_name(p.type.elem->elem->name);
        } else {
          mp.is_float = is_float_type_name(p.type.elem->name);
          mp.is_i64 = is_pi2_i64_type_name(p.type.elem->name);
        }
      } else {
        mp.is_i64 = is_pi2_i64_type_name(p.type.name);
      }
      // Walker itok: scalar pointer-width params (ty 2/3/4) register under
      // the param name; used for ReturnIdent's ret_is_i64 bit. Seeded into
      // g_i64_names after the per-proc reset below, like obj_params.
      if (is_ptr_width_type_name(p.type.name)) {
        i64_params.push_back(p.name);
      }
      // Walker is_var: only `var array[...]` params (collect sets preg(10)
      // from ti[1] for array types only; matrix params force 0 in
      // mir_param_line).
      mp.is_var = p.type.kind == TypeKind::Array && p.type.is_var && !mp.is_matrix;
      fn.params.push_back(std::move(mp));
    }
    if (!proc.is_extern) {
      g_cur_proc = &proc;
      // Per-proc registries: array sizes / int arrays must not leak across
      // procs (main.li is itself compiled by this lowerer, so a leak would
      // change the walker's own behavior).
      g_array_sizes.clear();
      g_matrices.clear();
      g_matrix_params.clear();
      g_par_fns.clear();
      g_object_vars.clear();
      g_i64_names.clear();
      for (const auto& op : obj_params) {
        g_object_vars[op.first] = op.second;
      }
      for (const auto& ip : i64_params) {
        g_i64_names.insert(ip);
      }
      std::unordered_set<std::string> float_names;
      std::unordered_set<std::string> float_arrays;
      seed_float_params(fn, float_names, float_arrays);
      if (fn.is_async) {
        // Walker emits AsyncFrameEnter (INS 47) + AsyncFrameLeave (INS 48)
        // at the top of every async proc body, before the first statement.
        MirInsn enter;
        enter.op = MirOp::AsyncFrameEnter;
        fn.body.push_back(std::move(enter));
        MirInsn leave;
        leave.op = MirOp::AsyncFrameLeave;
        fn.body.push_back(std::move(leave));
      }
      lower_stmts(proc.body, module, fn.returns_float, fn.returns_i64, fn.body, float_names,
                  float_arrays);
      append_implicit_return(fn.body);
      g_cur_proc = nullptr;
      mir.uses_async = mir.uses_async || fn.is_async;
    }
    if (!proc.is_extern) {
      // Synthesized __li_par_* functions replay BEFORE the enclosing FN
      // (walker hold-buffer swap), so splice them in ahead of this proc.
      for (auto& pf : g_par_fns) {
        mir.functions.push_back(std::move(pf));
      }
      g_par_fns.clear();
    }
    if (!proc.is_extern && fn.body.empty()) {
      MirInsn ins;
      ins.op = MirOp::ReturnVoid;
      fn.body.push_back(std::move(ins));
    }
    mir.functions.push_back(std::move(fn));
  }
  mir.uses_openmp = g_uses_openmp;
  return mir;
}

}  // namespace li
