// Lennard-Jones MD — optimized cell-list kernel (md_core.h)
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "../common/md_core.h"

static void record_energy(std::ostream& out, int step, const LiMdState& s) {
  double ke = 0.0, pe = 0.0;
  li_md_kinetic(&s, &ke);
  li_md_potential(&s, &pe);
  out << step << ',' << pe << ',' << ke << ',' << (pe + ke) << '\n';
}

static double run_with_trace(std::ostream* trace_out) {
  LiMdRng rng;
  LiMdState state;
  li_md_rng_init(&rng, LI_MD_SEED);
  li_md_init_lattice(&state, &rng);
  li_md_compute_forces(&state);
  if (trace_out) {
    *trace_out << "step,pe,ke,etotal\n";
    record_energy(*trace_out, 0, state);
  }
  double ke = 0.0, pe = 0.0;
  li_md_kinetic(&state, &ke);
  li_md_potential(&state, &pe);
  const double e0 = pe + ke;
  for (int step = 1; step <= LI_MD_STEPS; ++step) {
    li_md_step(&state);
    if (trace_out && (step % LI_MD_TRACE_INTERVAL == 0 || step == LI_MD_STEPS)) {
      record_energy(*trace_out, step, state);
    }
  }
  li_md_kinetic(&state, &ke);
  li_md_potential(&state, &pe);
  const double e1 = pe + ke;
  const double denom = (e0 >= e1 ? e0 : e1);
  const double d = denom > 1e-12 ? denom : 1e-12;
  const double diff = e1 - e0;
  return (diff >= 0.0 ? diff : -diff) / d;
}

int main(int argc, char** argv) {
  std::string trace_path;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--trace" && i + 1 < argc) {
      trace_path = argv[i + 1];
    }
  }
  std::unique_ptr<std::ofstream> trace_file;
  std::ostream* trace_out = nullptr;
  if (!trace_path.empty()) {
    trace_file = std::make_unique<std::ofstream>(trace_path);
    trace_out = trace_file.get();
  }
  const double drift = run_with_trace(trace_out);
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--verify") {
      std::cout.setf(std::ios::scientific);
      std::cout.precision(8);
      std::cout << drift << '\n';
      break;
    }
  }
  return 0;
}
