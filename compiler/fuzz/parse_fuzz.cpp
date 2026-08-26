// libFuzzer entry: parse arbitrary bytes as Li source (no codegen).
// Build: cmake -B build-fuzz -DLI_BUILD_FUZZ=ON -DLI_SANITIZE=fuzzer -DLLVM_DIR=...
// Run:   ./build-fuzz/compiler/fuzz/parse_fuzz corpus/ -max_len=65536
#include "li/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr size_t kMax = 1 << 20;
  if (size > kMax) {
    return 0;
  }
  std::string source(reinterpret_cast<const char*>(data), size);
  (void)li::parse_module(std::move(source), "fuzz.li");
  return 0;
}
