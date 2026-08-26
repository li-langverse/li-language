#include "md_stress.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define STRESS_N 256
#define STRESS_RC 2.5
#define STRESS_RHO 0.75
#define STRESS_TEMP 1.0
#define STRESS_SEED UINT64_C(42)

typedef struct Rng {
  uint64_t state;
} Rng;

static void rng_init(Rng* r, uint64_t seed) { r->state = seed; }

static double rng_next(Rng* r) {
  r->state = r->state * UINT64_C(6364136223846793005) + UINT64_C(1);
  return (double)(r->state >> 11) / (double)(1ULL << 53);
}

/* Box–Muller standard normal using two uniforms. */
static double rng_normal(Rng* r) {
  double u1 = rng_next(r);
  if (u1 < 1e-12) u1 = 1e-12;
  const double u2 = rng_next(r);
  return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

static double box_from_rho(int n, double rho) {
  return pow((double)n / rho, 1.0 / 3.0);
}

static double mic(double d, double box) {
  const double half = 0.5 * box;
  if (d > half) return d - box;
  if (d < -half) return d + box;
  return d;
}

static double wrap_pos(double x, double box) {
  x = fmod(x, box);
  if (x < 0.0) x += box;
  return x;
}

typedef struct SoA {
  double px[STRESS_N];
  double py[STRESS_N];
  double pz[STRESS_N];
  double vx[STRESS_N];
  double vy[STRESS_N];
  double vz[STRESS_N];
  double fx[STRESS_N];
  double fy[STRESS_N];
  double fz[STRESS_N];
} SoA;

static void init_fcc_liquid(SoA* s, double box, double temperature, Rng* rng) {
  static const double basis[4][3] = {
      {0.0, 0.0, 0.0}, {0.0, 0.5, 0.5}, {0.5, 0.0, 0.5}, {0.5, 0.5, 0.0}};
  int ncell = 1;
  while (4 * ncell * ncell * ncell < STRESS_N) {
    ++ncell;
  }
  const double a = box / (double)ncell;
  int idx = 0;
  for (int ix = 0; ix < ncell && idx < STRESS_N; ++ix) {
    for (int iy = 0; iy < ncell && idx < STRESS_N; ++iy) {
      for (int iz = 0; iz < ncell && idx < STRESS_N; ++iz) {
        for (int b = 0; b < 4 && idx < STRESS_N; ++b) {
          s->px[idx] = ((double)ix + basis[b][0]) * a;
          s->py[idx] = ((double)iy + basis[b][1]) * a;
          s->pz[idx] = ((double)iz + basis[b][2]) * a;
          const double scale = sqrt(temperature);
          s->vx[idx] = scale * rng_normal(rng);
          s->vy[idx] = scale * rng_normal(rng);
          s->vz[idx] = scale * rng_normal(rng);
          ++idx;
        }
      }
    }
  }
  double px_sum = 0.0, py_sum = 0.0, pz_sum = 0.0, ke = 0.0;
  for (int i = 0; i < STRESS_N; ++i) {
    px_sum += s->vx[i];
    py_sum += s->vy[i];
    pz_sum += s->vz[i];
    ke += 0.5 * (s->vx[i] * s->vx[i] + s->vy[i] * s->vy[i] + s->vz[i] * s->vz[i]);
  }
  const double inv_n = 1.0 / (double)STRESS_N;
  const double target = 1.5 * (double)STRESS_N * temperature;
  const double vel_scale = ke > 1e-20 ? sqrt(target / ke) : 1.0;
  for (int i = 0; i < STRESS_N; ++i) {
    s->vx[i] = (s->vx[i] - px_sum * inv_n) * vel_scale;
    s->vy[i] = (s->vy[i] - py_sum * inv_n) * vel_scale;
    s->vz[i] = (s->vz[i] - pz_sum * inv_n) * vel_scale;
  }
}

static double lj_pe_pair(double r2) {
  if (r2 >= STRESS_RC * STRESS_RC || r2 < 1e-12) return 0.0;
  const double inv_r2 = 1.0 / r2;
  const double inv_r6 = inv_r2 * inv_r2 * inv_r2;
  const double inv_r12 = inv_r6 * inv_r6;
  return 4.0 * (inv_r12 - inv_r6);
}

static double lj_pe_shifted(double r2) {
  const double rc2 = STRESS_RC * STRESS_RC;
  const double pe = lj_pe_pair(r2);
  if (r2 >= rc2) return 0.0;
  const double pe_rc = lj_pe_pair(rc2);
  return pe - pe_rc;
}

static void lj_forces(SoA* s, double box) {
  const double rc2 = STRESS_RC * STRESS_RC;
  memset(s->fx, 0, sizeof(s->fx));
  memset(s->fy, 0, sizeof(s->fy));
  memset(s->fz, 0, sizeof(s->fz));
  for (int i = 0; i < STRESS_N; ++i) {
    for (int j = i + 1; j < STRESS_N; ++j) {
      const double dx = mic(s->px[j] - s->px[i], box);
      const double dy = mic(s->py[j] - s->py[i], box);
      const double dz = mic(s->pz[j] - s->pz[i], box);
      const double r2 = dx * dx + dy * dy + dz * dz;
      if (r2 >= rc2 || r2 < 1e-12) continue;
      const double inv_r2 = 1.0 / r2;
      const double inv_r6 = inv_r2 * inv_r2 * inv_r2;
      const double inv_r12 = inv_r6 * inv_r6;
      const double f_scalar = 48.0 * inv_r12 - 24.0 * inv_r6;
      const double fx = f_scalar * dx;
      const double fy = f_scalar * dy;
      const double fz = f_scalar * dz;
      s->fx[i] -= fx;
      s->fy[i] -= fy;
      s->fz[i] -= fz;
      s->fx[j] += fx;
      s->fy[j] += fy;
      s->fz[j] += fz;
    }
  }
}

static double total_energy(const SoA* s, double box) {
  double ke = 0.0, pe = 0.0;
  for (int i = 0; i < STRESS_N; ++i) {
    ke += 0.5 * (s->vx[i] * s->vx[i] + s->vy[i] * s->vy[i] + s->vz[i] * s->vz[i]);
  }
  for (int i = 0; i < STRESS_N; ++i) {
    for (int j = i + 1; j < STRESS_N; ++j) {
      const double dx = mic(s->px[j] - s->px[i], box);
      const double dy = mic(s->py[j] - s->py[i], box);
      const double dz = mic(s->pz[j] - s->pz[i], box);
      const double r2 = dx * dx + dy * dy + dz * dz;
      pe += lj_pe_shifted(r2);
    }
  }
  return pe + ke;
}

static void vv_step(SoA* s, double box, double dt) {
  for (int i = 0; i < STRESS_N; ++i) {
    s->vx[i] += 0.5 * dt * s->fx[i];
    s->vy[i] += 0.5 * dt * s->fy[i];
    s->vz[i] += 0.5 * dt * s->fz[i];
  }
  for (int i = 0; i < STRESS_N; ++i) {
    s->px[i] = wrap_pos(s->px[i] + dt * s->vx[i], box);
    s->py[i] = wrap_pos(s->py[i] + dt * s->vy[i], box);
    s->pz[i] = wrap_pos(s->pz[i] + dt * s->vz[i], box);
  }
  lj_forces(s, box);
  for (int i = 0; i < STRESS_N; ++i) {
    s->vx[i] += 0.5 * dt * s->fx[i];
    s->vy[i] += 0.5 * dt * s->fy[i];
    s->vz[i] += 0.5 * dt * s->fz[i];
  }
}

static double nve_energy_msd(double dt, int steps) {
  Rng rng;
  rng_init(&rng, STRESS_SEED);
  const double box = box_from_rho(STRESS_N, STRESS_RHO);
  SoA s;
  init_fcc_liquid(&s, box, STRESS_TEMP, &rng);
  lj_forces(&s, box);
  const int equil = steps / 5;
  for (int step = 0; step < equil; ++step) {
    vv_step(&s, box, dt);
  }
  double mean = 0.0;
  double msd = 0.0;
  for (int step = 0; step < steps; ++step) {
    const double e = total_energy(&s, box) / (double)STRESS_N;
    const double delta = e - mean;
    mean += delta / (double)(step + 1);
    const double delta2 = e - mean;
    msd += delta * delta2;
    vv_step(&s, box, dt);
  }
  return msd / (double)steps;
}

LiMdStressResult li_md_stress_harmonic(double dt, int steps) {
  double x = 1.0;
  double v = 0.0;
  const double e0 = 0.5 * x * x;
  double max_dev = 0.0;
  for (int step = 0; step < steps; ++step) {
    const double a = -x;
    v += 0.5 * dt * a;
    x += dt * v;
    const double a2 = -x;
    v += 0.5 * dt * a2;
    const double e = 0.5 * (x * x + v * v);
    const double dev = fabs(e - e0) / e0;
    if (dev > max_dev) max_dev = dev;
    (void)step;
  }
  const double threshold = 0.5 * dt * dt;
  LiMdStressResult r = {"harmonic_energy", max_dev, threshold, max_dev < threshold};
  return r;
}

LiMdStressResult li_md_stress_nve_energy_msd(double dt, int steps) {
  const double msd = nve_energy_msd(dt, steps);
  const double threshold = 3.0e-8;
  LiMdStressResult r = {"nve_energy_msd", msd, threshold, msd < threshold};
  return r;
}

LiMdStressResult li_md_stress_timestep_halving(double dt, int steps) {
  const double msd_coarse = nve_energy_msd(dt, steps);
  const double msd_fine = nve_energy_msd(0.5 * dt, steps);
  const double ratio = msd_coarse / (msd_fine > 1e-30 ? msd_fine : 1e-30);
  LiMdStressResult r = {"timestep_halving_ratio", ratio, 16.0, ratio >= 12.0 && ratio <= 20.0};
  return r;
}

LiMdStressResult li_md_stress_momentum(double dt, int steps) {
  Rng rng;
  rng_init(&rng, STRESS_SEED);
  const double box = box_from_rho(STRESS_N, STRESS_RHO);
  SoA s;
  init_fcc_liquid(&s, box, STRESS_TEMP, &rng);
  lj_forces(&s, box);
  double p0x = 0.0, p0y = 0.0, p0z = 0.0;
  for (int i = 0; i < STRESS_N; ++i) {
    p0x += s.vx[i];
    p0y += s.vy[i];
    p0z += s.vz[i];
  }
  double max_drift = 0.0;
  for (int step = 0; step < steps; ++step) {
    double px = 0.0, py = 0.0, pz = 0.0;
    for (int i = 0; i < STRESS_N; ++i) {
      px += s.vx[i];
      py += s.vy[i];
      pz += s.vz[i];
    }
    const double dx = px - p0x;
    const double dy = py - p0y;
    const double dz = pz - p0z;
    const double drift = sqrt(dx * dx + dy * dy + dz * dz) / (double)STRESS_N;
    if (drift > max_drift) max_drift = drift;
    vv_step(&s, box, dt);
    (void)step;
  }
  const double threshold = 1.0e-8;
  LiMdStressResult r = {"momentum_drift", max_drift, threshold, max_drift < threshold};
  return r;
}

int li_md_stress_run_all(LiMdStressResult* out, int cap) {
  if (cap < 4) return 0;
  out[0] = li_md_stress_harmonic(0.004, 50000);
  out[1] = li_md_stress_nve_energy_msd(0.004, 20000);
  out[2] = li_md_stress_timestep_halving(0.004, 5000);
  out[3] = li_md_stress_momentum(0.004, 5000);
  return 4;
}

int li_md_stress_cli_all(void) {
  LiMdStressResult results[8];
  const int n = li_md_stress_run_all(results, 8);
  printf("name,value,threshold,passed\n");
  for (int i = 0; i < n; ++i) {
    printf("%s,%.12e,%.12e,%d\n", results[i].name, results[i].value, results[i].threshold,
           results[i].passed);
  }
  return 0;
}
