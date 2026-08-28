#pragma once

#include "li/ast.hpp"

namespace li {

struct VcSummary {
  std::size_t requires_count = 0;
  std::size_t ensures_count = 0;
  std::size_t prob_ensures_count = 0;
  std::size_t decreases_count = 0;
  std::size_t invariant_count = 0;
  std::size_t proc_count = 0;
  std::size_t mir_fn_count = 0;
  std::size_t ensures_witnessed = 0;
  std::size_t mir_return_linked = 0;
  /// proof-db layer: declared axiom / theorem / lemma propositions.
  std::size_t axiom_count = 0;
  std::size_t theorem_count = 0;
  std::size_t lemma_count = 0;
};

VcSummary summarize_vcs(const Module& module);

}  // namespace li
