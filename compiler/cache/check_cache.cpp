#include "li/check_cache.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace li {
namespace {

constexpr const char* kCacheFormatVersion = "1";

std::uint64_t fnv1a64_update(std::uint64_t hash, const unsigned char* data, std::size_t len) {
  constexpr std::uint64_t prime = 1099511628211ull;
  constexpr std::uint64_t offset = 14695981039346656037ull;
  for (std::size_t i = 0; i < len; ++i) {
    hash ^= static_cast<std::uint64_t>(data[i]);
    hash *= prime;
  }
  return hash == 0 ? offset : hash;
}

std::string hex64(std::uint64_t value) {
  static const char* kHex = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 15; i >= 0; --i) {
    out[static_cast<std::size_t>(i)] = kHex[value & 0xf];
    value >>= 4;
  }
  return out;
}

bool is_safe_cache_key(const std::string& key) {
  if (key.empty() || key.size() > 160) {
    return false;
  }
  for (const char c : key) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || c == '-')) {
      return false;
    }
  }
  return true;
}

std::filesystem::path weakly_canonical_path(const std::filesystem::path& path) {
  std::error_code ec;
  std::filesystem::path canon = std::filesystem::weakly_canonical(path, ec);
  if (!ec && !canon.empty()) {
    return canon;
  }
  canon = std::filesystem::absolute(path, ec);
  return ec ? path : canon;
}

bool path_within_root(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  const auto root_canon = weakly_canonical_path(root);
  const auto cand_canon = weakly_canonical_path(candidate);
  if (root_canon.empty() || cand_canon.empty()) {
    return false;
  }
  auto root_str = root_canon.lexically_normal().string();
  auto cand_str = cand_canon.lexically_normal().string();
  if (root_str.empty() || cand_str.empty()) {
    return false;
  }
#if defined(_WIN32)
  auto norm = [](std::string s) {
    for (char& c : s) {
      if (c == '\\') {
        c = '/';
      }
    }
    while (!s.empty() && s.back() == '/') {
      s.pop_back();
    }
    return s;
  };
  root_str = norm(root_str);
  cand_str = norm(cand_str);
#endif
  if (cand_str == root_str) {
    return true;
  }
  const char sep = '/';
  if (root_str.back() != sep) {
    root_str.push_back(sep);
  }
  return cand_str.rfind(root_str, 0) == 0;
}

bool is_symlink_path(const std::filesystem::path& path) {
  std::error_code ec;
  const auto st = std::filesystem::symlink_status(path, ec);
  return !ec && std::filesystem::is_symlink(st);
}

std::optional<std::filesystem::path> resolve_cache_entry_path(const std::filesystem::path& cache_dir,
                                                              const std::string& key) {
  if (!is_safe_cache_key(key)) {
    return std::nullopt;
  }
  const auto path = cache_dir / (key + ".json");
  if (!path_within_root(cache_dir, path)) {
    return std::nullopt;
  }
  return path;
}

std::size_t dir_size_bytes(const std::filesystem::path& dir) {
  std::size_t total = 0;
  if (!std::filesystem::exists(dir)) {
    return 0;
  }
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file()) {
      total += static_cast<std::size_t>(entry.file_size());
    }
  }
  return total;
}

bool read_cache_file(const std::filesystem::path& path, std::uint64_t expected_content_hash,
                     CheckCacheHit& hit) {
  std::error_code ec;
  if (is_symlink_path(path)) {
    return false;
  }
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || size > kCheckCacheMaxEntryBytes) {
    std::filesystem::remove(path, ec);
    return false;
  }

  std::ifstream in(path);
  if (!in) {
    return false;
  }
  std::string line;
  if (!std::getline(in, line) || line != std::string("v=") + kCacheFormatVersion) {
    return false;
  }
  if (!std::getline(in, line) || line.rfind("exit=", 0) != 0) {
    return false;
  }
  hit.exit_code = std::atoi(line.c_str() + 5);
  if (!std::getline(in, line) || line.rfind("content=", 0) != 0) {
    return false;
  }
  const std::string content_hex = line.substr(8);
  if (content_hex.size() != 16) {
    return false;
  }
  std::uint64_t stored_content = 0;
  for (char c : content_hex) {
    stored_content <<= 4;
    if (c >= '0' && c <= '9') {
      stored_content |= static_cast<std::uint64_t>(c - '0');
    } else if (c >= 'a' && c <= 'f') {
      stored_content |= static_cast<std::uint64_t>(10 + c - 'a');
    } else {
      return false;
    }
  }
  if (stored_content != expected_content_hash) {
    return false;
  }
  if (!std::getline(in, line) || line != "---") {
    return false;
  }
  std::ostringstream body;
  body << in.rdbuf();
  hit.output = body.str();
  if (hit.output.size() > kCheckCacheMaxEntryBytes) {
    hit.output.clear();
    return false;
  }
  hit.hit = true;
  return true;
}

}  // namespace

const char* check_cache_compiler_version() {
#ifdef LI_VERSION
  return LI_VERSION;
#else
  return "dev";
#endif
}

std::string check_config_hash(const CheckConfig& cfg) {
  std::ostringstream ss;
  ss << "typosquat=";
  switch (cfg.typosquat) {
    case CheckRuleLevel::Allow:
      ss << "allow";
      break;
    case CheckRuleLevel::Deny:
      ss << "deny";
      break;
    case CheckRuleLevel::Warn:
    default:
      ss << "warn";
      break;
  }
  return ss.str();
}

std::uint64_t hash_file_content(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return 0;
  }
  std::uint64_t hash = 14695981039346656037ull;
  std::vector<unsigned char> buf(65536);
  while (in) {
    in.read(reinterpret_cast<char*>(buf.data()),
            static_cast<std::streamsize>(buf.size()));
    const auto got = static_cast<std::size_t>(in.gcount());
    if (got == 0) {
      break;
    }
    hash = fnv1a64_update(hash, buf.data(), got);
  }
  return hash;
}

std::uint64_t hash_direct_import_graph(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in) {
    return 0;
  }
  std::uint64_t hash = 14695981039346656037ull;
  std::string line;
  while (std::getline(in, line)) {
    std::size_t start = 0;
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
      ++start;
    }
    if (line.compare(start, 7, "import ") != 0) {
      continue;
    }
    start += 7;
    while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start]))) {
      ++start;
    }
    const std::size_t end = line.find_first_of(" ;\t\r\n", start);
    const std::string_view spec(line.data() + start,
                                (end == std::string::npos ? line.size() : end) - start);
    if (!spec.empty()) {
      hash = fnv1a64_update(hash, reinterpret_cast<const unsigned char*>(spec.data()), spec.size());
    }
  }
  return hash;
}

std::uint64_t hash_bytes_fnv(std::string_view data) {
  return fnv1a64_update(0, reinterpret_cast<const unsigned char*>(data.data()), data.size());
}

std::string make_check_cache_key(const std::filesystem::path& path, std::uint64_t content_hash,
                                 const std::string& config_hash, const std::string& version,
                                 std::uint64_t import_graph_hash, std::uint64_t output_mode_hash) {
  std::ostringstream combined;
  combined << path.string() << '|' << content_hash << '|' << config_hash << '|' << version << '|'
           << import_graph_hash << '|' << output_mode_hash;
  return hex64(hash_bytes_fnv(combined.str()));
}

void normalize_check_cache_options(CheckCacheOptions& opts) {
  if (opts.max_mb == 0) {
    opts.max_mb = kCheckCacheDefaultMaxMb;
  }
  if (opts.max_mb > kCheckCacheMaxDirMb) {
    opts.max_mb = kCheckCacheMaxDirMb;
  }
  if (!opts.cache_dir.empty()) {
    opts.cache_dir = weakly_canonical_path(opts.cache_dir);
  }
}

bool apply_check_cache_flag(std::string_view arg, CheckCacheOptions& out) {
  if (arg == "--no-cache") {
    out.enabled = false;
    return true;
  }
  if (arg.rfind("--cache-dir=", 0) == 0) {
    out.cache_dir = std::filesystem::path(std::string(arg.substr(12)));
    return true;
  }
  if (arg.rfind("--cache-max-mb=", 0) == 0) {
    const int n = std::atoi(arg.substr(15).data());
    if (n > 0) {
      out.max_mb = static_cast<std::size_t>(n);
      if (out.max_mb > kCheckCacheMaxDirMb) {
        out.max_mb = kCheckCacheMaxDirMb;
      }
    }
    return true;
  }
  return false;
}

CheckCacheHit try_load_check_cache(const CheckCacheOptions& opts, const std::string& key,
                                   std::uint64_t expected_content_hash) {
  CheckCacheHit hit;
  if (!opts.enabled || opts.cache_dir.empty()) {
    return hit;
  }
  const auto path_opt = resolve_cache_entry_path(opts.cache_dir, key);
  if (!path_opt) {
    return hit;
  }
  if (!read_cache_file(*path_opt, expected_content_hash, hit)) {
    hit.hit = false;
    hit.output.clear();
  }
  return hit;
}

void store_check_cache(const CheckCacheOptions& opts, const std::string& key, int exit_code,
                       const std::string& output, std::uint64_t content_hash) {
  if (!opts.enabled || opts.cache_dir.empty() || output.size() > kCheckCacheMaxEntryBytes) {
    return;
  }
  const auto path_opt = resolve_cache_entry_path(opts.cache_dir, key);
  if (!path_opt) {
    return;
  }
  const auto path = *path_opt;
  if (is_symlink_path(path)) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(opts.cache_dir, ec);
  std::ofstream out(path, std::ios::trunc);
  if (!out) {
    return;
  }
  out << "v=" << kCacheFormatVersion << "\nexit=" << exit_code << "\ncontent=" << hex64(content_hash)
      << "\n---\n" << output;
  out.close();
  evict_check_cache_lru(opts.cache_dir, opts.max_mb);
}

void evict_check_cache_lru(const std::filesystem::path& cache_dir, std::size_t max_mb) {
  if (max_mb == 0 || !std::filesystem::exists(cache_dir)) {
    return;
  }
  const std::size_t cap = max_mb * 1024u * 1024u;
  struct Entry {
    std::filesystem::path path;
    std::filesystem::file_time_type mtime;
    std::uintmax_t size = 0;
  };
  std::vector<Entry> entries;
  for (const auto& item : std::filesystem::directory_iterator(cache_dir)) {
    if (!item.is_regular_file()) {
      continue;
    }
    const auto sz = item.file_size();
    if (sz > kCheckCacheMaxEntryBytes) {
      std::error_code ec;
      std::filesystem::remove(item.path(), ec);
      continue;
    }
    entries.push_back(
        Entry{item.path(), std::filesystem::last_write_time(item.path()), sz});
  }
  auto total = dir_size_bytes(cache_dir);
  std::sort(entries.begin(), entries.end(),
            [](const Entry& a, const Entry& b) { return a.mtime < b.mtime; });
  for (const auto& entry : entries) {
    if (total <= cap) {
      break;
    }
    std::error_code ec;
    std::filesystem::remove(entry.path, ec);
    if (!ec) {
      total -= static_cast<std::size_t>(entry.size);
    }
  }
}

}  // namespace li
