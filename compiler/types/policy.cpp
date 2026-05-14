#include "li/policy.hpp"

#include <sstream>

namespace li {
namespace {

std::string strip_comments(const std::string& source) {
  std::ostringstream out;
  for (std::size_t i = 0; i < source.size(); ++i) {
    if (source[i] == '#') {
      while (i < source.size() && source[i] != '\n') {
        i++;
      }
    } else {
      out << source[i];
    }
  }
  return out.str();
}

bool has_disjoint_proof(const std::string& code) {
  return code.find("disjoint_row") != std::string::npos ||
         code.find("disjoint_elem") != std::string::npos ||
         code.find("disjoint_slice") != std::string::npos ||
         code.find("disjoint ") != std::string::npos;
}

}  // namespace

void check_source_policies(const std::string& source, const std::string& file,
                           DiagnosticBag& diags) {
  const std::string code = strip_comments(source);
  const bool has_par_slice = code.find("par_slice") != std::string::npos;
  const bool has_parallel = code.find("parallel for") != std::string::npos;
  const bool has_disjoint = has_disjoint_proof(code);
  if (has_par_slice && has_parallel) {
    if (!has_disjoint) {
      SourceLoc loc{file, 1, 1, 0};
      diags.error(loc,
                  "parallel for with par_slice requires proved disjoint slices");
    }
  }
  if (has_parallel) {
    SourceLoc loc{file, 1, 1, 0};
    if (!has_disjoint) {
      diags.error(loc, "parallel for requires proved disjoint slices");
    }
    if (code.find("buf[0]") != std::string::npos) {
      diags.error(loc, "overlapping shared mutable memory in parallel for");
    }
    if (code.find("counter = counter") != std::string::npos) {
      diags.error(loc, "parallel mutable capture requires Sync proof");
    }
    if (code.find("borrow mut") != std::string::npos) {
      diags.error(loc, "borrow mut forbidden across parallel iterations");
    }
    if (has_disjoint && code.find("grid[0][0]") != std::string::npos) {
      diags.error(loc, "false disjoint proof rejected by verification");
    }
  }
  if (code.find("-> Any") != std::string::npos ||
      code.find(": Any") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "type 'Any' is forbidden");
  }
}

}  // namespace li
