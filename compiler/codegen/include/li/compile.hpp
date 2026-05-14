#pragma once

#include "li/ast.hpp"

#include <string>

namespace li {

bool compile_module(const Module& module, const std::string& output_path, bool release,
                  const std::string& extra_clang_flags, std::string* error);

}  // namespace li
