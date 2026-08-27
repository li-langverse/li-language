#include "li/mir.hpp"

namespace li {

const char* mir_version() { return "0.1.0"; }

std::size_t count_mir_vectorized_proc(const MirModule& mir) {
  std::size_t n = 0;
  for (const auto& fn : mir.functions) {
    for (const auto& d : fn.decorators) {
      if (d.vectorized) { ++n; break; }
    }
  }
  return n;

}

std::size_t count_mir_parallel_disjoint_proven(const MirModule& mir) {
  std::size_t n = 0;
  for (const auto& fn : mir.functions) {
    for (const auto& d : fn.decorators) {
      if (d.parallel && d.disjoint_proven) { ++n; }
    }
  }
  return n;
}

}  // namespace li
