#include "li/ast_dump.hpp"
#include "li/check_cmd.hpp"
#include "li/compile.hpp"
#include "li/frontend.hpp"
#include "li/resource_options.hpp"
#include "li/lexer.hpp"
#include "li/mir.hpp"
#include "li/mir_dump.hpp"
#include "li/parser.hpp"
#include "li/proof_cli.hpp"
#include "li/smoke_llvm.hpp"
#include "li/typecheck.hpp"
#include "li/vc_emit.hpp"
#include "li/vc_summary.hpp"
#include "li/vc_witness.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
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
            << "  lic check --format=json <file>   agent JSON diagnostics\n"
            << "  lic check --workspace[=li.toml]  check workspace members\n"
            << "  lic diagnose <file>    JSON diagnostics (agent-facing)\n"
            << "  lic build <file> -o <out> [--release]\n"
            << "  lic verify <file>      VC summary; --lean runs semantics stub\n"
            << "  lic mir <file>         lower to MIR and dump\n"
            << "  lic httpd <validate-config|explain-config> <cfg.toml>\n"
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

// Single-quote an argument for the POSIX shell so `std::system` cannot be
// made to execute shell metacharacters embedded in a path/script name.
std::string shell_quote(const std::string& s) {
  std::string out = "'";
  for (const char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }
  out += "'";
  return out;
}

// Repo-relative build path (LI_REPO_ROOT/build/<rel>, fallback "build/<rel>").
// Mirrors li::repo_build_path without pulling the common library into lic.
std::string repo_build_path(const char* relative) {
  std::string prefix;
  if (const char* root = std::getenv("LI_REPO_ROOT")) {
    prefix = std::string(root) + "/build";
  } else {
    prefix = "build";
  }
  return prefix + "/" + relative;
}

// Agent CLI: `lic check --format=json` / `--workspace` / `lic diagnose`
// (li/check_cmd.hpp) build on the same walker-parity frontend above; plain
// `lic check <file>` stays on the parity path (see main's dispatch).

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

std::size_t count_mir_vectorized_proc(const li::MirModule& mir) {
  std::size_t n = 0;
  for (const auto& fn : mir.functions) {
    for (const auto& d : fn.decorators) {
      if (d.vectorized) {
        ++n;
        break;
      }
    }
  }
  return n;
}

std::size_t count_mir_parallel_disjoint_proven(const li::MirModule& mir) {
  std::size_t n = 0;
  for (const auto& fn : mir.functions) {
    for (const auto& d : fn.decorators) {
      if (d.parallel && d.disjoint_proven) {
        ++n;
      }
    }
  }
  return n;
}

// VC summary + MIR-linked witness telemetry (restored `lic verify`, dropped in
// the c132e1a9 squash merge; gates in li-tests/tooling/lic_verify_smoke.sh and
// contracts_verify_lean.sh depend on it). `--lean` runs the semantics stub.
int verify_file(const char* path, bool run_lean) {
  const std::string source = read_file(path);
  li::Module module;
  li::DiagnosticBag diags;
  if (!frontend(path, source, module, diags)) {
    li::print_diagnostics(diags);
    return 1;
  }
  std::string mir_err;
  const li::MirModule mir = li::lower_to_mir(module, &mir_err);
  if (!mir_err.empty()) {
    std::cerr << "verify: " << mir_err << '\n';
    return 1;
  }
  const li::VcSummary vc = li::summarize_vcs(module);
  const li::VcWitnessStats ws = li::compute_vc_witness_stats(module, &mir);
  std::cout << "verify: procs=" << vc.proc_count << " mir_fns=" << mir.functions.size()
            << " requires=" << vc.requires_count << " ensures=" << vc.ensures_count
            << " witnessed_ensures=" << ws.ensures_witnessed
            << " mir_return_linked=" << ws.mir_return_linked
            << " mir_vectorized_proc=" << count_mir_vectorized_proc(mir)
            << " mir_parallel_disjoint=" << count_mir_parallel_disjoint_proven(mir)
            << " decreases=" << vc.decreases_count << " invariant=" << vc.invariant_count
            << '\n';
  if (vc.requires_count == 0 && vc.ensures_count == 0) {
    std::cerr << "verify: warning \u2014 no procedure contracts (G-vc partial)\n";
  }
  if (std::getenv("LI_EMIT_VCS") != nullptr) {
    const std::string vc_path = repo_build_path("vcs.json");
    std::string vc_err;
    if (!li::write_vcs_json(module, vc_path, &vc_err)) {
      std::cerr << "verify: " << vc_err << '\n';
    } else {
      std::cout << "verify: wrote " << vc_path << '\n';
    }
  }
  if (!run_lean) {
    return 0;
  }
  std::string script = "scripts/lean-verify-stub.sh";
  if (const char* root = std::getenv("LI_REPO_ROOT")) {
    script = std::string(root) + "/" + script;
  }
  const std::string cmd = "bash " + shell_quote(script);
  return std::system(cmd.c_str()) == 0 ? 0 : 1;
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
  if (cmd == "httpd") {
    // M1 wrapper: `lic httpd validate-config <cfg>` / `lic httpd
    // explain-config <cfg>` delegate to the Python schema (the sanctioned
    // interim surface, same delegation as scripts/lic-validate-httpd-config.sh
    // and scripts/li-httpd-explain-config.sh) until the Li httpd surface
    // lands. Exit codes propagate so the gate's reject loop works.
    if (argc < 4) {
      return usage();
    }
    const std::string sub = argv[2];
    const std::string cfg = argv[3];
    std::string scripts = "scripts";
    if (const char* root = std::getenv("LI_REPO_ROOT")) {
      scripts = std::string(root) + "/scripts";
    }
    std::string script;
    if (sub == "validate-config") {
      script = scripts + "/validate-httpd-config.py";
    } else if (sub == "explain-config") {
      script = scripts + "/httpd_config.py";
    } else {
      return usage();
    }
    std::string cmdline = "python3 " + shell_quote(script) + " " +
                          shell_quote(cfg) +
                          (sub == "explain-config" ? " --explain" : "");
    return std::system(cmdline.c_str()) == 0 ? 0 : 1;
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
  if (cmd == "ast") {
    // AST parity: canonical int-encoded pre-order dump matching the li
    // bootstrap parser (bootstrap/lic/main.li `ast`). Parse only — no
    // typecheck — so the gate compares pure parse-tree shape.
    if (argc < 3) {
      return usage();
    }
    const std::string source = read_file(argv[2]);
    auto result = li::parse_module(source, argv[2]);
    if (!result.ok() || !result.module) {
      li::print_diagnostics(result.diagnostics);
      return 1;
    }
    std::cout << li::dump_module_ast(*result.module, source);
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
      7,   // BinaryLit -> 7
      8,   // StringLit -> 8
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
      -1,  // KwEcho -> remapped per-token (private=16, public=17, echo=skip)
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
      // Our branch folds KwPrivate/KwPublic into KwEcho (unused enum slot).
      if (t.kind == li::TokenKind::KwEcho) {
        std::string s(t.text);
        if (s == "private") {
          std::cout << 16 << '\t' << s << '\n';
        } else if (s == "public") {
          std::cout << 17 << '\t' << s << '\n';
        }
        // 'echo' has no walker equivalent; skip it.
        continue;
      }
      if (k >= 0 && k < nelem && wk[k] >= 0) {
        int out_kind = wk[k];
        // Our branch folds KwDef into KwProc and treats `for` as a plain
        // Ident; the walker distinguishes both.
        if (t.kind == li::TokenKind::KwProc && std::string(t.text) == "def") {
          out_kind = 10;  // walker KwDef
        }
        if (t.kind == li::TokenKind::Ident && std::string(t.text) == "for") {
          out_kind = 26;  // walker KwFor
        }
        // Our branch folds `prob_ensures` into KwInvariant (walker kw 43);
        // emit the walker's kind so token streams stay byte-identical.
        if (t.kind == li::TokenKind::KwInvariant && std::string(t.text) == "prob_ensures") {
          out_kind = 43;
        }
        // `async`/`await` are plain idents in our branch (walker kw 33/34).
        if (t.kind == li::TokenKind::Ident) {
          const std::string s(t.text);
          if (s == "async") {
            out_kind = 33;
          } else if (s == "await") {
            out_kind = 34;
          }
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
    // Agent CLI flags (JSON diagnostics / workspace driver / check cache)
    // route to the re-landed lic_check_main; plain `lic check <file>` keeps
    // the walker-parity path below so check_li_check_parity is untouched.
    for (int i = 2; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "--format=json" || arg.rfind("--workspace", 0) == 0 ||
          arg.rfind("--cache-dir=", 0) == 0 || arg.rfind("--cache-max-mb=", 0) == 0 ||
          arg == "--no-cache" || arg == "--deny-warnings" ||
          arg.rfind("--jobs=", 0) == 0 || arg.rfind("--max-memory=", 0) == 0) {
        return li::lic_check_main(argc, argv, argv[0]);
      }
    }
    return check_file(argv[2]);
  }
  if (cmd == "diagnose") {
    // Agent-facing JSON diagnostics for one file (same envelope as
    // `lic check --format=json` with command="diagnose").
    return li::lic_diagnose_main(argc, argv);
  }
  if (cmd == "verify") {
    if (argc < 3) {
      return usage();
    }
    bool run_lean = false;
    for (int i = 3; i < argc; ++i) {
      if (std::string_view(argv[i]) == "--lean") {
        run_lean = true;
      }
    }
    return verify_file(argv[2], run_lean);
  }
  if (cmd == "build") {
    if (argc < 3) {
      return usage();
    }
    const char* input = nullptr;
    const char* output = "/dev/null";
    bool release = false;
    std::string extra_flags;
    li::reset_resource_options();
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
      } else if (li::apply_resource_flag(arg, li::resource_options())) {
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
    li::finalize_resource_options(li::resource_options());
    li::note_compile_jobs_reserved(li::resource_options());
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
    // AutoVC emission: every build regenerates build/generated/AutoVC.lean (or
    // <build-dir>/generated when --build-dir= is given) so the Lean discharge
    // tooling (li-tests/tooling/discharge_*_lean.sh and the lake-build CI
    // stage) can typecheck the proof obligations. Restore of the slice dropped
    // in the c132e1a9 squash merge; emission only — the build-gating checks
    // live in the tooling scripts.
    std::string vc_lean;
    if (!li::resource_options().build_dir.empty()) {
      vc_lean = li::resource_options().build_dir + "/generated/AutoVC.lean";
    } else {
      vc_lean = repo_build_path("generated/AutoVC.lean");
    }
    std::error_code fs_err;
    std::filesystem::create_directories(std::filesystem::path(vc_lean).parent_path(), fs_err);
    std::size_t native_closed = 0;
    if (!li::write_vcs_lean(module, vc_lean, &err, &native_closed)) {
      std::cerr << "vc emit: " << err << '\n';
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
    std::string mir_err;
    auto mir = li::lower_to_mir(module, &mir_err);
    if (!mir_err.empty()) {
      std::cerr << "mir: " << mir_err << '\n';
      return 1;
    }
    std::cout << li::dump_mir_module(mir);
    return 0;
  }
  return usage();
}
