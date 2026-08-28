#include "li/resource_options.hpp"

#include <cstdlib>
#include <iostream>

namespace li {
namespace {

ResourceOptions g_opts;
bool g_env_warned = false;

void warn_deprecated_env_once(const char* var, const char* flag) {
  if (g_env_warned) {
    return;
  }
  g_env_warned = true;
  std::cerr << "lic: warning: " << var << " is deprecated; use " << flag << " on the command line\n";
}

void warn_env_ignored_once(const char* var) {
  if (g_env_warned) {
    return;
  }
  g_env_warned = true;
  std::cerr << "lic: warning: " << var << " is set but ignored because the matching flag was passed\n";
}

unsigned parse_positive(const char* s) {
  if (s == nullptr || *s == '\0') {
    return 0;
  }
  const int n = std::atoi(s);
  return n > 0 ? static_cast<unsigned>(n) : 0u;
}

std::size_t parse_positive_size(const char* s) {
  const unsigned n = parse_positive(s);
  return n > 0 ? static_cast<std::size_t>(n) : 0u;
}

}  // namespace

ResourceOptions& resource_options() { return g_opts; }

void reset_resource_options() {
  g_opts = ResourceOptions{};
  g_env_warned = false;
}

unsigned ResourceOptions::effective_jobs(unsigned default_per_job_mb) const {
  unsigned j = jobs > 0 ? jobs : 1u;
  const std::size_t per = job_memory_mb > 0 ? job_memory_mb : default_per_job_mb;
  if (max_memory_mb == 0 || per == 0) {
    return j;
  }
  const std::size_t cap = max_memory_mb / per;
  const unsigned capped = cap < 1 ? 1u : static_cast<unsigned>(cap);
  return j < capped ? j : capped;
}

bool apply_resource_flag(std::string_view arg, ResourceOptions& out) {
  if (arg.rfind("--jobs=", 0) == 0) {
    out.jobs = parse_positive(arg.substr(7).data());
    out.jobs_from_flag = out.jobs > 0;
    return true;
  }
  if (arg.rfind("--max-memory=", 0) == 0) {
    out.max_memory_mb = parse_positive_size(arg.substr(13).data());
    out.max_memory_from_flag = out.max_memory_mb > 0;
    return true;
  }
  if (arg.rfind("--job-memory-mb=", 0) == 0) {
    out.job_memory_mb = parse_positive_size(arg.substr(16).data());
    out.job_memory_from_flag = out.job_memory_mb > 0;
    return true;
  }
  if (arg.rfind("--build-dir=", 0) == 0) {
    out.build_dir = std::string(arg.substr(12));
    out.build_dir_from_flag = !out.build_dir.empty();
    return true;
  }
  if (arg.rfind("--threads=", 0) == 0) {
    out.threads = parse_positive(arg.substr(10).data());
    out.threads_from_flag = out.threads > 0;
    return true;
  }
  return false;
}

void finalize_resource_options(ResourceOptions& opts) {
  if (!opts.jobs_from_flag && std::getenv("LI_COMPILE_JOBS")) {
    if (const unsigned v = parse_positive(std::getenv("LI_COMPILE_JOBS")); v > 0) {
      opts.jobs = v;
      warn_deprecated_env_once("LI_COMPILE_JOBS", "--jobs=N");
    }
  } else if (opts.jobs_from_flag && std::getenv("LI_COMPILE_JOBS")) {
    warn_env_ignored_once("LI_COMPILE_JOBS");
  }
  if (!opts.max_memory_from_flag && std::getenv("LI_MAX_MEMORY_MB")) {
    if (const std::size_t v = parse_positive_size(std::getenv("LI_MAX_MEMORY_MB")); v > 0) {
      opts.max_memory_mb = v;
      warn_deprecated_env_once("LI_MAX_MEMORY_MB", "--max-memory=MB");
    }
  } else if (opts.max_memory_from_flag && std::getenv("LI_MAX_MEMORY_MB")) {
    warn_env_ignored_once("LI_MAX_MEMORY_MB");
  }
  if (!opts.build_dir_from_flag && std::getenv("LI_BUILD_DIR")) {
    if (const char* env = std::getenv("LI_BUILD_DIR"); env && env[0]) {
      opts.build_dir = env;
      warn_deprecated_env_once("LI_BUILD_DIR", "--build-dir=PATH");
    }
  } else if (opts.build_dir_from_flag && std::getenv("LI_BUILD_DIR")) {
    warn_env_ignored_once("LI_BUILD_DIR");
  }
  if (!opts.threads_from_flag && std::getenv("LI_OMP_THREADS")) {
    if (const unsigned v = parse_positive(std::getenv("LI_OMP_THREADS")); v > 0) {
      opts.threads = v;
      warn_deprecated_env_once("LI_OMP_THREADS", "--threads=N");
    }
  } else if (opts.threads_from_flag && std::getenv("LI_OMP_THREADS")) {
    warn_env_ignored_once("LI_OMP_THREADS");
  }
  if (opts.jobs == 0) {
    opts.jobs = 1;
  }
}

unsigned g_compile_jobs_reserved = 1;
void note_compile_jobs_reserved(const ResourceOptions& opts) {
  g_compile_jobs_reserved = opts.effective_jobs();
  setenv("LI_COMPILE_JOBS", std::to_string(g_compile_jobs_reserved).c_str(), 1);
}
unsigned compile_jobs_reserved() { return g_compile_jobs_reserved; }

}  // namespace li
