#pragma once

#include <string>

namespace li {

/// Emit and verify LLVM IR for `int main() { return 0; }`.
bool smoke_llvm(std::string* error = nullptr);

}  // namespace li
