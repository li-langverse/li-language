#pragma once

#include "li/ast.hpp"
#include "li/diagnostics.hpp"

#include <string>

namespace li {

/// Load `import` modules into `out` (append types/procs). Returns false on error.
bool resolve_imports(Module& out, const std::string& file_path, DiagnosticBag& diags);

}  // namespace li
