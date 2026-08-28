#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

#include <string>

namespace li {

void run_advisory_passes(const Module& module, const std::string& file, DiagnosticBag& diags);

}  // namespace li
