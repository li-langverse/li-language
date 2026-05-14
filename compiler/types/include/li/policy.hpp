#pragma once

#include "li/diagnostics.hpp"

#include <string>

namespace li {

void check_source_policies(const std::string& source, const std::string& file,
                           DiagnosticBag& diags);

}  // namespace li
