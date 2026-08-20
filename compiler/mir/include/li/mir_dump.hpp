#pragma once

#include "li/mir.hpp"

#include <string>

namespace li {

/// Canonical byte-exact dump of `lower_to_mir()` output used as the Layer 5
/// self-host parity contract.
///
/// `lic mir <file>` prints the lowered MIR as a deterministic stream; the li
/// bootstrap backend (bootstrap/lic/main.li `mir <file>` subcommand) must emit
/// the byte-identical stream from its own AST -> MIR lowering, and
/// scripts/check_li_mir_parity.sh diffs the two.
///
/// Format (one field per token, space-separated, one record per line):
///
///   MIR <fn_count> <uses_openmp> <uses_async> <needs_rt_httpd> <needs_rt_net>
///       <needs_rt_log> <fp_numerically_stable>
///   FN <name> <returns_float> <returns_i64> <returns_void> <returns_object>
///      <is_extern> <is_async> <no_vectorize>
///   DEC <name> <lanes> <vectorized> <parallel> <disjoint_proven>
///   PARAM <name> <is_float> <is_string> <is_i64> <is_simd_f64> <simd_lanes>
///         <fixed_array_elems> <is_matrix> <matrix_cols> <is_var>
///   RETPARAM <name> ...          (same fields as PARAM; return object layout)
///   INS <op> <int_value> <float_value> <ident> <str_value> <callee>
///       <lhs_ident> <rhs_ident> <label> <bin_op> <ret_is_float> <ret_is_i64>
///       <index_is_literal> <index_ident> <use_loaded_int> <rhs_is_literal>
///       <rhs_int> <rhs_is_string> <lhs_is_literal> <lhs_int> <is_i64>
///       <array_is_float> <array_is_matrix> <array_broadcast_lhs_len1>
///       <array_broadcast_rhs_len1> <simd_lanes>
///   ARG <is_literal> <int_value> <is_float_literal> <float_value> <ident>
///       <is_string> <str_value> <is_array_ident> <is_var_ref>
///   OBJ <name> ...               (same fields as PARAM; insn object layout)
///
/// Boolean/int fields are emitted as 0/1. The MIR opcode is the ordinal of the
/// {@link MirOp} enumerator (ReturnVoid=0 ... ArraySimdScope=49); bin_op is the
/// ordinal of {@link BinOp} (Add=0 ... Implies=16). Text fields are escaped:
/// backslash, newline, carriage return, tab and space become `\\`, `\n`, `\r`,
/// `\t` and `\x20` respectively, so a string literal never spans fields.
std::string dump_mir_module(const MirModule& m);

}  // namespace li
