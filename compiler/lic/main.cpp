#include "li/ast_dump.hpp"
#include "li/check_cmd.hpp"
#include "li/compile.hpp"
#include "li/diagnostics.hpp"
#include "li/lexer.hpp"
#include "li/parser.hpp"
#include "li/platform.hpp"
#include "li/prelude.hpp"
#include "li/smoke_llvm.hpp"
#include "li/vc_emit.hpp"
#include "li/mir.hpp"
#include "li/mir_dump.hpp"
#include "li/vc_summary.hpp"
#include "li/vc_prove.hpp"
#include "li/vc_li_prover.hpp"
#include "li/vc_witness.hpp"
#include "li/terminal.hpp"
#include "li/error_codes.hpp"
#include "li/proof_cli.hpp"
#include "li/resource_options.hpp"

#include "li_rt.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

int usage() {
  const unsigned jobs = li::default_host_jobs();
  li::print_lic_banner(std::cerr);
  std::cerr << li::styled_accent("lic") << li::styled_dim(" — prove · write · run fast") << li::reset_style()
            << "\nusage:\n"
            << "  lic parse <file>       parse and validate syntax\n"
            << "  lic ast <file>         canonical int-encoded AST dump (self-host parity)\n"
            << "  lic mir <file>         canonical MIR dump of lower_to_mir() (self-host parity)\n"
            << "  lic check <file>|--workspace [li.toml] [--format=json] [--deny-warnings]\n"
            << "                       [--cache-dir=DIR] [--cache-max-mb=N] [--no-cache] [--jobs=N]\n"
            << "  lic diagnose <file>    agent-oriented JSON diagnostics\n"
            << "  lic verify <file>      VC summary; --lean lake; --strict-lean fails open VCs\n"
            << "                       [--allow-open-vc] [--no-lean-verify]\n"
            << "  lic build <file> -o <out> [--release] [--numerically-stable]\n"
            << "                       [--strict-lean]  fail on open AutoVC goals + lake strict check\n"
            << "                       [--allow-open-vc]  allow obligations without Lean proof (dev only)\n"
            << "                       [--prob-check]     Monte Carlo discharge for prob_ensures P(event)<ε\n"
            << "                       [--no-lean-verify] skip lake / AutoVC typecheck\n"
            << "                       default: fail on open goals; lake typecheck when installed\n"
            << "                       [--threads=N] [--jobs=N] [--max-memory=MB]\n"
            << "                       [--coverage-instrument]\n"
            << "  lic smoke-llvm         verify LLVM can emit main returning 0\n"
            << "  lic httpd explain-config <file.toml>  desugar [routes] to canonical form\n"
            << "  lic httpd validate-config <file.toml>  validate [routes] (E0501–E0504)\n"
            << "  lic validate-httpd-config <file.toml>  M1 TOML schema + overlap (Python)\n"
            << "  lic --version          print version\n"
            << "\n"
            << "resource flags (preferred; LI_* env deprecated):\n"
            << "  --jobs=N — parallel compile workers (default 1; host=" << jobs << ")\n"
            << "  --max-memory=MB --job-memory-mb=N — cap effective --jobs\n"
            << "  --build-dir=PATH — isolated build tree (AutoVC, temps)\n"
            << "  --threads=N — runtime parallel team size\n"
            << "  --numerically-stable / LI_FP_NUMERICALLY_STABLE=1 — cancellation-safe FP\n";
  return 1;
}

std::filesystem::path resolve_httpd_config_script() {
  if (const char* root = std::getenv("LI_REPO_ROOT")) {
    const std::filesystem::path p =
        std::filesystem::path(root) / "scripts" / "httpd_config.py";
    if (std::filesystem::exists(p)) {
      return p;
    }
  }
  const std::filesystem::path candidates[] = {
      std::filesystem::path("scripts/httpd_config.py"),
      std::filesystem::path("../scripts/httpd_config.py"),
  };
  for (const auto& c : candidates) {
    if (std::filesystem::exists(c)) {
      return std::filesystem::absolute(c);
    }
  }
  return {};
}

static li::ErrorCode httpd_error_kind_to_code(int32_t kind) {
  switch (kind) {
    case 1:
      return li::ErrorCode::E0501;
    case 2:
      return li::ErrorCode::E0502;
    case 3:
      return li::ErrorCode::E0503;
    case 4:
      return li::ErrorCode::E0504;
    default:
      return li::ErrorCode::E0502;
  }
}

int httpd_validate_config(int argc, char** argv) {
  if (argc < 4 || std::string_view(argv[2]) != "validate-config") {
    std::cerr << "usage: lic httpd validate-config <config.toml>\n";
    return 1;
  }
  const char* path = argv[3];
  if (li_rt_httpd_load_config(path) != 0) {
    const int32_t kind = li_rt_httpd_last_error_kind();
    const char* msg = li_rt_httpd_last_error_message();
    const auto code = httpd_error_kind_to_code(kind);
    const auto fd = li::format_diagnostic(code, msg ? msg : "config error",
                                        "fix [routes] keys or paths; see docs/ecosystem/httpd-prerequisites.md");
    std::cerr << path << ":1:1: " << li::styled_error_label() << " [" << fd.code << "]: " << fd.message;
    if (!fd.hint.empty()) {
      std::cerr << "\n  hint: " << fd.hint;
    }
    std::cerr << '\n';
    return 1;
  }
  std::cout << "OK: " << li_rt_httpd_route_count() << " routes\n";
  return 0;
}

int validate_httpd_config_cmd(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: lic validate-httpd-config <config.toml>\n";
    return 1;
  }
  std::filesystem::path repo = std::filesystem::current_path();
  if (const char* root = std::getenv("LI_REPO_ROOT")) {
    repo = root;
  }
  const std::filesystem::path script = repo / "scripts/validate-httpd-config.py";
  if (!std::filesystem::is_regular_file(script)) {
    std::cerr << "validate-httpd-config: missing " << script << " (set LI_REPO_ROOT)\n";
    return 1;
  }
  std::ostringstream cmd;
  cmd << "python3 " << script.string() << " " << argv[2];
  const int st = std::system(cmd.str().c_str());
  return st != 0 ? 1 : 0;
}

int httpd_explain_config(int argc, char** argv) {
  if (argc < 4 || std::string_view(argv[2]) != "explain-config") {
    std::cerr << "usage: lic httpd explain-config <config.toml>\n";
    return 1;
  }
  const std::filesystem::path script = resolve_httpd_config_script();
  if (script.empty()) {
    std::cerr << "lic: cannot find scripts/httpd_config.py (set LI_REPO_ROOT)\n";
    return 1;
  }
  std::ostringstream cmd;
  cmd << "python3 \"" << script.string() << "\" \"" << argv[3] << "\" --explain";
  const int rc = std::system(cmd.str().c_str());
  if (rc != 0) {
    return 1;
  }
  return 0;
}

void apply_resource_options_to_env() {
  const auto& opts = li::resource_options();
  if (!opts.build_dir.empty()) {
    setenv("LI_BUILD_DIR", opts.build_dir.c_str(), 1);
  }
  if (opts.threads > 0) {
    setenv("LI_OMP_THREADS", std::to_string(opts.threads).c_str(), 1);
  }
  if (opts.max_memory_mb > 0) {
    setenv("LI_MAX_MEMORY_MB", std::to_string(opts.max_memory_mb).c_str(), 1);
  }
}

std::string read_file(const char* path) {
  std::ifstream in(path);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

bool frontend(const char* path, const std::string& source, li::Module& out,
              li::DiagnosticBag& diags) {
  return li::run_frontend_check(path, source, out, diags);
}

// A theorem/lemma is discharged when EITHER engine closes it: the C++ native
// engine (rewriting + linear integer arithmetic) or the self-hosted li prover
// (the rewriting half, bootstrap/prover/main.li). It stays open only when both
// leave it open — the li prover never runs LIA, so LIA-closed theorems must
// not fail the build gate just because the li prover alone cannot close them.
std::vector<std::string> open_in_both(const li::TheoremDischargeResult& cpp,
                                      const li::LiProverResult& li) {
  std::vector<std::string> out;
  for (const auto& name : cpp.open_names) {
    if (std::find(li.open_names.begin(), li.open_names.end(), name) !=
        li.open_names.end()) {
      out.push_back(name);
    }
  }
  return out;
}

void warn_deprecated_proof_env() {
  if (std::getenv("LI_ALLOW_OPEN_VC") != nullptr) {
    std::cerr << "lic: warning — LI_ALLOW_OPEN_VC is ignored; use --allow-open-vc on the command line\n";
  }
  if (const char* v = std::getenv("LI_BUILD_VERIFY_LEAN"); v != nullptr && v[0] == '0') {
    std::cerr << "lic: warning — LI_BUILD_VERIFY_LEAN=0 is ignored; use --no-lean-verify\n";
  }
  if (std::getenv("LI_BUILD_VERIFY_LEAN_STRICT") != nullptr) {
    std::cerr << "lic: warning — LI_BUILD_VERIFY_LEAN_STRICT is ignored; use --strict-lean\n";
  }
}

bool parse_proof_cli_flag(std::string_view arg) {
  if (arg == "--allow-open-vc") {
    li::proof_cli_flags().allow_open_vc = true;
    return true;
  }
  if (arg == "--no-lean-verify") {
    li::proof_cli_flags().no_lean_verify = true;
    return true;
  }
  if (arg == "--prob-check") {
    li::proof_cli_flags().prob_check = true;
    return true;
  }
  return false;
}

int run_prob_check_script(const char* input_path) {
  const char* root = std::getenv("LI_REPO_ROOT");
  std::string script = "scripts/prob_check.py";
  if (root != nullptr) {
    script = std::string(root) + "/" + script;
  }
  if (!std::filesystem::exists(script)) {
    std::cerr << "lic build: --prob-check requires " << script << "\n";
    return 1;
  }
  const std::string cmd =
      "python3 " + script + " \"" + std::string(input_path) + "\"";
  const int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::cerr << "lic build: probabilistic contract check failed (prob_ensures)\n";
    return 1;
  }
  return 0;
}

bool lake_available() {
  return std::system("command -v lake >/dev/null 2>&1") == 0;
}

int run_lean_verify_script(bool check_open_goals) {
  const char* root = std::getenv("LI_REPO_ROOT");
  std::string script = "scripts/lean-verify-stub.sh";
  if (root != nullptr) {
    script = std::string(root) + "/" + script;
  }
  if (!lake_available()) {
    std::cerr << "lic build: warning — Lean 4 / lake not installed; skipping semantics proof "
                 "(install elan for full 2f gate)\n";
    return 0;
  }
  std::string cmd = "bash " + script;
  if (check_open_goals) {
    cmd += " --check-open-goals";
  }
  const int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::cerr << "lic build: Lean semantics verification failed (see docs/semantics)\n";
    return 1;
  }
  return 0;
}

int count_open_autovc_goals() {
  const char* root = std::getenv("LI_REPO_ROOT");
  std::string script = "scripts/check-autovc-open-goals.sh";
  if (root != nullptr) {
    script = std::string(root) + "/" + script;
  }
  const std::string autovc = li::repo_build_path("generated/AutoVC.lean");
  const std::string cmd = "bash \"" + script + "\" \"" + autovc + "\" 2>&1";
  FILE* pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return -1;
  }
  char buf[256];
  std::string out;
  while (fgets(buf, sizeof(buf), pipe) != nullptr) {
    out += buf;
  }
  pclose(pipe);
  if (out.find("open obligation") != std::string::npos) {
    std::size_t i = 0;
    int count = 0;
    while ((i = out.find("open VC:", i)) != std::string::npos) {
      ++count;
      ++i;
    }
    return count;
  }
  return 0;
}

int verify_file(const char* path, bool run_lean, bool strict_lean, const char* lic_exe) {
  const std::string source = read_file(path);
  li::Module module;
  li::DiagnosticBag diags;
  if (!frontend(path, source, module, diags)) {
    li::print_diagnostics(diags);
    return 1;
  }
  const li::MirModule mir = li::lower_to_mir(module);
  li::VcSummary vc = li::summarize_vcs(module);
  vc.mir_fn_count = mir.functions.size();
  const li::VcWitnessStats witness = li::compute_vc_witness_stats(module, &mir);
  vc.ensures_witnessed = witness.ensures_witnessed;
  vc.mir_return_linked = witness.mir_return_linked;
  const std::size_t mir_parallel_disjoint = li::count_mir_parallel_disjoint_proven(mir);
  const std::size_t mir_vectorized_proc = li::count_mir_vectorized_proc(mir);
  const li::TheoremDischargeResult theorem_result = li::discharge_theorems_natively(module);
  const li::LiProverResult li_result = li::discharge_with_li_prover(module, lic_exe);
  const std::vector<std::string> open_both = open_in_both(theorem_result, li_result);
  std::cout << "verify: procs=" << vc.proc_count << " mir_fns=" << vc.mir_fn_count
            << " requires=" << vc.requires_count << " ensures=" << vc.ensures_count
            << " prob_ensures=" << vc.prob_ensures_count
            << " decreases=" << vc.decreases_count << " invariant=" << vc.invariant_count
            << " witnessed_ensures=" << vc.ensures_witnessed
            << " mir_return_linked=" << vc.mir_return_linked
            << " mir_parallel_disjoint=" << mir_parallel_disjoint
            << " mir_vectorized_proc=" << mir_vectorized_proc
            << " axioms=" << vc.axiom_count << " theorems=" << vc.theorem_count
            << " lemmas=" << vc.lemma_count
            << " theorems_proved=" << theorem_result.proved_count
            << " theorems_li_proved=" << li_result.li_proved
            << " theorems_open=" << open_both.size() << '\n';
  if (!open_both.empty()) {
    std::cerr << li::styled_warning("verify") << li::styled_dim(" — native discharge left open:")
              << li::reset_style();
    for (const auto& name : open_both) {
      std::cerr << ' ' << name;
    }
    std::cerr << '\n';
  }
  if (li_result.li_proved > 0) {
    std::cerr << li::styled_success("li-prover") << li::styled_dim(" closed ")
              << li::reset_style() << li_result.li_proved << " proposition(s) in li:";
    for (const auto& name : li_result.li_proved_names) {
      std::cerr << ' ' << name;
    }
    std::cerr << '\n';
  }
  if (li::terminal_color_enabled()) {
    std::cout << li::styled_success("verify") << li::styled_dim(" telemetry") << li::reset_style()
              << '\n';
    li::print_verify_telemetry(std::cout, "procs", std::to_string(vc.proc_count));
    li::print_verify_telemetry(std::cout, "mir_fns", std::to_string(vc.mir_fn_count));
    li::print_verify_telemetry(std::cout, "requires", std::to_string(vc.requires_count));
    li::print_verify_telemetry(std::cout, "ensures", std::to_string(vc.ensures_count));
    li::print_verify_telemetry(std::cout, "witnessed_ensures",
                               std::to_string(vc.ensures_witnessed));
    li::print_verify_telemetry(std::cout, "mir_return_linked",
                               std::to_string(vc.mir_return_linked));
    li::print_verify_telemetry(std::cout, "mir_vectorized_proc",
                               std::to_string(mir_vectorized_proc));
    if (vc.axiom_count > 0 || vc.theorem_count > 0 || vc.lemma_count > 0) {
      li::print_verify_telemetry(std::cout, "axioms", std::to_string(vc.axiom_count));
      li::print_verify_telemetry(std::cout, "theorems", std::to_string(vc.theorem_count));
      li::print_verify_telemetry(std::cout, "lemmas", std::to_string(vc.lemma_count));
    }
  }
  if (vc.requires_count == 0 && vc.ensures_count == 0) {
    std::cerr << li::styled_warning("verify") << li::styled_dim(" — no procedure contracts (G-vc partial)")
              << li::reset_style() << '\n';
  }
  if (std::getenv("LI_EMIT_VCS") != nullptr) {
    std::string vc_path = li::repo_build_path("vcs.json");
    std::string vc_err;
    if (!li::write_vcs_json(module, vc_path, &vc_err)) {
      std::cerr << "verify: " << vc_err << '\n';
    } else {
      std::cout << "verify: wrote " << vc_path << '\n';
    }
  }
  if (run_lean) {
    const std::string vc_lean = li::repo_build_path("generated/AutoVC.lean");
    std::error_code fs_err;
    std::filesystem::create_directories(std::filesystem::path(vc_lean).parent_path(), fs_err);
    std::string vc_err;
    std::size_t native_closed = 0;
    (void)li::write_vcs_lean(module, vc_lean, &vc_err, &native_closed);
    std::cout << "verify: vcs_natively_closed=" << native_closed << '\n';
    const int open = count_open_autovc_goals();
    if (open >= 0) {
      std::cout << "verify: open_vc_goals=" << open << '\n';
      if (strict_lean && open > 0 && !li::allow_open_vc()) {
        std::cerr << "verify: strict-lean failed (" << open << " open obligations)\n";
        return 1;
      }
    }
    if (!li::no_lean_verify()) {
      return run_lean_verify_script(strict_lean);
    }
  }
  return 0;
}

int build_file(const char* path, const char* output, const li::CompileOptions& opts) {
  const std::string source = read_file(path);
  li::Module module;
  li::DiagnosticBag diags;
  if (!frontend(path, source, module, diags)) {
    li::print_diagnostics(diags);
    return 1;
  }
  std::string err;
  if (!li::compile_module(module, output, opts, "", &err)) {
    std::cerr << li::styled_error_label() << ": build failed: " << err << li::reset_style() << '\n';
    return 1;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  li::reset_resource_options();
  if (argc < 2) {
    return usage();
  }
  const std::string_view cmd = argv[1];
  if (cmd == "--version" || cmd == "-V") {
    li::print_lic_banner(std::cout);
#ifdef LI_VERSION
    std::cout << li::styled_accent("lic ") << LI_VERSION << li::reset_style() << '\n';
#else
    std::cout << li::styled_accent("lic 0.0.0-dev") << li::reset_style() << '\n';
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
  if (cmd == "ast") {
    // Self-host parity: canonical pre-order AST dump, one node per line.
    // The li bootstrap parser (bootstrap/lic/main.li `ast`) emits the same
    // stream; scripts/check_li_ast_parity.sh diffs the two.
    if (argc < 3) {
      return usage();
    }
    const std::string source = read_file(argv[2]);
    auto result = li::parse_module(source, argv[2]);
    if (!result.ok()) {
      li::print_diagnostics(result.diagnostics);
      return 1;
    }
    std::cout << li::dump_module_ast(*result.module, source);
    return 0;
  }
  if (cmd == "mir") {
    // Layer 5 self-host parity: canonical MIR dump of lower_to_mir() output.
    // The li bootstrap backend (bootstrap/lic/main.li `mir <file>`) emits the
    // same stream; scripts/check_li_mir_parity.sh diffs the two.
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
    std::cout << li::dump_mir_module(li::lower_to_mir(module));
    return 0;
  }
  if (cmd == "lex") {
    // Dev/parity command: dump the token stream as `kind<TAB>lexeme`.
    // The self-hosted lexer (bootstrap/lic/main.li) emits the same format so
    // scripts/check_li_lexer_parity.sh can diff the two implementations.
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
    for (const auto& t : lexer.tokens()) {
      std::cout << static_cast<int>(t.kind) << '\t' << std::string(t.text) << '\n';
    }
    return 0;
  }
  if (cmd == "check") {
    return li::lic_check_main(argc, argv, argv[0]);
  }
  if (cmd == "diagnose") {
    return li::lic_diagnose_main(argc, argv);
  }
  if (cmd == "verify") {
    if (argc < 3) {
      return usage();
    }
    li::reset_proof_cli_flags();
    warn_deprecated_proof_env();
    bool run_lean = false;
    bool strict_lean = false;
    for (int i = 3; i < argc; ++i) {
      if (std::string_view(argv[i]) == "--lean") {
        run_lean = true;
      } else if (std::string_view(argv[i]) == "--strict-lean") {
        run_lean = true;
        strict_lean = true;
      } else if (li::apply_resource_flag(argv[i], li::resource_options())) {
        continue;
      } else if (parse_proof_cli_flag(argv[i])) {
        continue;
      }
    }
    li::finalize_resource_options(li::resource_options());
    apply_resource_options_to_env();
    return verify_file(argv[2], run_lean, strict_lean, argv[0]);
  }
  if (cmd == "validate-httpd-config") {
    return validate_httpd_config_cmd(argc, argv);
  }
  if (cmd == "httpd") {
    if (argc >= 3 && std::string_view(argv[2]) == "validate-config") {
      return httpd_validate_config(argc, argv);
    }
    if (argc >= 3 && std::string_view(argv[2]) == "explain-config") {
      return httpd_explain_config(argc, argv);
    }
    std::cerr << "usage: lic httpd explain-config|validate-config <config.toml>\n";
    return 1;
  }
  if (cmd == "build") {
    if (argc < 3) {
      return usage();
    }
    li::reset_proof_cli_flags();
    warn_deprecated_proof_env();
    const char* input = nullptr;
    const char* output = li::null_output_path();
    li::CompileOptions opts;
    bool coverage = false;
    bool strict_lean = false;
    std::string extra_flags;
    if (const char* env_stable = std::getenv("LI_FP_NUMERICALLY_STABLE");
        env_stable && *env_stable && env_stable[0] != '0') {
      opts.fp_numerically_stable = true;
    }
    for (int i = 2; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (arg == "-o" && i + 1 < argc) {
        output = argv[++i];
      } else if (!input && arg[0] != '-' && arg != "--release") {
        input = argv[i];
      } else if (arg == "--release") {
        opts.release = true;
      } else if (arg == "--numerically-stable") {
        opts.fp_numerically_stable = true;
      } else if (arg == "--coverage-instrument") {
        coverage = true;
      } else if (arg == "--strict-lean") {
        strict_lean = true;
      } else if (parse_proof_cli_flag(arg)) {
        continue;
      } else if (li::apply_resource_flag(arg, li::resource_options())) {
        continue;
      } else {
        extra_flags.append(argv[i]);
        extra_flags.push_back(' ');
      }
    }
    li::finalize_resource_options(li::resource_options());
    li::note_compile_jobs_reserved(li::resource_options());
    apply_resource_options_to_env();
    if (li::resource_options().threads > 0) {
      opts.runtime_threads = static_cast<int>(li::resource_options().threads);
    }
    if (coverage) {
      extra_flags += "-fprofile-instr-generate -fcoverage-mapping ";
    }
    if (!input) {
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
    if (!li::compile_module(module, output, opts, extra_flags, &err)) {
      std::cerr << li::styled_error_label() << ": build failed: " << err << li::reset_style() << '\n';
      return 1;
    }
    if (li::terminal_color_enabled()) {
      std::cout << li::styled_success("build") << li::styled_dim(" ok → ") << li::styled_accent(output)
                << li::reset_style() << '\n';
    }
    const std::string vc_lean = li::repo_build_path("generated/AutoVC.lean");
    std::error_code fs_err;
    std::filesystem::create_directories(std::filesystem::path(vc_lean).parent_path(), fs_err);
    std::size_t native_closed = 0;
    if (!li::write_vcs_lean(module, vc_lean, &err, &native_closed)) {
      std::cerr << "vc emit: " << err << '\n';
    }
    const li::TheoremDischargeResult theorem_result = li::discharge_theorems_natively(module);
    const li::LiProverResult li_result = li::discharge_with_li_prover(module, argv[0]);
    const std::vector<std::string> open_both = open_in_both(theorem_result, li_result);
    if (!li::allow_open_vc() && !open_both.empty()) {
      std::cerr << "lic build: " << open_both.size()
                << " theorem/lemma proposition(s) not discharged natively:";
      for (const auto& name : open_both) {
        std::cerr << ' ' << name;
      }
      std::cerr << "\nhint: lic's native proof engine (rewriting in li + integer arithmetic) "
                   "could not close these — simplify the proposition, strengthen it into an "
                   "axiom, or pass --allow-open-vc only for documented dev/tests\n";
      return 1;
    }
    if (!li::allow_open_vc()) {
      const int open = count_open_autovc_goals();
      if (open > 0) {
        std::cerr << "lic build: " << open
                  << " proof obligation(s) still need a Lean proof "
                     "(see " << vc_lean << ")\n";
        std::cerr << "hint: a `requires` or `ensures` on your code created a goal the compiler "
                     "could not close automatically — prove it in Lean, simplify the "
                     "contract, or pass --allow-open-vc only for documented dev/tests\n";
        return 1;
      }
    }
    if (!li::no_lean_verify()) {
      if (const int lean_rc = run_lean_verify_script(strict_lean); lean_rc != 0) {
        return lean_rc;
      }
    } else {
      std::cerr << "lic build: warning — Lean verify skipped (--no-lean-verify)\n";
    }
    if (li::prob_check()) {
      if (const int prob_rc = run_prob_check_script(input); prob_rc != 0) {
        return prob_rc;
      }
    }
    return 0;
  }
  return usage();
}
