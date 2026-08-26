#pragma once

#include "li/ast.hpp"
#include "li/check_config.hpp"
#include "li/diagnostics.hpp"

#include <string>

namespace li {

void check_source_policies(const std::string& source, const std::string& file,
                           const CheckConfig& cfg, DiagnosticBag& diags);

/// Structured parallel/disjoint checks from AST (Phase 7d-c).
void check_module_policies(const Module& module, const std::string& file,
                           DiagnosticBag& diags);

}  // namespace li
