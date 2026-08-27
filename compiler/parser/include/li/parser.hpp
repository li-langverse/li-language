#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"
#include "li/token.hpp"

#include <optional>
#include <string>
#include <vector>

namespace li {

struct ParseResult {
  std::optional<Module> module;
  DiagnosticBag diagnostics;
  bool ok() const { return module.has_value() && diagnostics.empty(); }
};

ParseResult parse_module(std::string source, std::string file);

}  // namespace li
