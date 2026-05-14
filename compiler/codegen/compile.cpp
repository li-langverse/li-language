#include "li/compile.hpp"
#include "li/emit.hpp"
#include "li/mir.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <unistd.h>

namespace li {

bool compile_module(const Module& module, const std::string& output_path, bool release,
                  const std::string& extra_clang_flags, std::string* error) {
  const MirModule mir = lower_to_mir(module);
  const std::string ll_path =
      (std::filesystem::temp_directory_path() /
       ("li_build_" + std::to_string(getpid()) + ".ll"))
          .string();

  if (!emit_llvm_ir(mir, ll_path, error)) {
    return false;
  }

  if (output_path == "/dev/null") {
    std::filesystem::remove(ll_path);
    return true;
  }

  std::filesystem::path rt_path = std::filesystem::path("runtime") / "li_rt.c";
  if (!std::filesystem::exists(rt_path)) {
    rt_path = std::filesystem::path("..") / "runtime" / "li_rt.c";
  }

  std::ostringstream cmd;
  const char* cc_env = std::getenv("CC");
  const char* cc = (cc_env && *cc_env) ? cc_env : "clang";
  cmd << cc << " -Wno-override-module -x ir \"" << ll_path << "\" -x c \""
      << rt_path.string() << "\" -o \"" << output_path << "\"";
  if (release) {
    cmd << " -O2";
  }
  if (!extra_clang_flags.empty()) {
    cmd << " " << extra_clang_flags;
  }
  if (const char* extra_c = std::getenv("LI_EXTRA_C")) {
    std::string paths(extra_c);
    std::size_t start = 0;
    while (start < paths.size()) {
      const std::size_t end = paths.find(' ', start);
      const std::string path = paths.substr(start, end == std::string::npos ? std::string::npos
                                                                            : end - start);
      if (!path.empty()) {
        cmd << " -x c \"" << path << "\"";
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
    cmd << " -lm";
  }
  const int rc = std::system(cmd.str().c_str());
  std::filesystem::remove(ll_path);
  if (rc != 0) {
    if (error) {
      *error = "clang link failed";
    }
    return false;
  }
  return true;
}

}  // namespace li
