#include "md_core.h"

#include <stdio.h>
#include <stdlib.h>

static double g_li_md_checksum;

double li_md_temperature_from_env(void) {
  const char* s = getenv("LI_MD_TEMP");
  if (s != NULL && s[0] != '\0') {
    return atof(s);
  }
  return LI_MD_TEMP;
}

static void record_energy(FILE* out, int step, const LiMdState* s) {
  double ke = 0.0, pe = 0.0;
  li_md_kinetic(s, &ke);
  li_md_potential(s, &pe);
  fprintf(out, "%d,%.17g,%.17g,%.17g\n", step, pe, ke, pe + ke);
}

static double md_run_impl(FILE* trace_out) {
  LiMdRng rng;
  LiMdState state;
  li_md_rng_init(&rng, LI_MD_SEED);
  li_md_init_lattice(&state, &rng);
  li_md_compute_forces(&state);
  if (trace_out) {
    fprintf(trace_out, "step,pe,ke,etotal\n");
    record_energy(trace_out, 0, &state);
  }
  double ke = 0.0, pe = 0.0;
  li_md_kinetic(&state, &ke);
  li_md_potential(&state, &pe);
  const double e0 = pe + ke;
  for (int step = 1; step <= LI_MD_STEPS; ++step) {
    li_md_step(&state);
    if (trace_out && (step % LI_MD_TRACE_INTERVAL == 0 || step == LI_MD_STEPS)) {
      record_energy(trace_out, step, &state);
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

static double li_md_run(void) { return md_run_impl(NULL); }

__attribute__((noinline)) void li_md_kernel(void) {
  g_li_md_checksum = li_md_run();
}

double li_md_checksum(void) { return g_li_md_checksum; }

double li_md_run_trace(const char* path) {
  FILE* f = fopen(path, "w");
  if (!f) {
    return -1.0;
  }
  const double drift = md_run_impl(f);
  fclose(f);
  g_li_md_checksum = drift;
  return drift;
}

void li_md_lj_run(void) { li_md_kernel(); }

static void write_traj_frame(FILE* f, int step, const LiMdState* s) {
  fprintf(f, "F %d\n", step);
  for (int i = 0; i < LI_MD_N; ++i) {
    fprintf(f, "%.17g %.17g %.17g %.17g %.17g %.17g\n", s->px[i], s->py[i], s->pz[i], s->vx[i],
            s->vy[i], s->vz[i]);
  }
}

int li_md_export_trajectory(const char* path, int stride, int max_steps) {
  if (path == NULL || path[0] == '\0') {
    return -1;
  }
  if (stride < 1) {
    stride = LI_MD_TRACE_INTERVAL;
  }
  if (max_steps <= 0 || max_steps > LI_MD_STEPS) {
    max_steps = LI_MD_STEPS;
  }

  FILE* f = fopen(path, "w");
  if (!f) {
    return -1;
  }

  fprintf(f, "li_md_traj_v1\n");
  fprintf(f, "n %d\n", LI_MD_N);
  fprintf(f, "box %.17g\n", LI_MD_BOX);
  fprintf(f, "dt %.17g\n", LI_MD_DT);
  fprintf(f, "rho %.17g\n", LI_MD_RHO);
  const double temp = li_md_temperature_from_env();
  fprintf(f, "temp %.17g\n", temp);
  fprintf(f, "init fcc\n");
  fprintf(f, "ncell %d\n", li_md_fcc_ncell());
  fprintf(f, "stride %d\n", stride);
  fprintf(f, "max_steps %d\n", max_steps);
  fprintf(f, "---\n");

  LiMdRng rng;
  LiMdState state;
  li_md_rng_init(&rng, LI_MD_SEED);
  li_md_init_fcc(&state, &rng, temp);
  li_md_compute_forces(&state);
  write_traj_frame(f, 0, &state);

  for (int step = 1; step <= max_steps; ++step) {
    li_md_step(&state);
    if (step % stride == 0 || step == max_steps) {
      write_traj_frame(f, step, &state);
    }
  }

  fclose(f);
  return 0;
}
