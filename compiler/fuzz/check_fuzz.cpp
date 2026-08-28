// libFuzzer: parse + policy + typecheck (no codegen).
// Build: cmake -B build-fuzz -DLI_BUILD_FUZZ=ON ...
// Run:   ./build-fuzz/compiler/fuzz/check_fuzz corpus/ -max_len=65536
#include "li/parser.hpp"
#include "li/check_config.hpp"
#include "li/policy.hpp"
#include "li/typecheck.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMax = 1 << 20;
  if (size > kMax) {
    return 0;
  }
  std::string source(reinterpret_cast<const char*>(data), size);
  li::DiagnosticBag policy_diags;
  li::check_source_policies(source, "fuzz.li", li::CheckConfig{}, policy_diags);
  if (!policy_diags.empty()) {
    return 0;
  }
  auto parsed = li::parse_module(std::move(source), "fuzz.li");
  if (!parsed.module) {
    return 0;
  }
  (void)li::typecheck_module(*parsed.module);
  return 0;
}
