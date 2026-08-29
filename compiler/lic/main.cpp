#include "li/compile.hpp"
#include "li/mir.hpp"
#include "li/mir_dump.hpp"
#include "li/parser.hpp"
#include "li/policy.hpp"
#include "li/smoke_llvm.hpp"
#include "li/typecheck.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int usage() {
  std::cerr << "lic — Li compiler\n"
            << "usage:\n"
            << "  lic parse <file>       parse and validate syntax\n"
            << "  lic check <file>       parse + typecheck\n"
            << "  lic build <file> -o <out> [--release]\n"
            << "  lic mir <file>         lower to MIR and dump\n"
            << "  lic smoke-llvm         verify LLVM can emit main returning 0\n"
            << "  lic --version          print version\n";
  return 1;
}

std::string read_file(const char* path) {
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

// ---- Import resolution (mirrors li_rt_resolve_import in runtime/li_rt.c) ----
//
// The self-hosted walker resolves `import X` with this exact candidate chain
// (bootstrap/lic/main.li pre-scan + runtime/li_rt.c li_rt_resolve_import):
//   1. same-directory sibling      <dir>/X.li
//   2. same-directory package      <dir>/X/X.li
//   3. workspace root (walk up to the first dir containing packages/):
//      a. packages/li-<dash(X)>/src/lib.li          (dots/underscores -> '-')
//      b. packages/<dash(X)>/src/lib.li
//      c. li_<rest> imports strip the li_ prefix, then packages/li-<dash>/src/lib.li
//   4. std.* tree                  <root>/std/<segments...>/<leaf>.li
//      and std package form       <root>/std/<all>/<all>.li
//      plus fallback              <root>/packages/li-<dash>/src/lib.li
// Resolution is per-import, in the walker's import order, with dedup by the
// canonical resolved path. The walker emits main's procs first, then each
// resolved import (recursively) in resolution order.

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
    const std::string p2a = r + "/packages/li-" + dash + "/src/lib.li";
    if (auto p = candidate(p2a)) {
      return p;
    }
    // 2b. packages/<dash>/src/lib.li
    const std::string p2b = r + "/packages/" + dash + "/src/lib.li";
    if (auto p = candidate(p2b)) {
      return p;
    }
    // 2c. li_<rest> strips the li_ prefix, then packages/li-<dash>/src/lib.li
    if (mlen > 3 && m[0] == 'l' && m[1] == 'i' && m[2] == '_') {
      const std::string rest = m.substr(3);
      const std::string p2c = r + "/packages/li-" + dash_convert(rest) + "/src/lib.li";
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
      const std::string p4 = r + "/packages/li-" + dash_convert(stripped) + "/src/lib.li";
      if (auto p = candidate(p4)) {
        return p;
      }
    }
  }
  return std::nullopt;
}

// Parse a file (with its own import declarations preserved) into a module.
li::ParseResult parse_import_file(const std::string& path) {
  const std::string src = read_file(path.c_str());
  return li::parse_module(src, path);
}

// Merge `from` procs/types into `to` (all procs; the walker emits every proc
// it walks, public or not). Types merge so imported object types resolve.
void merge_imported(li::Module& to, li::Module&& from) {
  for (auto& t : from.types) {
    to.types.push_back(std::move(t));
  }
  for (auto& p : from.procs) {
    to.procs.push_back(std::move(p));
  }
}

// Resolve all imports of `mod` (already parsed, with .imports populated) and
// append the imported modules' procs/types, recursively, in walker order.
// Dedup by resolved canonical path. Mirrors the walker's transitive import
// scan + emit (main first, then imports in resolution order).
void resolve_module_imports(li::Module& mod, const std::string& base_file,
                            std::set<std::string>& resolved) {
  const std::vector<li::ImportDecl> imports = mod.imports;
  for (const auto& imp : imports) {
    const auto path = resolve_import_path(imp.module, base_file);
    if (!path) {
      continue;
    }
    const std::string canon = std::filesystem::weakly_canonical(*path).string();
    if (resolved.count(canon)) {
      continue;
    }
    resolved.insert(canon);
    auto parsed = parse_import_file(*path);
    if (!parsed.module) {
      continue;
    }
    // Recursively resolve this import's own imports (base = its path).
    resolve_module_imports(*parsed.module, *path, resolved);
    merge_imported(mod, std::move(*parsed.module));
  }
}

bool frontend(const char* path, const std::string& source, li::Module& out,
              li::DiagnosticBag& diags) {
  li::check_source_policies(source, path, diags);
  if (!diags.empty()) {
    return false;
  }
  auto parsed = li::parse_module(source, path);
  for (const auto& d : parsed.diagnostics.items()) {
    diags.error(d.loc, d.message);
  }
  // parse_module may recover and still build a (partial) module after emitting
  // parse diagnostics; reject on any parse error so `lic check`/`lic mir`/
  // `lic build` reject malformed input exactly where the Li walker does.
  if (!parsed.module || !parsed.diagnostics.empty()) {
    return false;
  }
  auto checked = li::typecheck_module(*parsed.module);
  for (const auto& d : checked.diagnostics.items()) {
    diags.error(d.loc, d.message);
  }
  if (!checked.ok) {
    return false;
  }
  out = std::move(*parsed.module);
  return true;
}

int check_file(const char* path) {
  const std::string source = read_file(path);
  li::Module module;
  li::DiagnosticBag diags;
  if (!frontend(path, source, module, diags)) {
    li::print_diagnostics(diags);
    return 1;
  }
  return 0;
}

int build_file(const char* path, const char* output, bool release) {
  const std::string source = read_file(path);
  li::Module module;
  li::DiagnosticBag diags;
  if (!frontend(path, source, module, diags)) {
    li::print_diagnostics(diags);
    return 1;
  }
  std::string err;
  if (!li::compile_module(module, output, release, "", &err)) {
    std::cerr << "build failed: " << err << '\n';
    return 1;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    return usage();
  }
  const std::string_view cmd = argv[1];
  if (cmd == "--version" || cmd == "-V") {
#ifdef LI_VERSION
    std::cout << "lic " << LI_VERSION << '\n';
#else
    std::cout << "lic 0.0.0-dev\n";
#endif
    return 0;
  }
  if (cmd == "smoke-llvm") {
    std::string err;
    if (!li::smoke_llvm(&err)) {
      std::cerr << "smoke-llvm failed: " << err << '\n';
      return 1;
    }
    std::cout << "smoke-llvm: ok (main returns 0)\n";
    return 0;
  }
  if (cmd == "parse") {
    if (argc < 3) {
      return usage();
    }
    const std::string source = read_file(argv[2]);
    auto result = li::parse_module(source, argv[2]);
    if (!result.ok()) {
      li::print_diagnostics(result.diagnostics);
      return 1;
    }
    return 0;
  }
  if (cmd == "check") {
    if (argc < 3) {
      return usage();
    }
    return check_file(argv[2]);
  }
  if (cmd == "build") {
    if (argc < 3) {
      return usage();
    }
    const char* input = argv[2];
    const char* output = "/dev/null";
    bool release = false;
    std::string extra_flags;
    for (int i = 3; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "-o" && i + 1 < argc) {
        output = argv[++i];
      } else if (arg == "--release") {
        release = true;
      } else if (arg == "--allow-open-vc" || arg == "--no-lean-verify") {
        // Frontend-only flags: the parity/CI harness passes them when
        // building bootstrap/lic/main.li. This frontend has no external
        // prover step, so they are consumed here and never reach clang.
        continue;
      } else {
        extra_flags.append(argv[i]);
        extra_flags.push_back(' ');
      }
    }
    const std::string source = read_file(input);
    li::Module module;
    li::DiagnosticBag diags;
    if (!frontend(input, source, module, diags)) {
      li::print_diagnostics(diags);
      return 1;
    }
    std::string err;
    if (!li::compile_module(module, output, release, extra_flags, &err)) {
      std::cerr << "build failed: " << err << '\n';
      return 1;
    }
    return 0;
  }
  if (cmd == "mir") {
    if (argc < 3) {
      return usage();
    }
    const std::string source = read_file(argv[2]);
    li::Module module;
    li::DiagnosticBag diags;
    if (!frontend(argv[2], source, module, diags)) {
      li::print_diagnostics(diags);
      return 1;
    }
    // Resolve imports into the module (walker order: main procs first, then
    // each import's procs recursively, dedup by canonical path).
    std::set<std::string> resolved;
    resolve_module_imports(module, argv[2], resolved);
    auto mir = li::lower_to_mir(module);
    std::cout << li::dump_mir_module(mir);
    return 0;
  }
  return usage();
}
