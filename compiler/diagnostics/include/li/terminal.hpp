#pragma once

#include <string>
#include <string_view>

namespace li {

bool terminal_color_enabled();

std::string styled_error_label();
std::string styled_note_label();
std::string styled_dim(std::string_view text);
std::string styled_accent(std::string_view text);
std::string styled_success(std::string_view text);
std::string styled_warning(std::string_view text);
std::string reset_style();

void print_lic_banner(std::ostream& out);
void print_verify_telemetry(std::ostream& out, std::string_view key, std::string_view value);

}  // namespace li
