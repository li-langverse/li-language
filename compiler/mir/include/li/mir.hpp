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
  EchoInt,
  EchoString,
  CallExtern,
  CallProc,
  ArrayAlloc,
  ArrayStoreInt,
  ArrayLoadInt,
  LocalAllocInt,
  LocalAllocI64,
  StoreInt,
  StoreI64,
  StoreFloat,
  LoadIntToIdent,
  BinOpInt,
  BinOpFloat,
  LocalAllocFloat,
  Label,
  Jump,
  BranchIfZero,
};

struct MirArg {
  bool is_literal = false;
  std::int64_t int_value = 0;
  std::string ident;
  bool is_string = false;
  std::string str_value;
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
  bool index_is_literal = true;
  std::string index_ident;
  bool use_loaded_int = false;
  bool rhs_is_literal = true;
  std::int64_t rhs_int = 0;
  bool lhs_is_literal = false;
  std::int64_t lhs_int = 0;
  bool is_i64 = false;
  std::vector<MirArg> args;
};

struct MirParam {
  std::string name;
  bool is_float = false;
  bool is_string = false;
  bool is_i64 = false;
};

struct MirFn {
  std::string name;
  bool returns_float = false;
  bool returns_void = false;
  bool is_extern = false;
  std::vector<MirParam> params;
  std::vector<MirInsn> body;
};

struct MirModule {
  std::vector<MirFn> functions;
};

MirModule lower_to_mir(const Module& module);

}  // namespace li
