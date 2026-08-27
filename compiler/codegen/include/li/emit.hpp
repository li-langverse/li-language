#pragma once

#include "li/mir.hpp"

#include <string>

namespace li {

bool emit_llvm_ir(const MirModule& mir, const std::string& out_path, int runtime_threads,
                  std::string* error);

}  // namespace li
