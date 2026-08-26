#pragma once

#include "li/resource_options.hpp"

#include <cstddef>
#include <cstdlib>
#include <string>
#include <thread>

namespace li {


inline std::string repo_root_from_env() {
  if (const char* root = std::getenv("LI_REPO_ROOT")) { return root; }
  return {};
}
inline std::string repo_build_prefix() {
  if (const char* dir = std::getenv("LI_BUILD_DIR")) { return dir; }
  const std::string root = repo_root_from_env();
  if (!root.empty()) return root + "/build";
  return "build";
}
inline std::string repo_build_path(const char* relative) {
  std::string prefix = repo_build_prefix();
  if (relative == nullptr || relative[0] == 0) return prefix;
  if (prefix.empty() || prefix.back() != '/') prefix += '/';
  prefix += relative;
  return prefix;
}
/// Host CPU count for tool defaults (LI_BUILD_JOBS / lic --jobs).
inline unsigned default_host_jobs() {
  if (const char* env = std::getenv("LI_BUILD_JOBS")) {
    const int n = std::atoi(env);
    if (n > 0) {
      return static_cast<unsigned>(n);
    }
  }
  const unsigned hc = std::thread::hardware_concurrency();
  return hc > 0 ? hc : 1u;
}

/// Optional compile-time memory budget (MB); 0 = unset.
inline std::size_t max_memory_mb_from_env() {
  if (const std::size_t from_opts = resource_options().max_memory_mb; from_opts > 0) {
    return from_opts;
  }
  if (const char* env = std::getenv("LI_MAX_MEMORY_MB")) {
    const int n = std::atoi(env);
    if (n > 0) {
      return static_cast<std::size_t>(n);
    }
  }
  return 0;
}

/// Parallel frontend job budget reserved for this `lic build` (8p-c).
inline unsigned compile_jobs_from_options() {
  const unsigned reserved = compile_jobs_reserved();
  if (reserved > 0) {
    return reserved;
  }
  if (const char* env = std::getenv("LI_COMPILE_JOBS")) {
    const int n = std::atoi(env);
    if (n > 0) {
      return static_cast<unsigned>(n);
    }
  }
  return default_host_jobs();
}


/// Discard-linked build output (POSIX /dev/null, Windows NUL).
inline constexpr const char* null_output_path() {
#if defined(_WIN32)
  return "NUL";
#else
  return "/dev/null";
#endif
}

inline bool is_null_output_path(const std::string& path) {
#if defined(_WIN32)
  return path == "NUL" || path == "nul";
#else
  return path == "/dev/null";
#endif
}

/// Reject shell metacharacters in paths passed to std::system via clang link.
inline bool is_safe_link_path(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  for (char c : path) {
    if (c == ';' || c == '|' || c == '&' || c == '$' || c == '`' || c == '\n' ||
        c == '\r' || c == '"' || c == '\'') {
      return false;
    }
#if defined(_WIN32)
    if (c == '^' || c == '%' || c == '<' || c == '>') {
      return false;
    }
#endif
  }
  if (path.find("$(") != std::string::npos || path.find("${") != std::string::npos) {
    return false;
  }
  return true;
}

}  // namespace li
