#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace li {

enum class DiagnosticSeverity { Error, Warning, Note };

struct SourceLoc {
  std::string file;
  std::size_t line = 1;
  std::size_t column = 1;
  std::size_t offset = 0;
};

struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  SourceLoc loc;
  std::string code;
  std::string message;
  std::optional<std::string> hint;
};

class DiagnosticBag {
 public:
  void error(SourceLoc loc, std::string message);
  void error(SourceLoc loc, std::string code, std::string message, std::string hint = {});
  void warning(SourceLoc loc, std::string code, std::string message, std::string hint = {});
  void note(SourceLoc loc, std::string code, std::string message, std::string hint = {});

  bool empty() const { return items_.empty(); }
  bool has_errors() const;
  bool has_warnings() const;
  const std::vector<Diagnostic>& items() const { return items_; }

 private:
  void push(DiagnosticSeverity severity, SourceLoc loc, std::string code, std::string message,
            std::string hint);
  std::vector<Diagnostic> items_;
};

void print_diagnostics(const DiagnosticBag& bag);

/// Render diagnostics (banner + items) to a stream instead of stderr. Used by
/// `lic check` to surface advisory warnings/notes on success and cache them.
void render_diagnostics(const DiagnosticBag& bag, std::ostream& out);

void print_diagnostics_json(const DiagnosticBag& bag, std::ostream& out,
                            std::string_view command = "check");

std::string_view severity_string(DiagnosticSeverity severity);

}  // namespace li
