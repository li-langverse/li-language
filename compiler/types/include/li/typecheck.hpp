#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

#include <optional>
#include <string>

namespace li {

struct TypecheckResult {
  bool ok = false;
  DiagnosticBag diagnostics;
};

TypecheckResult typecheck_module(const Module& module);

}  // namespace li
