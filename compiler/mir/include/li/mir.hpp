#pragma once

#include "li/ast.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace li {

// NOTE: the enumerator ORDER is part of the MIR dump ABI. The self-hosted
// walker (bootstrap/lic/main.li) emits these exact numeric opcodes, so the
// order below must match the walker's emitter constants (StoreInt=26,
// BinOpInt=30, Label=43, Jump=44, BranchIfZero=45, ...). The reduced subset
// used by lower_to_mir is a prefix of this enum; the extra members document
// ops the walker can emit for decorator/async/object code.
enum class MirOp {
  ReturnVoid,
  ReturnInt,
  ReturnFloat,
  ReturnIdent,
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
  ArrayLoad2DF64,
  ArrayStore2DF64,
  ArrayMatMul2DF64,
  ArraySumF64,
  ArraySumI64,
  ArrayBinOpF64,
  ArrayBinOpI64,
  ArrayScaleF64,
  ArrayAxpyF64,
  LocalAllocInt,
  LocalAllocI64,
  StoreInt,
  StoreI64,
  StoreFloat,
  LoadIntToIdent,
  BinOpInt,
  BinOpFloat,
  FmaFloatF64,
  HornerFmaUnroll,
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
  bool is_array = false;
  int array_size = 0;
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
  bool needs_rt_httpd = false;
  bool needs_rt_net = false;
  bool needs_rt_log = false;
  bool fp_numerically_stable = false;
};

MirModule lower_to_mir(const Module& module);

}  // namespace li
