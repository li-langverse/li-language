#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

namespace li {

void borrow_check_module(const Module& module, DiagnosticBag& diags,
                         std::size_t main_proc_count);
void effects_check_module(const Module& module, DiagnosticBag& diags,
                          std::size_t main_proc_count);

}  // namespace li
