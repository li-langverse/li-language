#include "li/check_cmd.hpp"
#include "li/workspace_check.hpp"

#include "li/advisory.hpp"
#include "li/check_cache.hpp"
#include "li/resource_options.hpp"
#include "li/check_config.hpp"
#include "li/import_resolve.hpp"
#include "li/parser.hpp"
#include "li/platform.hpp"
#include "li/policy.hpp"
#include "li/prelude.hpp"
#include "li/typecheck.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

namespace li {
namespace {

enum class DiagOutput { Human, Json };

#ifdef LI_VERSION
constexpr const char* kCompilerVersion = LI_VERSION;
#else
constexpr const char* kCompilerVersion = "dev";
#endif

std::string read_file(const char* path) {
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void append_diagnostic(DiagnosticBag& out, const Diagnostic& d) {
  const std::string hint = d.hint ? *d.hint : std::string{};
  if (d.severity == DiagnosticSeverity::Error) {
    if (!d.code.empty()) {
      out.error(d.loc, d.code, d.message, hint);
    } else {
      out.error(d.loc, d.message);
    }
  } else if (d.severity == DiagnosticSeverity::Warning) {
    out.warning(d.loc, d.code, d.message, hint);
  } else {
    out.note(d.loc, d.code, d.message, hint);
  }
}

int check_exit_code(const DiagnosticBag& diags, bool deny_warnings) {
  if (diags.has_errors()) {
    return 1;
  }
  if (deny_warnings && diags.has_warnings()) {
    return 1;
  }
  return 0;
}

void finalize_check_cache(CheckCacheOptions& cache) {
  if (cache.enabled && cache.cache_dir.empty()) {
    cache.cache_dir = std::filesystem::path(repo_build_path("check-cache"));
  }
  normalize_check_cache_options(cache);
}

bool is_workspace_flag(std::string_view arg) {
  return arg == "--workspace" || arg.rfind("--workspace=", 0) == 0;
}

int check_file(const char* path, const CheckCommandOptions& opts, DiagOutput output,
               std::string_view json_command) {
  const std::filesystem::path file_path(path);
  const CheckConfig cfg = load_check_config(file_path);
  const std::string config_hash = check_config_hash(cfg);
  const std::uint64_t content_hash = hash_file_content(file_path);
  const std::uint64_t import_hash = hash_direct_import_graph(file_path);
  const std::string_view mode_tag =
      (output == DiagOutput::Json) ? json_command : std::string_view("human");
  const std::string key = make_check_cache_key(
      file_path, content_hash, config_hash, check_cache_compiler_version(), import_hash,
      hash_bytes_fnv(mode_tag));
  const bool use_cache = opts.cache.enabled && !opts.cache.cache_dir.empty();

  if (use_cache) {
    const CheckCacheHit hit = try_load_check_cache(opts.cache, key, content_hash);
    if (hit.hit) {
      if (!hit.output.empty()) {
        std::cout << hit.output;
        if (hit.output.back() != '\n') {
          std::cout << '\n';
        }
      }
      return hit.exit_code;
    }
  }

  const std::string source = read_file(path);
  Module module;
  DiagnosticBag diags;
  FrontendCheckOptions frontend;
  frontend.deny_warnings = opts.deny_warnings;

  const bool ok = run_frontend_check(path, source, module, diags, frontend);
  int exit_code = ok ? check_exit_code(diags, opts.deny_warnings) : 1;
  std::string cache_payload;

  if (output == DiagOutput::Json) {
    std::ostringstream json_out;
    print_diagnostics_json(diags, json_out, json_command);
    cache_payload = json_out.str();
    std::cout << cache_payload;
    if (!cache_payload.empty() && cache_payload.back() != '\n') {
      std::cout << '\n';
    }
  } else if (!diags.empty()) {
    // Advisory mode: surface warnings/notes on success too (exit stays 0
    // unless --deny-warnings escalated them), so `lic check` never hides a
    // warning behind a green exit code.
    std::ostringstream human_out;
    render_diagnostics(diags, human_out);
    cache_payload = human_out.str();
    std::cout << cache_payload;
    if (!cache_payload.empty() && cache_payload.back() != '\n') {
      std::cout << '\n';
    }
  }

  // Human-mode failures print diagnostics to stderr directly, so `cache_payload`
  // is empty for them; caching would replay an empty (unhelpful) result on the
  // next run. Only cache successes and JSON-mode results, which carry a payload.
  if (use_cache && (exit_code == 0 || output == DiagOutput::Json)) {
    store_check_cache(opts.cache, key, exit_code, cache_payload, content_hash);
  }
  return exit_code;
}

}  // namespace

bool run_frontend_check(const char* path, const std::string& source, Module& out,
                        DiagnosticBag& diags, const FrontendCheckOptions& options) {
  const CheckConfig cfg = load_check_config(path);
  const AdvisoryOptions advisory_opts{cfg};

  check_source_policies(source, path, cfg, diags);
  if (diags.has_errors()) {
    return false;
  }

  auto parsed = parse_module(source, path);
  for (const auto& d : parsed.diagnostics.items()) {
    append_diagnostic(diags, d);
  }
  if (!parsed.module) {
    return false;
  }

  check_stdlib_seal(*parsed.module, path, diags);
  if (diags.has_errors()) {
    return false;
  }
  if (!resolve_imports(*parsed.module, path, diags)) {
    return false;
  }
  check_module_policies(*parsed.module, path, diags);
  if (diags.has_errors()) {
    return false;
  }
  check_duplicate_definitions(*parsed.module, path, diags);
  if (diags.has_errors()) {
    return false;
  }

  run_advisory_passes(*parsed.module, path, advisory_opts, diags);
  auto checked = typecheck_module(*parsed.module, parsed.module->procs.size());
  for (const auto& d : checked.diagnostics.items()) {
    append_diagnostic(diags, d);
  }
  if (!checked.ok) {
    return false;
  }
  if (options.deny_warnings && diags.has_warnings()) {
    SourceLoc loc{path, 1, 1, 0};
    diags.error(loc, "warnings denied by --deny-warnings");
    return false;
  }
  out = std::move(*parsed.module);
  return true;
}

int lic_check_main(int argc, char** argv, const char* lic_executable) {
  if (argc < 3) {
    std::cerr << "usage: lic check <file> | --workspace[=li.toml] [--jobs=N] [--max-memory=MB] "
                 "[--cache-dir=PATH] [--cache-max-mb=N] [--no-cache] [--format=json] "
                 "[--deny-warnings]\n";
    return 1;
  }

  CheckCommandOptions opts;
  finalize_check_cache(opts.cache);
  const char* path = nullptr;
  DiagOutput output = DiagOutput::Human;
  bool has_workspace = false;

  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (is_workspace_flag(arg)) {
      has_workspace = true;
      continue;
    }
    if (arg == "--format=json") {
      output = DiagOutput::Json;
      continue;
    }
    if (arg == "--deny-warnings") {
      opts.deny_warnings = true;
      continue;
    }
    if (apply_check_cache_flag(arg, opts.cache)) {
      continue;
    }
    if (apply_resource_flag(arg, resource_options())) {
      continue;
    }
    if (path == nullptr && !arg.empty() && arg[0] != '-') {
      path = argv[i];
      continue;
    }
    std::cerr << "usage: lic check <file> | --workspace[=li.toml] [--jobs=N] [--max-memory=MB] "
                 "[--cache-dir=PATH] [--cache-max-mb=N] [--no-cache] [--format=json] "
                 "[--deny-warnings]\n";
    return 1;
  }

  if (has_workspace) {
    finalize_resource_options(resource_options());
    return lic_workspace_check_main(argc, argv, lic_executable, opts.cache);
  }

  finalize_resource_options(resource_options());

  if (path == nullptr) {
    std::cerr << "usage: lic check <file> | --workspace[=li.toml] ...\n";
    return 1;
  }
  return check_file(path, opts, output, "check");
}

int lic_diagnose_main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: lic diagnose <file> [--deny-warnings] [--cache-dir=PATH] [--no-cache]\n";
    return 1;
  }
  CheckCommandOptions opts;
  finalize_check_cache(opts.cache);
  const char* path = nullptr;
  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "--deny-warnings") {
      opts.deny_warnings = true;
      continue;
    }
    if (apply_check_cache_flag(arg, opts.cache)) {
      continue;
    }
    if (path == nullptr) {
      path = argv[i];
    } else {
      std::cerr << "usage: lic diagnose <file> [--deny-warnings] [--cache-dir=PATH] [--no-cache]\n";
      return 1;
    }
  }
  if (path == nullptr) {
    std::cerr << "usage: lic diagnose <file> [--deny-warnings] [--cache-dir=PATH] [--no-cache]\n";
    return 1;
  }
  return check_file(path, opts, DiagOutput::Json, "diagnose");
}

}  // namespace li
