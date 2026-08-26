#include "li/numeric_types.hpp"

#include <string_view>
#include <vector>

namespace li {
namespace {

struct Row {
  std::string_view alias;
  NumericScalarDesc desc;
};

const std::vector<Row>& table() {
  static const std::vector<Row> k = [] {
    std::vector<Row> rows;
    auto add_int = [&](std::string_view alias, int bits) {
      rows.push_back({alias, {NumericScalarKind::IntSigned, bits, alias, false}});
    };
    auto add_uint = [&](std::string_view alias, int bits) {
      rows.push_back({alias, {NumericScalarKind::IntUnsigned, bits, alias, false}});
    };
    auto add_float = [&](std::string_view alias, int bits, std::string_view canon,
                         bool quantized = false) {
      rows.push_back({alias, {NumericScalarKind::Float, bits, canon, quantized}});
    };

    // Signed integers (4 … 512)
    add_int("int4", 4);
    add_int("Int4", 4);
    add_int("i4", 4);
    add_int("int8", 8);
    add_int("Int8", 8);
    add_int("i8", 8);
    add_int("int16", 16);
    add_int("Int16", 16);
    add_int("i16", 16);
    add_int("int32", 32);
    add_int("Int32", 32);
    add_int("i32", 32);
    add_int("int64", 64);
    add_int("Int64", 64);
    add_int("i64", 64);
    add_int("int", 64);
    add_int("long", 64);
    add_int("int128", 128);
    add_int("Int128", 128);
    add_int("i128", 128);
    add_int("int256", 256);
    add_int("Int256", 256);
    add_int("i256", 256);
    add_int("int512", 512);
    add_int("Int512", 512);
    add_int("i512", 512);

    // Unsigned
    add_uint("uint4", 4);
    add_uint("UInt4", 4);
    add_uint("u4", 4);
    add_uint("uint8", 8);
    add_uint("UInt8", 8);
    add_uint("u8", 8);
    add_uint("uint16", 16);
    add_uint("UInt16", 16);
    add_uint("u16", 16);
    add_uint("uint32", 32);
    add_uint("UInt32", 32);
    add_uint("u32", 32);
    add_uint("uint64", 64);
    add_uint("UInt64", 64);
    add_uint("u64", 64);
    add_uint("uint", 64);
    add_uint("usize", 64);
    add_uint("uint128", 128);
    add_uint("UInt128", 128);
    add_uint("u128", 128);
    add_uint("uint256", 256);
    add_uint("UInt256", 256);
    add_uint("u256", 256);
    add_uint("uint512", 512);
    add_uint("UInt512", 512);
    add_uint("u512", 512);

    // Floats — IEEE + logical quantization widths
    add_float("float4", 4, "float4", true);
    add_float("Float4", 4, "float4", true);
    add_float("f4", 4, "float4", true);
    add_float("float8", 8, "float8", true);
    add_float("Float8", 8, "float8", true);
    add_float("f8", 8, "float8", true);
    add_float("float16", 16, "float16");
    add_float("Float16", 16, "float16");
    add_float("f16", 16, "float16");
    add_float("float32", 32, "float32");
    add_float("Float32", 32, "float32");
    add_float("f32", 32, "float32");
    add_float("float64", 64, "float64");
    add_float("Float64", 64, "float64");
    add_float("f64", 64, "float64");
    add_float("float", 64, "float64");
    add_float("float128", 128, "float128");
    add_float("Float128", 128, "float128");
    add_float("f128", 128, "float128");
    add_float("float256", 256, "float256");
    add_float("Float256", 256, "float256");
    add_float("f256", 256, "float256");
    add_float("float512", 512, "float512");
    add_float("Float512", 512, "float512");
    add_float("f512", 512, "float512");

    return rows;
  }();
  return k;
}

}  // namespace

std::optional<NumericScalarDesc> lookup_numeric_scalar(const std::string_view name) {
  for (const auto& row : table()) {
    if (row.alias == name) {
      return row.desc;
    }
  }
  return std::nullopt;
}

bool is_numeric_scalar_type_name(const std::string_view name) {
  return lookup_numeric_scalar(name).has_value();
}

std::optional<NumericScalarDesc> lookup_literal_suffix(const std::string_view suffix,
                                                       const bool from_float) {
  if (suffix.empty()) {
    return std::nullopt;
  }
  if (!from_float && suffix == "u") {
    return NumericScalarDesc{NumericScalarKind::IntUnsigned, 64, "uint", false};
  }
  const auto desc = lookup_numeric_scalar(suffix);
  if (!desc) {
    return std::nullopt;
  }
  if (from_float) {
    if (desc->kind != NumericScalarKind::Float) {
      return std::nullopt;
    }
    return desc;
  }
  if (desc->kind == NumericScalarKind::Float) {
    return std::nullopt;
  }
  return desc;
}

}  // namespace li
