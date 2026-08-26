#pragma once

#include "li/ast.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace li {

enum class MirOp {
  ReturnVoid,
  ReturnInt,
  ReturnFloat,
  ReturnIdent,
  /** Pack locals named `ident + "_" + layout[i].name` into LLVM struct return (scalars or
   *  fixed arrays as `[N x T]` aggregates). */
  ReturnObject,
  EchoInt,
  EchoString,
  CallExtern,
  CallProc,
  ArrayAlloc,
  ArrayStoreInt,
  ArrayLoadInt,
  ArrayStoreFloat,
  ArrayLoadFloat,
  ArrayDotF64,
  /** Element of `array[M, array[N, float]]`; row=int_value/index_ident, col=rhs_int/lhs_ident */
  ArrayLoad2DF64,
  ArrayStore2DF64,
  /** C[M,N] = A[M,K] @ B[K,N] — nested `array[M, array[K, float]]`; M=int_value, K=rhs_int, N=lhs_int */
  ArrayMatMul2DF64,
  ArraySumF64,
  ArraySumI64,
  /** Element-wise binop into `ident` from `lhs_ident` and `rhs_ident` (length `int_value`). */
  ArrayBinOpF64,
  ArrayBinOpI64,
  /** `dest[i] = scale * lhs[i]` — scale in `rhs_ident` (float local) or `float_value` if literal. */
  ArrayScaleF64,
  /** `rhs[i] = scale * lhs[i] + rhs[i]` — scale in `ident`, lhs=x, rhs=y. */
  ArrayAxpyF64,
  LocalAllocInt,
  LocalAllocI64,
  StoreInt,
  StoreI64,
  StoreFloat,
  LoadIntToIdent,
  BinOpInt,
  BinOpFloat,
  /** `ident = lhs_ident * rhs_ident + float_value` (LLVM fmuladd) — horner / FMA chains */
  FmaFloatF64,
  /** Chained fmuladd: ident = fma(lhs_ident, ident, float_value) repeated int_value times (SSA). */
  HornerFmaUnroll,
  /** acc = acc * x^4 + (1+x+x²+x³); lhs_is_literal, float_value = const x; int_value = supersteps. */
  HornerStepPow4,
  LocalAllocFloat,
  LocalAllocSimdF64,
  SimdSplatF64,
  SimdMulF64,
  SimdAddF64,
  SimdHorizSumF64,
  SimdCopyF64,
  OmpParallelFor,
  Label,
  Jump,
  BranchIfZero,
  AsyncAwait,
  AsyncFrameEnter,
  AsyncFrameLeave,
  /** Push/pop scoped array SIMD: int_value 1=enable, 0=pop (pairs with `@vectorized` on `for`). */
  ArraySimdScope,
};

struct MirArg {
  bool is_literal = false;
  std::int64_t int_value = 0;
  bool is_float_literal = false;
  double float_value = 0.0;
  std::string ident;
  bool is_string = false;
  std::string str_value;
  /** Pass `ident` array alloca by address (CallProc array param). */
  bool is_array_ident = false;
  /** Pass `ident` scalar slot by address (CallProc `var` object field param). */
  bool is_var_ref = false;
};

struct MirParam {
  std::string name;
  bool is_float = false;
  bool is_string = false;
  bool is_i64 = false;
  bool is_simd_f64 = false;
  std::int64_t simd_lanes = 0;
  /** When >0, slot is `ident + "_" + name` as ArrayAlloc; LLVM uses `[N x scalar]` in structs. */
  std::int64_t fixed_array_elems = 0;
  /** `array[M, array[K, float]]` param: rows in fixed_array_elems, cols here. */
  bool is_matrix = false;
  std::int64_t matrix_cols = 0;
  /** `var` array param: passed by reference; callee writes propagate back. */
  bool is_var = false;
};

struct MirInsn {
  MirOp op = MirOp::ReturnVoid;
  std::int64_t int_value = 0;
  double float_value = 0.0;
  std::string ident;
  std::string str_value;
  std::string callee;
  std::string lhs_ident;
  std::string rhs_ident;
  std::string label;
  BinOp bin_op = BinOp::Add;
  bool ret_is_float = false;
  bool ret_is_i64 = false;
  bool index_is_literal = true;
  std::string index_ident;
  bool use_loaded_int = false;
  bool rhs_is_literal = true;
  std::int64_t rhs_int = 0;
  /** StoreI64 from a string literal (bytes/str/StringView init): emit the global and store its ptr. */
  bool rhs_is_string = false;
  bool lhs_is_literal = false;
  std::int64_t lhs_int = 0;
  bool is_i64 = false;
  bool array_is_float = false;
  /** `array[N, ptr|i64|str|bytes|StringView]`: elements are pointer-width. */
  bool array_is_i64 = false;
  /** `array[M, array[K, float]]` row-major tile; cols in rhs_int when true. */
  bool array_is_matrix = false;
  /** Element-wise op: other operand is `array[1, *]` — use its index 0 at every lane. */
  bool array_broadcast_lhs_len1 = false;
  bool array_broadcast_rhs_len1 = false;
  std::int64_t simd_lanes = 0;
  std::vector<MirArg> args;
  /** Layout entries under object root (`name` paths). Used for ReturnObject pack and CallProc
   *  unpack into `ident + "_" + name` (scalar locals or ArrayAlloc slots). */
  std::vector<MirParam> object_layout;
};

struct MirDecorator {
  std::string name;
  /** `@vectorized(lanes=N)` when name is vectorized; 0 if omitted. */
  std::int64_t lanes = 0;
  /** `@vectorized` on the owning `def` (7d-b MIR proc tag). */
  bool vectorized = false;
  bool parallel = false;
  bool disjoint_proven = false;
};

struct MirFn {
  std::string name;
  bool returns_float = false;
  /** When true, LLVM return type is i8* (ptr / int64 ABI). */
  bool returns_i64 = false;
  bool returns_void = false;
  /** When true, LLVM return type is a struct; `return_object_layout` lists leaf fields. */
  bool returns_object = false;
  bool is_extern = false;
  bool is_async = false;
  /** When true, `ArrayDotF64` / `ArrayBinOpF64` use scalar loops only. */
  bool no_vectorize = false;
  std::vector<MirDecorator> decorators;
  std::vector<MirParam> params;
  /** Populated when `returns_object`; parallel to ReturnObject / unpack layout. */
  std::vector<MirParam> return_object_layout;
  std::vector<MirInsn> body;
};

struct MirModule {
  std::vector<MirFn> functions;
  bool uses_openmp = false;
  bool uses_async = false;
  /** Link runtime/li_rt_httpd.c when MIR calls httpd routing/config symbols. */
  bool needs_rt_httpd = false;
  /** Link runtime/li_rt_net.c when MIR calls socket/epoll/proxy symbols. */
  bool needs_rt_net = false;
  /** Link runtime/li_rt_log.c when MIR calls li_log_* symbols. */
  bool needs_rt_log = false;
  /** When true: MIR stability pass + strict FP codegen (no fast-math reassociation). */
  bool fp_numerically_stable = false;
};

/** Count `def` decorators with {@link MirDecorator::vectorized}. */
std::size_t count_mir_vectorized_proc(const MirModule& mir);
std::size_t count_mir_parallel_disjoint_proven(const MirModule& mir);

MirModule lower_to_mir(const Module& module);

}  // namespace li
