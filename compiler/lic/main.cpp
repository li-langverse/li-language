#include "li/compile.hpp"
#include "li/lexer.hpp"
#include "li/mir.hpp"
#include "li/mir_dump.hpp"
#include "li/parser.hpp"
#include "li/policy.hpp"
#include "li/proof_cli.hpp"
#include "li/smoke_llvm.hpp"
#include "li/typecheck.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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
            << "  lic lex <file>         dump token stream\n"
            << "  lic ast <file>         dump AST\n"
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
// it walks, public or not). Types merge so imported object types resolve, and
// proc signatures merge so main-module calls to imported procs resolve their
// return types (mirroring the walker's import pre-scan).
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
bool resolve_module_imports(li::Module& mod, const std::string& base_file,
                            std::set<std::string>& resolved,
                            std::set<std::string>& loading,
                            li::DiagnosticBag& diags) {
  const std::vector<li::ImportDecl> imports = mod.imports;
  for (const auto& imp : imports) {
    const auto path = resolve_import_path(imp.module, base_file);
    if (!path) {
      diags.error(li::SourceLoc{base_file, 1, 1, imp.span.start},
                  "import_resolve: module not found: " + imp.module);
      continue;
    }
    const std::string canon = std::filesystem::weakly_canonical(*path).string();
    if (resolved.count(canon)) {
      continue;
    }
    if (loading.count(canon)) {
      diags.error(li::SourceLoc{base_file, 1, 1, imp.span.start},
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
      diags.error(li::SourceLoc{base_file, 1, 1, imp.span.start},
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
  auto checked = li::typecheck_module(*parsed.module, main_proc_count);
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
  if (cmd == "lex") {
    // Token-stream parity: dump `kind<TAB>lexeme` per token using the
    // walker's kind numbering so scripts/check_li_lexer_parity.sh can diff.
    if (argc < 3) {
      return usage();
    }
    const std::string source = read_file(argv[2]);
    li::Lexer lexer(source, argv[2]);
    li::DiagnosticBag diags;
    if (!lexer.tokenize(diags)) {
      li::print_diagnostics(diags);
      return 1;
    }
    // Map our C++ TokenKind enum to the walker's kind numbers.
    // The walker (bootstrap/lic/main.li) uses the github/main token.hpp
    // ordering; our branch's enum differs, so we translate here.
    static const int wk[] = {
      0,   // Eof -> 0
      1,   // Newline -> 1
      2,   // Indent -> 2
      3,   // Dedent -> 3
      4,   // Ident -> 4
      5,   // IntLit -> 5
      6,   // FloatLit -> 6
      8,   // StringLit -> 8 (walker has BinaryLit=7 first)
      9,   // KwProc -> 9
      11,  // KwType -> 11 (walker: def=10, type=11)
      15,  // KwObject -> 15
      19,  // KwEnum -> 19
      20,  // KwVar -> 20
      21,  // KwLet -> 21
      22,  // KwIf -> 22
      23,  // KwElse -> 23
      24,  // KwElif -> 24
      25,  // KwWhile -> 25
      27,  // KwBreak -> 27 (walker: for=26, break=27)
      28,  // KwContinue -> 28
      30,  // KwReturn -> 30 (walker: error=29, return=30)
      31,  // KwRaises -> 31
      0,   // KwEcho -> remapped per-token (private=16, public=17, echo=skip)
      32,  // KwExtern -> 32
      35,  // KwTrue -> 35 (walker: async=33, await=34, true=35)
      36,  // KwFalse -> 36
      37,  // KwAnd -> 37
      38,  // KwOr -> 38
      39,  // KwNot -> 39
      40,  // KwIs -> 40
      41,  // KwRequires -> 41
      42,  // KwEnsures -> 42
      44,  // KwDecreases -> 44 (walker: prob_ensures=43, decreases=44)
      45,  // KwInvariant -> 45
      46,  // KwResult -> 46
      47,  // KwProtocol -> 47
      48,  // KwCallable -> 48
      18,  // KwImport -> 18
      49,  // LParen -> 49
      50,  // RParen -> 50
      51,  // LBracket -> 51
      52,  // RBracket -> 52
      53,  // LBrace -> 53
      54,  // RBrace -> 54
      55,  // Comma -> 55
      56,  // Colon -> 56
      57,  // Arrow -> 57
      58,  // Eq -> 58
      59,  // Plus -> 59
      60,  // Minus -> 60
      61,  // Star -> 61
      63,  // Slash -> 63 (walker: star=61, **=62, /=63)
      65,  // Mod -> 65 (walker: //=64, %=65)
      64,  // FloorDiv -> 64 (walker: //=64)
      62,  // StarStar -> 62 (walker: **=62)
      76,  // At -> 76 (walker: @=76)
      66,  // Le -> 66
      67,  // Lt -> 67
      68,  // Ge -> 68
      69,  // Gt -> 69
      70,  // EqEq -> 70
      71,  // Ne -> 71
      73,  // DotDotLt -> 73 (walker: ..<=73)
      74,  // Pipe -> 74 (walker: |=74)
      75,  // Ellipsis -> 75 (walker: ...=75)
      72,  // Dot -> 72 (walker: .=72)
      12,  // KwAxiom -> 12
      13,  // KwTheorem -> 13
      14,  // KwLemma -> 14
    };
    const int nelem = static_cast<int>(std::size(wk));
    for (const auto& t : lexer.tokens()) {
      int k = static_cast<int>(t.kind);
      if (k >= 0 && k < nelem && wk[k] >= 0) {
        int out_kind = wk[k];
        // Our branch folds KwDef into KwProc; the walker distinguishes them.
        if (t.kind == li::TokenKind::KwProc && std::string(t.text) == "def") {
          out_kind = 10;  // walker KwDef
        }
        // Our branch folds KwPrivate/KwPublic into KwEcho (unused slot).
        if (t.kind == li::TokenKind::KwEcho) {
          std::string s(t.text);
          if (s == "private") out_kind = 16;
          else if (s == "public") out_kind = 17;
          else continue;  // walker has no 'echo' keyword
        }
        std::cout << out_kind << '\t' << std::string(t.text) << '\n';
      }
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
    const char* input = nullptr;
    const char* output = "/dev/null";
    bool release = false;
    std::string extra_flags;
    for (int i = 2; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "-o" && i + 1 < argc) {
        output = argv[++i];
      } else if (arg == "--release") {
        release = true;
      } else if (arg == "--allow-open-vc") {
        li::proof_cli_flags().allow_open_vc = true;
      } else if (arg == "--no-lean-verify") {
        continue;
      } else if (input == nullptr) {
        input = argv[i];
      } else {
        extra_flags.append(argv[i]);
        extra_flags.push_back(' ');
      }
    }
    if (input == nullptr) {
      return usage();
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
    // Imports (types and procs) were already merged into the module by
    // frontend(), in walker order: main procs first, then each import's procs
    // recursively, dedup by canonical path. Lower the merged module directly.
    auto mir = li::lower_to_mir(module);
    std::cout << li::dump_mir_module(mir);
    return 0;
  }
  return usage();
}
