#include "li/compile.hpp"
#include "li/parser.hpp"
#include "li/policy.hpp"
#include "li/smoke_llvm.hpp"
#include "li/typecheck.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

int usage() {
  std::cerr << "lic — Li compiler\n"
            << "usage:\n"
            << "  lic parse <file>       parse and validate syntax\n"
            << "  lic check <file>       parse + typecheck\n"
            << "  lic build <file> -o <out> [--release]\n"
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
  if (!parsed.module) {
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
  return usage();
}
