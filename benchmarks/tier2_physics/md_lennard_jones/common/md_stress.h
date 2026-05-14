#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Stress-test metrics (see benchmarks/harness/stability.py). */
typedef struct LiMdStressResult {
  const char* name;
  double value;
  double threshold;
  int passed;
} LiMdStressResult;

/* Harmonic oscillator VV: max |E(t)-E(0)|/E(0); pass if < dt^2/2. */
LiMdStressResult li_md_stress_harmonic(double dt, int steps);

/* NVE liquid at rho=0.75, T=1: MSD of (E/N) about its mean; pass if < 3e-8 at dt=0.004. */
LiMdStressResult li_md_stress_nve_energy_msd(double dt, int steps);

/* Timestep halving: MSD(dt/2)/MSD(dt); pass if in [12, 20]. */
LiMdStressResult li_md_stress_timestep_halving(double dt, int steps);

/* Max |P(t)-P(0)| per atom during NVE liquid run. */
LiMdStressResult li_md_stress_momentum(double dt, int steps);

/* Run all stress tests; returns count written (max cap). */
int li_md_stress_run_all(LiMdStressResult* out, int cap);

/* Print CSV rows to stdout (harness); always returns 0. */
int li_md_stress_cli_all(void);

#ifdef __cplusplus
}
#endif
