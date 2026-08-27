#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

namespace li {

void borrow_check_module(const Module& module, DiagnosticBag& diags);
void effects_check_module(const Module& module, DiagnosticBag& diags);

}  // namespace li
