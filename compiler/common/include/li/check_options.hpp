#pragma once
#include "li/resource_options.hpp"
#include <cstddef>
#include <string>
namespace li {
struct CheckCacheOptions { bool enabled = true; std::string cache_dir; std::size_t max_entries = 4096; };
enum class CheckDiagFormat { Human, Json };
struct CheckOptions { std::string path; std::string workspace_toml; CheckDiagFormat format = CheckDiagFormat::Human; std::string json_command = "check"; CheckCacheOptions cache; ResourceOptions resources; };
}
