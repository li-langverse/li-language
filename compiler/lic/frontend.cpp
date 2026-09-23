#include "li/frontend.hpp"

#include "li/parser.hpp"
#include "li/policy.hpp"
#include "li/prelude.hpp"
#include "li/typecheck.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>

namespace li {
namespace {

std::string read_file(const char* path) {
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// Repo-relative build path (LI_REPO_ROOT/build/<rel>, fallback "build/<rel>").
std::string dash_convert(const std::string& s) {
  std::string out = s;
  for (char& c : out) {
    if (c == '.' || c == '_') {
      c = '-';
    }
  }
  return out;
}

std::optional<std::string> find_workspace_root(const std::string& file_path) {
  const auto slash = file_path.find_last_of('/');
  if (slash == std::string::npos) {
    return std::nullopt;
  }
  std::string dir = file_path.substr(0, slash);
  while (!dir.empty()) {
    if (std::filesystem::is_directory(dir + "/packages")) {
      return dir;
    }
    const auto p = dir.find_last_of('/');
    if (p == std::string::npos) {
      break;
    }
    dir = dir.substr(0, p);
  }
  if (std::filesystem::is_directory("packages")) {
    return std::string(".");
  }
  return std::nullopt;
}

bool try_read_import(const std::string& path, std::string& out) {
  std::ifstream in(path);
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

ParseResult parse_import_file(const std::string& path) {
  const std::string src = read_file(path.c_str());
  return parse_module(src, path);
}

// Merge `from` procs/types into `to` (all procs; the walker emits every proc
// it walks, public or not). Types merge so imported object types resolve, and
// proc signatures merge so main-module calls to imported procs resolve their
// return types (mirroring the walker's import pre-scan).
void merge_imported(Module& to, Module&& from) {
  for (auto& t : from.types) {
    to.types.push_back(std::move(t));
  }
  for (auto& p : from.procs) {
    // `private def` procs are invisible to importing modules (the walker
    // records ep_priv and never surfaces them in import scope), so calls to
    // them from an importer fail with an unknown-proc verdict.
    if (p.is_private) {
      continue;
    }
    to.procs.push_back(std::move(p));
  }
}

// Resolve all imports of `mod` (already parsed, with .imports populated) and
// append the imported modules' procs/types, recursively, in walker order.
// Dedup by resolved canonical path. Mirrors the walker's transitive import
// scan + emit (main first, then imports in resolution order).
bool resolve_module_imports(Module& mod, const std::string& base_file,
                            std::set<std::string>& resolved,
                            std::set<std::string>& loading,
                            DiagnosticBag& diags) {
  const std::vector<ImportDecl> imports = mod.imports;
  for (const auto& imp : imports) {
    const auto path = resolve_import_path(imp.module, base_file);
    if (!path) {
      diags.error(SourceLoc{base_file, 1, 1, imp.span.start},
                  "import_resolve: module not found: " + imp.module);
      continue;
    }
    const std::string canon = std::filesystem::weakly_canonical(*path).string();
    if (resolved.count(canon)) {
      continue;
    }
    if (loading.count(canon)) {
      diags.error(SourceLoc{base_file, 1, 1, imp.span.start},
                  "import_cycle: " + canon);
      continue;
    }
    loading.insert(canon);
    auto parsed = parse_import_file(*path);
    // Imported modules follow the walker's declaration-loading contract: use
    // the recovered module when available, but do not surface its parser
    // diagnostics as errors in the importing file. Structural resolver
    // failures (missing module/cycle) are reported above.
    if (!parsed.module) {
      loading.erase(canon);
      diags.error(SourceLoc{base_file, 1, 1, imp.span.start},
                  "import_parse: unable to parse " + canon);
      continue;
    }
    // Recursively resolve this import's own imports before merging it, matching
    // the walker's depth-first import traversal and making cycle state explicit.
    const bool nested_ok = resolve_module_imports(*parsed.module, *path, resolved,
                                                   loading, diags);
    loading.erase(canon);
    if (!nested_ok) {
      continue;
    }
    resolved.insert(canon);
    merge_imported(mod, std::move(*parsed.module));
  }
  return diags.empty();
}

}  // namespace

// Mirror li_rt_resolve_import candidate order. Returns the resolved file path.
std::optional<std::string> resolve_import_path(const std::string& module,
                                               const std::string& base_file) {
  const auto slash = base_file.find_last_of('/');
  const std::string dir =
      slash == std::string::npos ? std::string() : base_file.substr(0, slash);
  const std::string& m = module;
  const std::size_t mlen = m.size();
  const std::string dash = dash_convert(m);
  std::string content;

  auto candidate = [&](const std::string& path) -> std::optional<std::string> {
    if (try_read_import(path, content)) {
      return path;
    }
    return std::nullopt;
  };

  // 1. same-directory sibling: <dir>/<module>.li
  const std::string sib = dir.empty() ? m + ".li" : dir + "/" + m + ".li";
  if (auto p = candidate(sib)) {
    return p;
  }
  // 1b. same-directory package: <dir>/<module>/<module>.li
  const std::string sibpkg =
      dir.empty() ? m + "/" + m + ".li" : dir + "/" + m + "/" + m + ".li";
  if (auto p = candidate(sibpkg)) {
    return p;
  }

  // 2. workspace package candidates (walk up to the first dir with packages/).
  if (const auto root = find_workspace_root(base_file)) {
    const std::string r = *root;
    // 2a. packages/li-<dash>/src/lib.li
    const std::string p2a = r + "/packages/li-" + dash + "/src/lib.li";  // carve-out: import-resolver (compile-time package source lookup)
    if (auto p = candidate(p2a)) {
      return p;
    }
    // 2b. packages/<dash>/src/lib.li
    const std::string p2b = r + "/packages/" + dash + "/src/lib.li";  // carve-out: import-resolver (compile-time package source lookup)
    if (auto p = candidate(p2b)) {
      return p;
    }
    // 2c. li_<rest> strips the li_ prefix, then packages/li-<dash>/src/lib.li
    if (mlen > 3 && m[0] == 'l' && m[1] == 'i' && m[2] == '_') {
      const std::string rest = m.substr(3);
      const std::string p2c = r + "/packages/li-" + dash_convert(rest) + "/src/lib.li";  // carve-out: import-resolver (compile-time package source lookup)
      if (auto p = candidate(p2c)) {
        return p;
      }
    }
    // 3. std.* tree: std/<segments...>/<leaf>.li and std/<all>/<all>.li
    if (mlen > 4 && m.compare(0, 4, "std.") == 0) {
      const std::string stripped = m.substr(4);
      const std::size_t last_dot = stripped.find_last_of('.');
      // 3a. std/<prefix-as-dirs>/<leaf>.li
      std::string seg_path;
      if (last_dot != std::string::npos) {
        for (std::size_t i = 0; i < last_dot; ++i) {
          seg_path += stripped[i] == '.' ? '/' : stripped[i];
        }
        if (last_dot > 0) {
          seg_path += "/";
        }
        seg_path += stripped.substr(last_dot + 1);
        const std::string p3a = r + "/std/" + seg_path + ".li";
        if (auto p = candidate(p3a)) {
          return p;
        }
      }
      // 3b. std/<all>/<all>.li
      const std::string p3b = r + "/std/" + stripped + "/" + stripped + ".li";
      if (auto p = candidate(p3b)) {
        return p;
      }
      // 4. fallback packages/li-<dash>/src/lib.li
      const std::string p4 = r + "/packages/li-" + dash_convert(stripped) + "/src/lib.li";  // carve-out: import-resolver (compile-time package source lookup)
      if (auto p = candidate(p4)) {
        return p;
      }
    }
  }

  // 5. same-package self-import: walk up to the nearest li.toml and, when its
  //    `name` (kebab/snake normalized) matches the module, resolve to the
  //    package's own src/lib.li (mirrors find_package_toml + same_package_entry
  //    in import_resolve.cpp, which the build/check paths don't link).
  {
    std::string dir = base_file;
    const auto first_slash = dir.find_last_of('/');
    if (first_slash != std::string::npos) {
      dir = dir.substr(0, first_slash);
    } else {
      dir.clear();
    }
    for (int depth = 0; depth < 12 && !dir.empty(); ++depth) {
      const std::string toml = dir + "/li.toml";
      std::ifstream in(toml);
      if (in) {
        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string text = ss.str();
        const std::size_t name_key = text.find("name");
        if (name_key != std::string::npos) {
          const std::size_t q1 = text.find('"', name_key);
          const std::size_t q2 = q1 == std::string::npos ? std::string::npos : text.find('"', q1 + 1);
          if (q1 != std::string::npos && q2 != std::string::npos) {
            std::string pkg_name = text.substr(q1 + 1, q2 - q1 - 1);
            for (char& c : pkg_name) {
              if (c == '-') {
                c = '_';
              }
            }
            if (pkg_name == m) {
              const std::string lib = dir + "/src/lib.li";
              if (auto p = candidate(lib)) {
                return p;
              }
            }
          }
        }
        break;
      }
      const auto p = dir.find_last_of('/');
      if (p == std::string::npos) {
        break;
      }
      dir = dir.substr(0, p);
    }
  }
  return std::nullopt;
}

bool frontend(const char* path, const std::string& source, Module& out,
              DiagnosticBag& diags) {
  check_source_policies(source, path, diags);
  if (!diags.empty()) {
    return false;
  }
  auto parsed = parse_module(source, path);
  for (const auto& d : parsed.diagnostics.items()) {
    diags.error(d.loc, d.message);
  }
  // parse_module may recover and still build a (partial) module after emitting
  // parse diagnostics; reject on any parse error so `lic check`/`lic mir`/
  // `lic build` reject malformed input exactly where the Li walker does.
  if (!parsed.module || !parsed.diagnostics.empty()) {
    return false;
  }
  // Module-level name rules (docs/language/stdlib.md): the prelude and the
  // std/ tree own a fixed set of names, and a module may not define the same
  // top-level name twice. This is the AST-based form of the two rules the
  // source-policy scan used to approximate with substring searches.
  check_duplicate_definitions(*parsed.module, path, diags);
  check_stdlib_seal(*parsed.module, path, diags);
  if (!diags.empty()) {
    return false;
  }
  // Merge imported types and proc signatures before typecheck so annotations
  // referencing imported types resolve (e.g. `var layout: StudioShellLayout =
  // ...`) and calls to imported procs resolve their return types - mirroring
  // the walker's import pre-scan, which registers imported types and procs
  // before walking the main module. Only the main module's own proc bodies
  // are checked (main_proc_count boundary): imported proc bodies are lowered
  // without a typecheck pass, so checking them would reject files whose
  // imports use constructs this frontend does not yet support.
  const std::size_t main_proc_count = parsed.module->procs.size();
  std::set<std::string> resolved;
  std::set<std::string> loading;
  if (!resolve_module_imports(*parsed.module, path, resolved, loading, diags)) {
    return false;
  }
  auto checked = typecheck_module(*parsed.module, main_proc_count);
  for (const auto& d : checked.diagnostics.items()) {
    diags.error(d.loc, d.message);
  }
  if (!checked.ok) {
    return false;
  }
  out = std::move(*parsed.module);
  return true;
}

}  // namespace li