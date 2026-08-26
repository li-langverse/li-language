#pragma once

#include <optional>
#include <string_view>

namespace li {

enum class NumericScalarKind { IntSigned, IntUnsigned, Float };

struct NumericScalarDesc {
  NumericScalarKind kind = NumericScalarKind::Float;
  int bits = 0;
  std::string_view canonical = {};
  bool is_quantized_logical = false;  // float4/float8 — layout TBD for kernels
};

/// Resolve a surface type name to a fixed-width scalar (int/float widths).
/// Returns nullopt for non-scalar names (e.g. `Vec3`, `bool`).
std::optional<NumericScalarDesc> lookup_numeric_scalar(std::string_view name);

bool is_numeric_scalar_type_name(std::string_view name);

/// Literal suffix (`f32`, `i32`, `u`, …) after a numeric token.
std::optional<NumericScalarDesc> lookup_literal_suffix(std::string_view suffix, bool from_float);

}  // namespace li
