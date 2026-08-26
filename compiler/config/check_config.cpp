#include "li/check_config.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace li {
namespace {

CheckRuleLevel parse_level(std::string_view value) {
  if (value == "allow") {
    return CheckRuleLevel::Allow;
  }
  if (value == "deny") {
    return CheckRuleLevel::Deny;
  }
  return CheckRuleLevel::Warn;
}

std::filesystem::path find_workspace_root(const std::filesystem::path& start) {
  auto dir = start.parent_path();
  for (int depth = 0; depth < 12 && !dir.empty() && dir != dir.parent_path(); ++depth) {
    if (std::filesystem::exists(dir / "li.toml")) {
      return dir;
    }
    dir = dir.parent_path();
  }
  return {};
}

}  // namespace

CheckConfig load_check_config(const std::filesystem::path& file_path) {
  CheckConfig config;
  const auto root = find_workspace_root(file_path);
  if (root.empty()) {
    return config;
  }
  std::ifstream in(root / "li.toml");
  if (!in) {
    return config;
  }
  std::string line;
  bool in_check = false;
  while (std::getline(in, line)) {
    if (line.find("[check]") != std::string::npos) {
      in_check = true;
      continue;
    }
    if (in_check && line.find('[') != std::string::npos) {
      break;
    }
    if (!in_check) {
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, eq);
    std::string value = line.substr(eq + 1);
    auto trim = [](std::string& s) {
      while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.erase(s.begin());
      }
      while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
      }
      if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        s = s.substr(1, s.size() - 2);
      }
    };
    trim(key);
    trim(value);
    if (key == "typosquat") {
      config.typosquat = parse_level(value);
    }
  }
  return config;
}

}  // namespace li
