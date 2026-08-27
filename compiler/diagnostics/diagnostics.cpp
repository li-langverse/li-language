#include "li/diagnostics.hpp"
#include "li/terminal.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

namespace li {

namespace {

std::string json_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 8);
  out.push_back('"');
  for (const char ch : text) {
    switch (ch) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
          out += buf;
        } else {
          out.push_back(ch);
        }
    }
  }
  out.push_back('"');
  return out;
}

std::string infer_diagnostic_code(std::string_view message) {
  const auto has = [&](std::string_view needle) {
    return message.find(needle) != std::string_view::npos;
  };
  if (has("indentation")) {
    return "E0101";
  }
  if (has("out of range") || has("index")) {
    return "E0201";
  }
  if (has("type") && has("expected")) {
    return "E0202";
  }
  if (has("import") || has("module")) {
    return "resolve.import";
  }
  if (has("stdlib_symbol_shadow")) {
    return "E0330";
  }
  if (has("shadow") || has("seal") || has("stdlib")) {
    return "E0330";
  }
  if (has("parallel_requires_disjoint") || has("disjoint slices")) {
    return "E0320";
  }
  if (has("parallel") || has("disjoint")) {
    return "E0320";
  }
  if (has("missing requires")) {
    return "E0301";
  }
  if (has("missing ensures")) {
    return "E0302";
  }
  if (has("ensures true") || has("weak postcondition")) {
    return "E0303";
  }
  if (has("does not satisfy callee") || has("Cannot call `") ||
      has("has a `requires` precondition")) {
    return "E0304";
  }
  if (has("does not satisfy refinement") || has("refinement type `")) {
    return "E0305";
  }
  if (has("contract") || has("requires") || has("ensures")) {
    return "E0301";
  }
  if (has("borrow")) {
    return "E0310";
  }
  if (has("break") && has("loop")) {
    return "E0401";
  }
  if (has("continue") && has("loop")) {
    return "E0402";
  }
  return "lic.error";
}

/** Stable category strings for agents (diagnostic-v1); maps E-codes to semantic ids. */
std::string agent_diagnostic_code(std::string_view code) {
  if (code == "E0201") {
    return "type.index";
  }
  if (code == "E0202") {
    return "type.mismatch";
  }
  if (code == "E0101") {
    return "parse.indent";
  }
  if (code == "E0301") {
    return "contract.requires";
  }
  if (code == "E0302") {
    return "contract.ensures";
  }
  if (code == "E0303") {
    return "contract.weak_ensures";
  }
  if (code == "E0304") {
    return "contract.callee_requires";
  }
  if (code == "E0305") {
    return "type.refinement";
  }
  if (code == "E0320") {
    return "parallel.disjoint";
  }
  if (code == "E0330") {
    return "resolve.shadow";
  }
  if (code == "E0401") {
    return "control.break";
  }
  if (code == "E0402") {
    return "control.continue";
  }
  return std::string(code);
}

std::string effective_code(const Diagnostic& d) {
  if (!d.code.empty()) {
    return d.code;
  }
  return infer_diagnostic_code(d.message);
}

}  // namespace

void DiagnosticBag::error(SourceLoc loc, std::string message) {
  push(DiagnosticSeverity::Error, std::move(loc), {}, std::move(message), {});
}

void DiagnosticBag::error(SourceLoc loc, std::string code, std::string message,
                          std::string hint) {
  push(DiagnosticSeverity::Error, std::move(loc), std::move(code), std::move(message),
       std::move(hint));
}

void DiagnosticBag::warning(SourceLoc loc, std::string code, std::string message,
                            std::string hint) {
  push(DiagnosticSeverity::Warning, std::move(loc), std::move(code), std::move(message),
       std::move(hint));
}

void DiagnosticBag::note(SourceLoc loc, std::string code, std::string message, std::string hint) {
  push(DiagnosticSeverity::Note, std::move(loc), std::move(code), std::move(message),
       std::move(hint));
}

void DiagnosticBag::push(DiagnosticSeverity severity, SourceLoc loc, std::string code,
                         std::string message, std::string hint) {
  std::optional<std::string> hint_opt;
  if (!hint.empty()) {
    hint_opt = std::move(hint);
  }
  items_.push_back(
      Diagnostic{severity, std::move(loc), std::move(code), std::move(message), std::move(hint_opt)});
}

bool DiagnosticBag::has_errors() const {
  return std::any_of(items_.begin(), items_.end(), [](const Diagnostic& d) {
    return d.severity == DiagnosticSeverity::Error;
  });
}

bool DiagnosticBag::has_warnings() const {
  return std::any_of(items_.begin(), items_.end(), [](const Diagnostic& d) {
    return d.severity == DiagnosticSeverity::Warning;
  });
}

std::string_view severity_string(DiagnosticSeverity severity) {
  switch (severity) {
    case DiagnosticSeverity::Error:
      return "error";
    case DiagnosticSeverity::Warning:
      return "warning";
    case DiagnosticSeverity::Note:
      return "note";
  }
  return "error";
}

void print_diagnostics(const DiagnosticBag& bag) {
  if (bag.empty()) {
    return;
  }
  print_lic_banner(std::cerr);
  for (const auto& d : bag.items()) {
    const std::string code = effective_code(d);
    std::string label;
    if (d.severity == DiagnosticSeverity::Error) {
      label = styled_error_label();
    } else if (d.severity == DiagnosticSeverity::Warning) {
      label = styled_warning("warning");
    } else {
      label = styled_note_label();
    }
    std::cerr << styled_dim(d.loc.file) << ':' << styled_accent(std::to_string(d.loc.line))
              << ':' << styled_accent(std::to_string(d.loc.column)) << ": " << label << " ["
              << styled_accent(code) << "]: " << d.message
              << reset_style() << '\n';
    if (d.hint && !d.hint->empty()) {
      std::cerr << "  " << styled_dim("hint:") << ' ' << *d.hint << reset_style() << '\n';
    }
  }
}

void render_diagnostics(const DiagnosticBag& bag, std::ostream& out) {
  if (bag.empty()) {
    return;
  }
  print_lic_banner(out);
  for (const auto& d : bag.items()) {
    const std::string code = effective_code(d);
    std::string label;
    if (d.severity == DiagnosticSeverity::Error) {
      label = styled_error_label();
    } else if (d.severity == DiagnosticSeverity::Warning) {
      label = styled_warning("warning");
    } else {
      label = styled_note_label();
    }
    out << styled_dim(d.loc.file) << ':' << styled_accent(std::to_string(d.loc.line))
        << ':' << styled_accent(std::to_string(d.loc.column)) << ": " << label << " ["
        << styled_accent(code) << "]: " << d.message
        << reset_style() << '\n';
    if (d.hint && !d.hint->empty()) {
      out << "  " << styled_dim("hint:") << ' ' << *d.hint << reset_style() << '\n';
    }
  }
}

void print_diagnostics_json(const DiagnosticBag& bag, std::ostream& out,
                            std::string_view command) {
  out << "{\"version\":1,\"schema\":\"diagnostic-v1\",\"tool\":\"lic\",\"command\":"
      << json_escape(command) << ",\"ok\":" << (bag.has_errors() ? "false" : "true")
      << ",\"diagnostics\":[";
  bool first = true;
  for (const auto& d : bag.items()) {
    if (!first) {
      out << ',';
    }
    first = false;
    const std::string code = agent_diagnostic_code(effective_code(d));
    out << "{\"severity\":" << json_escape(severity_string(d.severity)) << ",\"file\":"
        << json_escape(d.loc.file) << ",\"line\":" << d.loc.line << ",\"column\":" << d.loc.column
        << ",\"offset\":" << d.loc.offset << ",\"code\":" << json_escape(code)
        << ",\"message\":" << json_escape(d.message);
    if (d.hint && !d.hint->empty()) {
      out << ",\"fix_hint\":" << json_escape(*d.hint);
    } else {
      out << ",\"fix_hint\":null";
    }
    out << '}';
  }
  out << "]}\n";
}

}  // namespace li
