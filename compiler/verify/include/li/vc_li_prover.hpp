#pragma once

#include "li/ast.hpp"

#include <string>
#include <vector>

namespace li {

/// Serialize a theorem proposition as a comma-separated int list in the
/// pre-order node encoding understood by bootstrap/prover/main.li.
std::string serialize_theorem_prop(const TheoremDecl& thm);

/// Build (self-bootstrap) and run the li prover binary on the serialized
/// propositions. Returns one verdict per input (1 = proved, 0 = open) in
/// input order; an empty vector on any failure (missing binary, run error,
/// output mismatch) so callers fall back to the C++ engine.
std::vector<int> run_li_prover(const std::vector<std::string>& serialized,
                               const char* lic_exe);

/// Layer 3 discharge: C++ engine first, then the li prover for the rest.
struct LiProverResult {
  std::size_t li_proved = 0;       // closed by the li prover (not by C++)
  std::size_t open_count = 0;      // open in both engines
  std::vector<std::string> open_names;
  std::vector<std::string> li_proved_names;
};

LiProverResult discharge_with_li_prover(const Module& module, const char* lic_exe);

}  // namespace li
