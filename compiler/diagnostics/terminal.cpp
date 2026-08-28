#include "li/terminal.hpp"

#include <cstdlib>
#include <iostream>
#include <unistd.h>

namespace li {
namespace {

bool color_on() {
  if (std::getenv("NO_COLOR") != nullptr) {
    return false;
  }
  if (std::getenv("LI_FORCE_COLOR") != nullptr) {
    return true;
  }
  return isatty(STDERR_FILENO) != 0 || isatty(STDOUT_FILENO) != 0;
}

std::string wrap(std::string_view code, std::string_view text) {
  if (!color_on()) {
    return std::string(text);
  }
  return std::string(code) + std::string(text) + "\033[0m";
}

}  // namespace

bool terminal_color_enabled() { return color_on(); }

std::string styled_error_label() { return wrap("\033[1;38;2;255;92;122m", "error"); }

std::string styled_note_label() { return wrap("\033[1;38;2;255;179;71m", "note"); }

std::string styled_dim(std::string_view text) { return wrap("\033[2;38;2;139;156;179m", text); }

std::string styled_accent(std::string_view text) { return wrap("\033[1;38;2;61;214;255m", text); }

std::string styled_success(std::string_view text) { return wrap("\033[1;38;2;46;230;168m", text); }

std::string styled_warning(std::string_view text) { return wrap("\033[1;38;2;255;179;71m", text); }

std::string reset_style() { return color_on() ? "\033[0m" : ""; }

void print_lic_banner(std::ostream& out) {
  if (!color_on()) {
    return;
  }
  out << styled_dim("    ╭──────────────────────────────────────────╮\n")
      << "    │  " << styled_accent("Li") << styled_dim("  ·  prove · write · run fast") << styled_dim("  │\n")
      << "    ╰──────────────────────────────────────────╯\n"
      << reset_style();
}

void print_verify_telemetry(std::ostream& out, std::string_view key, std::string_view value) {
  out << "  " << styled_dim(key) << " " << styled_accent(value) << '\n';
}

}  // namespace li
