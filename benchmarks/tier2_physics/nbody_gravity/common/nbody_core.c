#include "nbody_core.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
  LI_NB_N = 128,
  LI_NB_STEPS = 50000,
};
#define LI_NB_DT 0.01
#define LI_NB_G 1.0
#define LI_NB_SOFT 1e-6
#define LI_NB_MASS 1.0
#define LI_NB_SEED UINT64_C(42)

typedef struct LiNbRng {
  uint64_t state;
} LiNbRng;

typedef struct LiNbState {
  double px[LI_NB_N];
  double py[LI_NB_N];
  double pz[LI_NB_N];
  double vx[LI_NB_N];
  double vy[LI_NB_N];
  double vz[LI_NB_N];
  double fx[LI_NB_N];
  double fy[LI_NB_N];
  double fz[LI_NB_N];
} LiNbState;

static double g_li_nbody_checksum;

static void li_nb_rng_init(LiNbRng* rng, uint64_t seed) { rng->state = seed; }

static double li_nb_rng_next(LiNbRng* rng) {
  rng->state = rng->state * UINT64_C(6364136223846793005) + UINT64_C(1);
  return (double)(rng->state >> 11) / (double)(1ULL << 53);
}

static void li_nb_init(LiNbState* s, LiNbRng* rng) {
  for (int i = 0; i < LI_NB_N; ++i) {
    s->px[i] = li_nb_rng_next(rng) - 0.5;
    s->py[i] = li_nb_rng_next(rng) - 0.5;
    s->pz[i] = li_nb_rng_next(rng) - 0.5;
    s->vx[i] = 0.01 * (li_nb_rng_next(rng) - 0.5);
    s->vy[i] = 0.01 * (li_nb_rng_next(rng) - 0.5);
    s->vz[i] = 0.01 * (li_nb_rng_next(rng) - 0.5);
  }
}

static void li_nb_forces(const LiNbState* s, LiNbState* out) {
  memset(out->fx, 0, sizeof(out->fx));
  memset(out->fy, 0, sizeof(out->fy));
  memset(out->fz, 0, sizeof(out->fz));
  const double eps2 = LI_NB_SOFT * LI_NB_SOFT;
  for (int i = 0; i < LI_NB_N; ++i) {
    for (int j = i + 1; j < LI_NB_N; ++j) {
      const double dx = s->px[j] - s->px[i];
      const double dy = s->py[j] - s->py[i];
      const double dz = s->pz[j] - s->pz[i];
      const double r2 = dx * dx + dy * dy + dz * dz + eps2;
      const double inv_r = 1.0 / sqrt(r2);
      const double inv_r3 = inv_r * inv_r * inv_r;
      const double scale = LI_NB_G * LI_NB_MASS * LI_NB_MASS * inv_r3;
      const double fx = scale * dx;
      const double fy = scale * dy;
      const double fz = scale * dz;
      out->fx[i] += fx;
      out->fy[i] += fy;
      out->fz[i] += fz;
      out->fx[j] -= fx;
      out->fy[j] -= fy;
      out->fz[j] -= fz;
    }
  }
}

static double li_nb_energy(const LiNbState* s) {
  double ke = 0.0;
  double pe = 0.0;
  const double eps2 = LI_NB_SOFT * LI_NB_SOFT;
  for (int i = 0; i < LI_NB_N; ++i) {
    ke += 0.5 * LI_NB_MASS
          * (s->vx[i] * s->vx[i] + s->vy[i] * s->vy[i] + s->vz[i] * s->vz[i]);
  }
  for (int i = 0; i < LI_NB_N; ++i) {
    for (int j = i + 1; j < LI_NB_N; ++j) {
      const double dx = s->px[j] - s->px[i];
      const double dy = s->py[j] - s->py[i];
      const double dz = s->pz[j] - s->pz[i];
      const double r = sqrt(dx * dx + dy * dy + dz * dz + eps2);
      pe -= LI_NB_G * LI_NB_MASS * LI_NB_MASS / r;
    }
  }
  return ke + pe;
}

__attribute__((noinline)) void li_nbody_gravity_kernel(void) {
  LiNbRng rng;
  LiNbState s, f;
  li_nb_rng_init(&rng, LI_NB_SEED);
  li_nb_init(&s, &rng);
  li_nb_forces(&s, &f);
  for (int step = 0; step < LI_NB_STEPS; ++step) {
    for (int i = 0; i < LI_NB_N; ++i) {
      s.vx[i] += 0.5 * LI_NB_DT * f.fx[i] / LI_NB_MASS;
      s.vy[i] += 0.5 * LI_NB_DT * f.fy[i] / LI_NB_MASS;
      s.vz[i] += 0.5 * LI_NB_DT * f.fz[i] / LI_NB_MASS;
    }
    for (int i = 0; i < LI_NB_N; ++i) {
      s.px[i] += LI_NB_DT * s.vx[i];
      s.py[i] += LI_NB_DT * s.vy[i];
      s.pz[i] += LI_NB_DT * s.vz[i];
    }
    li_nb_forces(&s, &f);
    for (int i = 0; i < LI_NB_N; ++i) {
      s.vx[i] += 0.5 * LI_NB_DT * f.fx[i] / LI_NB_MASS;
      s.vy[i] += 0.5 * LI_NB_DT * f.fy[i] / LI_NB_MASS;
      s.vz[i] += 0.5 * LI_NB_DT * f.fz[i] / LI_NB_MASS;
    }
    (void)step;
  }
  g_li_nbody_checksum = li_nb_energy(&s);
}

double li_nbody_gravity_checksum(void) { return g_li_nbody_checksum; }
