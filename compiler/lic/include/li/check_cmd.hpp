#pragma once

#include "li/ast.hpp"
#include "li/check_cache.hpp"
#include "li/diagnostics.hpp"

#include <string>

namespace li {

struct FrontendCheckOptions {
  bool deny_warnings = false;
};

struct CheckCommandOptions {
  bool deny_warnings = false;
  bool json_output = false;
  bool workspace = false;
  CheckCacheOptions cache;
};

bool run_frontend_check(const char* path, const std::string& source, Module& out,
                        DiagnosticBag& diags,
                        const FrontendCheckOptions& options = FrontendCheckOptions{});

int lic_check_main(int argc, char** argv, const char* lic_executable = "lic");
int lic_diagnose_main(int argc, char** argv);

}  // namespace li
