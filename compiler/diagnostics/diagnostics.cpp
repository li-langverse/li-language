#include "li/diagnostics.hpp"

#include <iostream>

namespace li {

void DiagnosticBag::error(SourceLoc loc, std::string message) {
  items_.push_back(Diagnostic{std::move(loc), std::move(message)});
}

void print_diagnostics(const DiagnosticBag& bag) {
  for (const auto& d : bag.items()) {
    std::cerr << d.loc.file << ':' << d.loc.line << ':' << d.loc.column << ": "
              << "error: " << d.message << '\n';
  }
}

}  // namespace li
