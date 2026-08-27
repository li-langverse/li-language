#!/usr/bin/env python3
"""One-shot bootstrap for WP1 advisory files (feat/lic-check-advisory)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FILES = {
    "compiler/config/include/li/check_config.hpp": """#pragma once
#include <filesystem>
namespace li {
enum class CheckRuleLevel { Allow, Warn, Deny };
struct CheckConfig { CheckRuleLevel typosquat = CheckRuleLevel::Warn; };
CheckConfig load_check_config(const std::filesystem::path& file_path);
}
""",
    "compiler/config/CMakeLists.txt": """add_library(li_config STATIC check_config.cpp)
target_include_directories(li_config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(li_config PUBLIC li_diagnostics)
""",
    "compiler/analyze/include/li/advisory.hpp": """#pragma once
#include "li/ast.hpp"
#include "li/check_config.hpp"
#include "li/diagnostics.hpp"
namespace li {
struct AdvisoryOptions { CheckConfig config; };
void run_advisory_passes(const Module& module, const std::string& file_path,
                         const AdvisoryOptions& options, DiagnosticBag& diags);
}
""",
    "compiler/analyze/CMakeLists.txt": """add_library(li_analyze STATIC advisory.cpp)
target_include_directories(li_analyze PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(li_analyze PUBLIC li_ast li_config li_diagnostics)
""",
    "compiler/lic/include/li/check_cmd.hpp": """#pragma once
#include "li/ast.hpp"
#include "li/diagnostics.hpp"
namespace li {
struct FrontendCheckOptions { bool deny_warnings = false; };
bool run_frontend_check(const char* path, const FrontendCheckOptions& options,
                        DiagnosticBag& diags, Module* out_module = nullptr);
int lic_check_main(int argc, char** argv);
int lic_diagnose_main(int argc, char** argv);
}
""",
    "compiler/lic/CMakeLists.txt": """add_executable(lic main.cpp check_cmd.cpp)
target_compile_definitions(lic PRIVATE LI_VERSION="${LI_VERSION_STRING}")
target_include_directories(lic PRIVATE "${CMAKE_SOURCE_DIR}/runtime"
                                           "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(lic PRIVATE li_analyze li_codegen li_config li_diagnostics li_parser
                                   li_types li_verify li_mir li_rt)
""",
    "compiler/CMakeLists.txt": """add_subdirectory(config)
add_subdirectory(analyze)
add_subdirectory(diagnostics)
add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(ast)
add_subdirectory(types)
add_subdirectory(mir)
add_subdirectory(codegen)
add_subdirectory(verify)
add_subdirectory(lic)
if(LI_BUILD_FUZZ)
  add_subdirectory(fuzz)
endif()
""",
}

# Large sources loaded from sibling files if present; else embedded below in main
def main():
    for rel, text in FILES.items():
        p = ROOT / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    print("bootstrap: wrote", len(FILES), "files under", ROOT)

if __name__ == "__main__":
    main()
