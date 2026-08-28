#pragma once

#include "li/diagnostics.hpp"

#include <string>
#include <string_view>

namespace li {

enum class ErrorCode {
  E0101,
  E0201,
  E0202,
  E0301,
  E0302,
  E0303,
  E0304,
  E0305,
  E0310,
  E0311,
  E0320,
  E0321,
  E0322,
  E0330,
  E0340,
  E0350,
  E0360,
  E0401,
  E0402,
  E0501,
  E0502,
  E0503,
  E0504,
  E0505,
};

enum class WarningCode {
  W0401,
  W0402,
  W0403,
};

enum class NoteCode {
  N0401,
};

std::string_view error_code_string(ErrorCode code);
std::string_view warning_code_string(WarningCode code);
std::string_view note_code_string(NoteCode code);

struct FormattedDiagnostic {
  std::string code;
  std::string message;
  std::string hint;
};

FormattedDiagnostic format_diagnostic(ErrorCode code, std::string_view message,
                                    std::string_view hint = {});

void diag_error(DiagnosticBag& bag, SourceLoc loc, ErrorCode code,
                std::string_view message, std::string_view hint = {});
void diag_warning(DiagnosticBag& bag, SourceLoc loc, WarningCode code,
                  std::string_view message, std::string_view hint = {});
void diag_note(DiagnosticBag& bag, SourceLoc loc, NoteCode code, std::string_view message,
               std::string_view hint = {});

}  // namespace li
