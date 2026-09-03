#pragma once

#include <string>

namespace li {

// Walker type-classification contract (mir_type_info in bootstrap/lic/main.li):
// ti[0] rd classes — 0 = int/bool, 1 = float, 2 = str/string, 3 =
// bytes/StringView, 4 = ptr/int64/i64/long, 5 = array, 6 = object, 7 = unit.
// The rd 2/3/4 scalar classes are all pointer-width and drive the same bit at
// several emission sites (FN reti64 via mir_proc ret==3||ret==4, CallProc and
// CallExtern ret_is_i64 via rd 3/4/5, ReturnIdent, and the ABI verifier).
// A single predicate keeps those sites in lockstep; bytes/StringView fell
// through the cracks when each site re-derived the set by hand.
inline bool is_ptr_width_type_name(const std::string& n) {
  return n == "str" || n == "string" || n == "bytes" || n == "StringView" ||
         n == "ptr" || n == "int64" || n == "i64" || n == "long";
}

// Walker mir_param_line ps bit (is_string): ty == 2 or ty == 3, i.e. both
// str/string and bytes/StringView. Note the walker's pi2 (is_i64) bit is
// deliberately narrower — ty == 4 or ty == 2 — so bytes params set is_string
// but not is_i64.
inline bool is_str_bytes_type_name(const std::string& n) {
  return n == "str" || n == "string" || n == "bytes" || n == "StringView";
}

// Scalar rd class 1 (float) — mir_proc/var-decl/param float classification.
inline bool is_float_type_name(const std::string& n) {
  return n == "float" || n == "f64" || n == "float64";
}

// Scalar rd class 2 (str/string) — the ty==2 half of the walker's pi2 is_i64
// rule (object fields/params set is_i64 only for ty 4 or ty 2, not bytes).
inline bool is_string_type_name(const std::string& n) {
  return n == "str" || n == "string";
}

// Scalar rd class 4 (ptr/int64/i64/long) — the ty==4 half of pi2 is_i64.
inline bool is_i64_type_name(const std::string& n) {
  return n == "ptr" || n == "int64" || n == "i64" || n == "long";
}

// Walker mir_param_line/field pi2 (is_i64): ty == 4 or ty == 2 — ptr/i64 AND
// str/string set the bit; bytes/StringView (ty 3) deliberately do not.
inline bool is_pi2_i64_type_name(const std::string& n) {
  return is_i64_type_name(n) || is_string_type_name(n);
}

}  // namespace li