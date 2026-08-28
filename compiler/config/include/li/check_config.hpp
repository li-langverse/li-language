#pragma once
#include <filesystem>
namespace li {
enum class CheckRuleLevel { Allow, Warn, Deny };
struct CheckConfig { CheckRuleLevel typosquat = CheckRuleLevel::Warn; };
CheckConfig load_check_config(const std::filesystem::path& file_path);
}
