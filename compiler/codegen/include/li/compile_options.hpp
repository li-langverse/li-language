#pragma once

namespace li {

struct CompileOptions {
  bool release = false;
  /** Enable cancellation-safe MIR rewrites and compensated reductions. */
  bool fp_numerically_stable = false;
  int runtime_threads = 0;
};

}  // namespace li
