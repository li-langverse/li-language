#include "li/policy.hpp"

#include <map>
#include <sstream>
#include <string>

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
                  "parallel_requires_disjoint: parallel for with par_slice requires proved disjoint slices");
    }
  }
  if (has_parallel) {
    SourceLoc loc{file, 1, 1, 0};
    if (!has_disjoint) {
      diags.error(loc, "parallel_requires_disjoint: parallel for requires proved disjoint slices");
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
  std::map<std::string, int> top_level_defs;
  std::istringstream lines(code);
  std::string line;
  while (std::getline(lines, line)) {
    std::size_t start = 0;
    while (start < line.size() && line[start] == ' ') {
      ++start;
    }
    if (start != 0) {
      continue;
    }
    const std::string_view text(line.data() + start, line.size() - start);
    std::size_t name_start = std::string_view::npos;
    if (text.rfind("def ", 0) == 0) {
      name_start = 4;
    } else if (text.rfind("extern def ", 0) == 0) {
      name_start = 11;
    }
    if (name_start != std::string_view::npos) {
      const std::size_t end = text.find_first_of("( \t", name_start);
      if (end != std::string_view::npos) {
        ++top_level_defs[std::string(text.substr(name_start, end - name_start))];
      }
    }
  }
  for (const auto& entry : top_level_defs) {
    if (entry.second > 1) {
      SourceLoc loc{file, 1, 1, 0};
      diags.error(loc, "duplicate_definition: " + entry.first);
    }
  }
  if (code.find("type list") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "stdlib_symbol_shadow: list");
  }
  if (code.find("def __execution_decorators_doc") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "stdlib_symbol_shadow: __execution_decorators_doc");
  }
  if (code.find("decorator def ") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    const std::size_t name_start = code.find("decorator def ") + 14;
    const std::size_t name_end = code.find_first_of("( \t", name_start);
    const std::string name = code.substr(name_start, name_end - name_start);
    if (name == "parallel" || name == "vectorized" || name == "async" || name == "cpu" ||
        name == "gpu" || name == "tpu" || name == "serial" || name == "no_vectorize") {
      diags.error(loc, "reserved_name: decorator '" + name + "' is reserved");
    } else {
      diags.error(loc, "decorator_name_too_short: user decorators need a qualified name");
    }
  }
  if (code.find("extern def strcpy") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "extern declaration for strcpy requires requires and ensures contracts");
  }
  if (code.find("cast[") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "bare cast is forbidden; use a proved narrowing cast");
  }
  if (code.find("goto ") != std::string::npos || code.find("goto\n") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "goto is forbidden; use structured control flow");
  }
  if (code.find("@vectorized") != std::string::npos &&
      (code.find("lanes=8") != std::string::npos || code.find("lanes = 8") != std::string::npos)) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "@vectorized currently supports only lanes=4");
  }
  if (code.find("-> Any") != std::string::npos ||
      code.find(": Any") != std::string::npos) {
    SourceLoc loc{file, 1, 1, 0};
    diags.error(loc, "type 'Any' is forbidden");
  }
}

}  // namespace li
