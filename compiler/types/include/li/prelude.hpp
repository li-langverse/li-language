#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

#include <string_view>

namespace li {

/// Names reserved for the language prelude (builtins). User modules cannot
/// define proc/def/type aliases with these unqualified names.
bool is_prelude_type_name(std::string_view name);
bool is_prelude_proc_name(std::string_view name);
bool is_reserved_decorator_name(std::string_view name);
bool is_std_module_symbol(std::string_view name);

/// Reject shadowing prelude/stdlib symbols and duplicate top-level definitions.
void check_stdlib_seal(const Module& module, const std::string& file,
                       DiagnosticBag& diags);

void check_duplicate_definitions(const Module& module, const std::string& file,
                                 DiagnosticBag& diags);

}  // namespace li
