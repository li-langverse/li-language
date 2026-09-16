#!/usr/bin/env bash
# Layer 4 self-host parity: the Li `check` subcommand (built from
# bootstrap/lic/main.li) must produce the same accept/reject verdicts
# as the C++ `lic check` on the typecheck corpus.
#
# `lic check` on both sides runs parse + source-policy/effects/borrow/
# encapsulation checks over the current li-tests/typecheck layout (the old
# let_bindings/closures/records files were removed in the compiler squash).
#
# Usage:
#   scripts/check_li_check_parity.sh
#   LI_CHECK_BIN=/custom/lic scripts/check_li_check_parity.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LIC="${LIC:-$("$ROOT/scripts/resolve-lic.sh")}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
LI="${LI_CHECK_BIN:-$TMP/lic-from-li}"

# Build the Li check binary from bootstrap/lic/main.li with the C++ host
# (the self-host milestone gate), unless an external binary is supplied.
if [[ -z "${LI_CHECK_BIN:-}" ]]; then
  "$LIC" build "$ROOT/bootstrap/lic/main.li" -o "$LI" --allow-open-vc \
    --no-lean-verify >/dev/null 2>&1 \
    || { echo "check_li_check_parity: could not build bootstrap/lic/main.li" >&2; exit 1; }
fi

# Corpus: files both sides must accept (exercises the parse + source/effects/
# borrow/encapsulation layers of the Li `check` subcommand).
CORPUS_OK=(
  "li-tests/typecheck/fib.li"
  "li-tests/typecheck/probe_ifdecl.li"
  "li-tests/typecheck/probe_move.li"
  "li-tests/typecheck/probe_parse.li"
  "li-tests/typecheck/probe_ptr.li"
  "li-tests/typecheck/probe_ref.li"
  "examples/hello.li"
  "examples/arrays.li"
  "li-tests/effects/io_ok.li"
  "li-tests/collections/typedict_ok.li"
  "li-tests/collections/enum_ok.li"
  "li-tests/collections/tuple_pair.li"
  "li-tests/lexer_parser/parser_accept_elif.li"
  "li-tests/typecheck/scalar_width_ok.li"
  "li-tests/typecheck/literal_suffix_ok.li"
  "li-tests/typecheck/binary_literal_ok.li"
  "li-tests/generics/precision_real_alias.li"
  "li-tests/encapsulation/def_method_parse.li"
  "li-tests/encapsulation/def_method_call.li"
  "li-tests/encapsulation/object_method_mutate.li"
  "li-tests/encapsulation/inheritance_subtype.li"
  "li-tests/encapsulation/private_method_lib.li"
  "li-tests/contracts_verify/method_call_requires_ok.li"
  "li-tests/contracts_verify/method_call_requires_fail.li"
  # Loop coverage: `for` bodies are parsed and traversed, and loop bodies stay
  # opaque to the move checker (see loop_move_ok.li).
  "li-tests/math_syntax/for_range_sum.li"
  "li-tests/decorators/vectorized_for_parse_ok.li"
  "li-tests/decorators/vectorized_for_scope_ok.li"
  "li-tests/borrow/loop_move_ok.li"
  "li-tests/contracts_verify/method_ensures_return_ok.li"
  "li-tests/advisory/check_deny_warn.li"
  "li-tests/advisory/check_ok.li"
  "li-tests/advisory/deny/check_fail.li"
  "li-tests/codegen/neg_float_procs.li"
  "li-tests/contracts_verify/bounds_refinement_release_ok.li"
  "li-tests/contracts_verify/caller_requires_local_ok.li"
  "li-tests/contracts_verify/caller_requires_ok.li"
  "li-tests/contracts_verify/discharge_const.li"
  "li-tests/contracts_verify/discharge_float_mirror.li"
  "li-tests/contracts_verify/discharge_identity.li"
  "li-tests/contracts_verify/discharge_trivial.li"
  "li-tests/contracts_verify/extern_call_requires_ok.li"
  "li-tests/contracts_verify/http_parse_forward_closed.li"
  "li-tests/contracts_verify/linalg_axpy4_int_closed.li"
  "li-tests/contracts_verify/linalg_dot4_float_closed.li"
  "li-tests/contracts_verify/linalg_dot4_int_closed.li"
  "li-tests/contracts_verify/linalg_dot4_int_loop_open.li"
  "li-tests/contracts_verify/linalg_mat2_at2_float_closed.li"
  "li-tests/contracts_verify/linalg_mat2_callproc_float_closed.li"
  "li-tests/contracts_verify/linalg_mat2_entry00_int_closed.li"
  "li-tests/contracts_verify/linalg_mat2_float_value.li"
  "li-tests/contracts_verify/linalg_mat2_float_wrong_value.li"
  "li-tests/contracts_verify/linalg_norm4_int_closed.li"
  "li-tests/contracts_verify/linalg_sum4_int_closed.li"
  "li-tests/contracts_verify/refinement_call_fail.li"
  "li-tests/contracts_verify/refinement_call_ok.li"
  "li-tests/contracts_verify/refinement_guard_ok.li"
  "li-tests/contracts_verify/refinement_init_fail.li"
  "li-tests/contracts_verify/refinement_init_ok.li"
  "li-tests/contracts_verify/refinement_inline_ok.li"
  "li-tests/contracts_verify/refinement_local_ok.li"
  "li-tests/contracts_verify/sqrt_open_bound.li"
  "li-tests/effects/async_ok.li"
  "li-tests/effects/net_forward_ok.li"
  "li-tests/effects/net_ok.li"
  "li-tests/encapsulation/def_multiline_params.li"
  "li-tests/encapsulation/def_syntax_ok.li"
  "li-tests/encapsulation/import_fixture.li"
  "li-tests/encapsulation/import_parse.li"
  "li-tests/encapsulation/object_public_field.li"
  "li-tests/encapsulation/trait_hash_impl.li"
  "li-tests/encapsulation/vault_lib.li"
  "li-tests/generics/precision_generic_fn.li"
  "li-tests/httpd/gate_placeholder.li"
  "li-tests/httpd/reserved_wellknown_note.li"
  "li-tests/index_helpers/cell_index_refined.li"
  "li-tests/lexer_parser/plus_minus_binop.li"
  "li-tests/math_linalg/array_dot_matmul.li"
  "li-tests/math_linalg/array_dot_mismatch.li"
  "li-tests/math_linalg/broadcast_invalid_len2_vs_len4.li"
  "li-tests/math_linalg/broadcast_len1_add_float4.li"
  "li-tests/math_linalg/broadcast_len1_mul_int4.li"
  "li-tests/math_linalg/broadcast_len1_pow_int4.li"
  "li-tests/math_linalg/elementwise_mul_float.li"
  "li-tests/math_linalg/elementwise_mul_float10.li"
  "li-tests/math_linalg/elementwise_pow_float4.li"
  "li-tests/math_linalg/matmul_2x3_ok.li"
  "li-tests/math_linalg/matmul_chain_ok.li"
  "li-tests/math_linalg/matmul_dim_mismatch.li"
  "li-tests/math_linalg/scale_float4.li"
  "li-tests/math_linalg/vec3_ops.li"
  "li-tests/math_syntax/int_floordiv.li"
  "li-tests/math_syntax/int_mod.li"
  "li-tests/math_syntax/int_pow.li"
  "li-tests/modules/greeter/greeter.li"
  "li-tests/modules/positive/positive.li"
  "li-tests/parallel_codegen/md_mini.li"
  "li-tests/parallel_codegen/three_float_arrays_12.li"
  "li-tests/parallel_codegen/three_float_arrays_32.li"
  "li-tests/parallel_codegen/three_float_arrays_8.li"
  "li-tests/parallel_codegen/two_arrays_one_loop.li"
  "li-tests/parallel_codegen/two_float_arrays.li"
  "li-tests/parallel_codegen/two_float_arrays_32.li"
  "li-tests/physics/golden_positions_sum.li"
  "li-tests/physics/profile_defaults.li"
  "li-tests/physics/three_body_mini.li"
  "li-tests/proof_gaps/false_ensures_still_builds.li"
  "li-tests/runtime/argv_ptr_abi.li"
  "li-tests/runtime/unary_minus.li"
  "li-tests/compile_ok/int_ne_literal.li"
  "li-tests/objects/nested_field_write.li"
  "li-tests/objects/nested_index_write.li"
  "li-tests/objects/nested_index_read.li"
  # Stdlib seal: a module may not shadow a prelude/std name, and `ok_control`
  # is the control proving the seal does not over-reject (see the
  # compiler/types/prelude.cpp name sets).
  "li-tests/stdlib_seal/ok_control.li"
)

# Corpus: files both sides must reject. Entries are `file:EXXXX` when a
# specific error code must appear in both diagnostics, or `file:-` when both
# sides only need to reject (no shared code surface yet).
CORPUS_FAIL=(
  "li-tests/typecheck/bad_array_index.li:E0303"
  "li-tests/typecheck/bad_int_float_add.li:E0303"
  "li-tests/typecheck/bad_numeric_mix.li:E0303"
  "li-tests/typecheck/probe_alias.li:E0303"
  "li-tests/typecheck/probe_arrret.li:E0303"
  "li-tests/typecheck/scalar_width_mix_fail.li:-"
  "li-tests/encapsulation/def_method_missing.li:-"
  "li-tests/encapsulation/private_method_use.li:-"
  "li-tests/effects/async_keyword_missing_raises.li:E0303"
  "li-tests/effects/await_outside_async.li:E0303"
  "li-tests/effects/bytes_need_alloc.li:E0303"
  "li-tests/encapsulation/override_mismatch.li:E0303"
  "li-tests/lexer_parser/async_await_parse.li:E0303"
  "li-tests/lexer_parser/python_none_option.li:E0303"
  "li-tests/prove_reject/weak_ensures_true.li:E0303"
  "li-tests/advisory/check_mixed.li:-"
  "li-tests/advisory/unreachable_after_return.li:-"
  "li-tests/bytes/reader_writer_smoke.li:-"
  "li-tests/compile_ok/volatile_sink_no_io.li:-"
  "li-tests/contracts_verify/caller_requires_fail.li:E0304"
  "li-tests/contracts_verify/loop_requires_while_fail.li:E0304"
  "li-tests/contracts_verify/loop_requires_for_fail.li:E0304"
  "li-tests/math_linalg/elementwise_len_mismatch.li:-"
  "li-tests/modules/import_cycle_b.li:-"
  "li-tests/encapsulation/extern_proc_syntax_rejected.li:-"
  "li-tests/encapsulation/proc_syntax_rejected.li:-"
  "li-tests/lexer_parser/decorators_parse.li:-"
  "li-tests/lexer_parser/parser_reject_bad_expr.li:-"
  "li-tests/lexer_parser/parser_reject_bad_paren.li:-"
  "li-tests/lexer_parser/parser_reject_no_body.li:-"
  "li-tests/lexer_parser/parser_reject_proc_kw.li:-"
  "li-tests/lexer_parser/parser_reject_unclosed_call.li:-"
  "li-tests/math_linalg/golden_dot4_ones_twos.li:-"
  "li-tests/math_linalg/golden_tier1_dot8.li:-"
  "li-tests/math_linalg/reductions/sum_non_array.li:-"
  "li-tests/modules/import_cycle_a.li:-"
  "li-tests/physics/game_runtime_smoke.li:-"
  "li-tests/race_shared_memory/false_disjoint_requires_true.li:-"
  "li-tests/stdlib_coverage/build_std_csv.li:-"
  # Stdlib seal: `duplicate_definition` and `stdlib_symbol_shadow` name no
  # shared code string on both sides yet (the C++ reference prints the prose
  # form `(stdlib_symbol_shadow: NAME)`, the walker the numeric `E0330`), so
  # these assert the reject verdict; li-tests/manifest.toml asserts the text.
  "li-tests/stdlib_seal/duplicate_proc.li:-"
  "li-tests/stdlib_seal/shadow_list_type.li:-"
  "li-tests/stdlib_seal/shadow_print.li:-"
  "li-tests/stdlib_seal/shadow_print_dep.li:-"
  "li-tests/stdlib_seal/shadow_std_symbol.li:-"
)

checked=0
pass=0
fail=0

for f in "${CORPUS_OK[@]}"; do
  fp="$ROOT/$f"
  if [[ ! -f "$fp" ]]; then
    echo "  SKIP  $f (not found)"
    continue
  fi
  cpp_rc=0; li_rc=0
  "$LIC" check "$fp" >/dev/null 2>&1 || cpp_rc=$?
  "$LI" check "$fp" >/dev/null 2>&1 || li_rc=$?
  checked=$((checked + 1))
  if [[ "$cpp_rc" == "0" && "$li_rc" == "0" ]]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "  FAIL  $f  C++=$cpp_rc Li=$li_rc (expected both 0)"
  fi
done

for entry in "${CORPUS_FAIL[@]}"; do
  f="${entry%%:*}"
  expected_code="${entry##*:}"
  fp="$ROOT/$f"
  if [[ ! -f "$fp" ]]; then
    echo "  SKIP  $f (not found)"
    continue
  fi
  cpp_out=$( "$LIC" check "$fp" 2>&1 ) && cpp_rc=0 || cpp_rc=$?
  li_out=$( "$LI" check "$fp" 2>&1 ) && li_rc=0 || li_rc=$?
  checked=$((checked + 1))
  if [[ "$expected_code" == "-" ]]; then
    if [[ "$cpp_rc" != "0" && "$li_rc" != "0" ]]; then
      pass=$((pass + 1))
    else
      fail=$((fail + 1))
      echo "  FAIL  $f  expected reject cpp=$cpp_rc li=$li_rc"
    fi
    continue
  fi
  cpp_has=$(echo "$cpp_out" | grep -c "$expected_code" || true)
  li_has=$(echo "$li_out" | grep -c "$expected_code" || true)
  if [[ "$cpp_has" != "0" && "$li_has" != "0" && "$cpp_rc" != "0" && "$li_rc" != "0" ]]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1))
    echo "  FAIL  $f  expected $expected_code cpp=$cpp_rc/$cpp_has li=$li_rc/$li_has"
  fi
done

echo ""
echo "check_li_check_parity: $pass/$checked passed, $fail failed"
if [[ "$fail" -gt 0 ]]; then
  exit 1
fi
