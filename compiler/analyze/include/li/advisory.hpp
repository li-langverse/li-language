#pragma once
#include "li/ast.hpp"
#include "li/check_config.hpp"
#include "li/diagnostics.hpp"
namespace li {
struct AdvisoryOptions { CheckConfig config; };
void run_advisory_passes(const Module& module, const std::string& file_path,
                         const AdvisoryOptions& options, DiagnosticBag& diags);
}
