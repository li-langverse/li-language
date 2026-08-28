#pragma once

#include "li/check_config.hpp"
#include "li/diagnostics.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace li {

// Per-entry and directory caps (see docs/security/check-cache-threat-model.md).
inline constexpr std::size_t kCheckCacheMaxEntryBytes = 1024u * 1024u;
inline constexpr std::size_t kCheckCacheDefaultMaxMb = 64u;
inline constexpr std::size_t kCheckCacheMaxDirMb = 4096u;

struct CheckCacheOptions {
  std::filesystem::path cache_dir;
  std::size_t max_mb = kCheckCacheDefaultMaxMb;
  bool enabled = true;
};

struct CheckCacheHit {
  bool hit = false;
  int exit_code = 1;
  std::string output;
};

std::string check_config_hash(const CheckConfig& cfg);
std::uint64_t hash_file_content(const std::filesystem::path& path);
std::uint64_t hash_direct_import_graph(const std::filesystem::path& path);
std::uint64_t hash_bytes_fnv(std::string_view data);
std::string make_check_cache_key(const std::filesystem::path& path, std::uint64_t content_hash,
                                 const std::string& config_hash, const std::string& version,
                                 std::uint64_t import_graph_hash,
                                 std::uint64_t output_mode_hash = 0);

CheckCacheHit try_load_check_cache(const CheckCacheOptions& opts, const std::string& key,
                                   std::uint64_t expected_content_hash);
void store_check_cache(const CheckCacheOptions& opts, const std::string& key, int exit_code,
                       const std::string& output, std::uint64_t content_hash);
void evict_check_cache_lru(const std::filesystem::path& cache_dir, std::size_t max_mb);

bool apply_check_cache_flag(std::string_view arg, CheckCacheOptions& out);
void normalize_check_cache_options(CheckCacheOptions& opts);

const char* check_cache_compiler_version();

}  // namespace li
