#pragma once

#include "li/ast.hpp"
#include "li/compile_options.hpp"

#include <string>

namespace li {

bool compile_module(const Module& module, const std::string& output_path,
                  const CompileOptions& opts, const std::string& extra_clang_flags,
                  std::string* error);

}  // namespace li
