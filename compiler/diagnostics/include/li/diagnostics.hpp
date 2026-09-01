#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace li {

struct SourceLoc {
  std::string file;
  std::size_t line = 1;
  std::size_t column = 1;
  std::size_t offset = 0;
};

struct Diagnostic {
  SourceLoc loc;
  std::string message;
};

class DiagnosticBag {
 public:
  void error(SourceLoc loc, std::string message);
  bool empty() const { return items_.empty(); }
  const std::vector<Diagnostic>& items() const { return items_; }

 private:
  std::vector<Diagnostic> items_;
};

void print_diagnostics(const DiagnosticBag& bag);

}  // namespace li
