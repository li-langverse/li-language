// Shared Lennard-Jones MD kernel — cell-linked list, stack SoA arrays.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  LI_MD_N = 256,
  LI_MD_STEPS = 10000,
  LI_MD_TRACE_INTERVAL = 25,
};
#define LI_MD_DT 0.004
#define LI_MD_RC 2.5
#define LI_MD_BOX 10.0
#define LI_MD_SEED UINT64_C(7)

typedef struct LiMdRng {
  uint64_t state;
} LiMdRng;

typedef struct LiMdState {
  double px[LI_MD_N];
  double py[LI_MD_N];
  double pz[LI_MD_N];
  double vx[LI_MD_N];
  double vy[LI_MD_N];
  double vz[LI_MD_N];
  double fx[LI_MD_N];
  double fy[LI_MD_N];
  double fz[LI_MD_N];
} LiMdState;

static inline void li_md_rng_init(LiMdRng* rng, uint64_t seed) { rng->state = seed; }

static inline double li_md_rng_next(LiMdRng* rng) {
  rng->state = rng->state * UINT64_C(6364136223846793005) + UINT64_C(1);
  return (double)(rng->state >> 11) / (double)(1ULL << 53);
}

static inline double li_md_mic(double d) {
  const double half = 0.5 * LI_MD_BOX;
  if (d > half) return d - LI_MD_BOX;
  if (d < -half) return d + LI_MD_BOX;
  return d;
}

static inline double li_md_wrap(double x) {
  x = fmod(x, LI_MD_BOX);
  if (x < 0.0) x += LI_MD_BOX;
  return x;
}

static inline void li_md_init_lattice(LiMdState* s, LiMdRng* rng) {
  const int cells = (int)ceil(cbrt((double)LI_MD_N));
  const double spacing = LI_MD_BOX / (double)cells;
  int idx = 0;
  for (int ix = 0; ix < cells; ++ix) {
    for (int iy = 0; iy < cells; ++iy) {
      for (int iz = 0; iz < cells; ++iz) {
        if (idx >= LI_MD_N) return;
        s->px[idx] = ((double)ix + 0.5) * spacing;
        s->py[idx] = ((double)iy + 0.5) * spacing;
        s->pz[idx] = ((double)iz + 0.5) * spacing;
        s->vx[idx] = 0.01 * (li_md_rng_next(rng) - 0.5);
        s->vy[idx] = 0.01 * (li_md_rng_next(rng) - 0.5);
        s->vz[idx] = 0.01 * (li_md_rng_next(rng) - 0.5);
        ++idx;
      }
    }
  }
}

static inline void li_md_kinetic(const LiMdState* s, double* ke_out) {
  double ke = 0.0;
  for (int i = 0; i < LI_MD_N; ++i) {
    ke += 0.5 * (s->vx[i] * s->vx[i] + s->vy[i] * s->vy[i] + s->vz[i] * s->vz[i]);
  }
  *ke_out = ke;
}

static inline void li_md_potential(const LiMdState* s, double* pe_out) {
  const double rc2 = LI_MD_RC * LI_MD_RC;
  double pe = 0.0;
  for (int i = 0; i < LI_MD_N; ++i) {
    for (int j = i + 1; j < LI_MD_N; ++j) {
      const double dx = li_md_mic(s->px[j] - s->px[i]);
      const double dy = li_md_mic(s->py[j] - s->py[i]);
      const double dz = li_md_mic(s->pz[j] - s->pz[i]);
      const double r2 = dx * dx + dy * dy + dz * dz;
      if (r2 >= rc2 || r2 < 1e-12) continue;
      const double inv_r2 = 1.0 / r2;
      const double inv_r6 = inv_r2 * inv_r2 * inv_r2;
      const double inv_r12 = inv_r6 * inv_r6;
      pe += 4.0 * (inv_r12 - inv_r6);
    }
  }
  *pe_out = pe;
}

static inline void li_md_compute_forces(LiMdState* s) {
  const double rc2 = LI_MD_RC * LI_MD_RC;
  memset(s->fx, 0, sizeof(s->fx));
  memset(s->fy, 0, sizeof(s->fy));
  memset(s->fz, 0, sizeof(s->fz));
  for (int i = 0; i < LI_MD_N; ++i) {
    for (int j = i + 1; j < LI_MD_N; ++j) {
      const double dx = li_md_mic(s->px[j] - s->px[i]);
      const double dy = li_md_mic(s->py[j] - s->py[i]);
      const double dz = li_md_mic(s->pz[j] - s->pz[i]);
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

static inline void li_md_step(LiMdState* s) {
  for (int i = 0; i < LI_MD_N; ++i) {
    s->vx[i] += 0.5 * LI_MD_DT * s->fx[i];
    s->vy[i] += 0.5 * LI_MD_DT * s->fy[i];
    s->vz[i] += 0.5 * LI_MD_DT * s->fz[i];
  }
  for (int i = 0; i < LI_MD_N; ++i) {
    s->px[i] = li_md_wrap(s->px[i] + LI_MD_DT * s->vx[i]);
    s->py[i] = li_md_wrap(s->py[i] + LI_MD_DT * s->vy[i]);
    s->pz[i] = li_md_wrap(s->pz[i] + LI_MD_DT * s->vz[i]);
  }
  li_md_compute_forces(s);
  for (int i = 0; i < LI_MD_N; ++i) {
    s->vx[i] += 0.5 * LI_MD_DT * s->fx[i];
    s->vy[i] += 0.5 * LI_MD_DT * s->fy[i];
    s->vz[i] += 0.5 * LI_MD_DT * s->fz[i];
  }
}

static inline double li_md_run(void) {
  LiMdRng rng;
  LiMdState state;
  li_md_rng_init(&rng, LI_MD_SEED);
  li_md_init_lattice(&state, &rng);
  li_md_compute_forces(&state);
  double ke = 0.0, pe = 0.0;
  li_md_kinetic(&state, &ke);
  li_md_potential(&state, &pe);
  const double e0 = pe + ke;
  for (int step = 0; step < LI_MD_STEPS; ++step) {
    li_md_step(&state);
    (void)step;
  }
  li_md_kinetic(&state, &ke);
  li_md_potential(&state, &pe);
  const double e1 = pe + ke;
  const double denom = (e0 >= e1 ? e0 : e1);
  const double d = denom > 1e-12 ? denom : 1e-12;
  const double diff = e1 - e0;
  return (diff >= 0.0 ? diff : -diff) / d;
}

void li_md_lj_run(void);

void li_md_kernel(void);
double li_md_checksum(void);

#ifdef __cplusplus
}
#endif
