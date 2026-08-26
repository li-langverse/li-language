#include "sph_dam_core.h"

#include <math.h>
#include <string.h>

/* Minimal 2D dam-break stub: column IC, gravity, pairwise repulsion (SPH v0). */
enum { LI_SPH_N = 256, LI_SPH_STEPS = 8000 };
#define LI_SPH_BOX 1.0
#define LI_SPH_H 0.08
#define LI_SPH_MASS 1.0
#define LI_SPH_DT 0.00025
#define LI_SPH_G 9.81
#define LI_SPH_K 500.0

typedef struct LiSphParticle {
  double x[2];
  double v[2];
  double a[2];
} LiSphParticle;

typedef struct LiSphState {
  LiSphParticle p[LI_SPH_N];
} LiSphState;

static double g_li_sph_dam_checksum;

static void li_sph_init(LiSphState* s) {
  int idx = 0;
  const int nx = 16;
  const int ny = 16;
  const double dx = 0.04;
  for (int j = 0; j < ny && idx < LI_SPH_N; ++j) {
    for (int i = 0; i < nx && idx < LI_SPH_N; ++i) {
      s->p[idx].x[0] = 0.05 + (double)i * dx;
      s->p[idx].x[1] = 0.05 + (double)j * dx;
      s->p[idx].v[0] = 0.0;
      s->p[idx].v[1] = 0.0;
      s->p[idx].a[0] = 0.0;
      s->p[idx].a[1] = -LI_SPH_G;
      ++idx;
    }
  }
  for (; idx < LI_SPH_N; ++idx) {
    memset(&s->p[idx], 0, sizeof(s->p[idx]));
  }
}

static double li_sph_sum_y(const LiSphState* s) {
  double acc = 0.0;
  for (int i = 0; i < LI_SPH_N; ++i) {
    acc += s->p[i].x[1];
  }
  return acc;
}

static void li_sph_forces(LiSphState* s) {
  for (int i = 0; i < LI_SPH_N; ++i) {
    s->p[i].a[0] = 0.0;
    s->p[i].a[1] = -LI_SPH_G;
  }
  const double h2 = LI_SPH_H * LI_SPH_H;
  for (int i = 0; i < LI_SPH_N; ++i) {
    for (int j = i + 1; j < LI_SPH_N; ++j) {
      const double rx = s->p[j].x[0] - s->p[i].x[0];
      const double ry = s->p[j].x[1] - s->p[i].x[1];
      const double r2 = rx * rx + ry * ry + 1e-12;
      const double r = sqrt(r2);
      if (r >= LI_SPH_H) {
        continue;
      }
      const double q = 1.0 - r / LI_SPH_H;
      const double f = LI_SPH_K * q * q / r;
      const double fx = f * rx;
      const double fy = f * ry;
      s->p[i].a[0] -= fx;
      s->p[i].a[1] -= fy;
      s->p[j].a[0] += fx;
      s->p[j].a[1] += fy;
    }
  }
  for (int i = 0; i < LI_SPH_N; ++i) {
    if (s->p[i].x[0] < 0.0) {
      s->p[i].x[0] = 0.0;
      s->p[i].v[0] = 0.0;
    }
    if (s->p[i].x[0] > LI_SPH_BOX) {
      s->p[i].x[0] = LI_SPH_BOX;
      s->p[i].v[0] = 0.0;
    }
    if (s->p[i].x[1] < 0.0) {
      s->p[i].x[1] = 0.0;
      s->p[i].v[1] = 0.0;
    }
    if (s->p[i].x[1] > LI_SPH_BOX) {
      s->p[i].x[1] = LI_SPH_BOX;
      s->p[i].v[1] = 0.0;
    }
  }
}

__attribute__((noinline)) void li_sph_dam_2d_kernel(void) {
  LiSphState s;
  li_sph_init(&s);
  for (int step = 0; step < LI_SPH_STEPS; ++step) {
    li_sph_forces(&s);
    for (int i = 0; i < LI_SPH_N; ++i) {
      s.p[i].v[0] += 0.5 * LI_SPH_DT * s.p[i].a[0];
      s.p[i].v[1] += 0.5 * LI_SPH_DT * s.p[i].a[1];
      s.p[i].x[0] += LI_SPH_DT * s.p[i].v[0];
      s.p[i].x[1] += LI_SPH_DT * s.p[i].v[1];
    }
    li_sph_forces(&s);
    for (int i = 0; i < LI_SPH_N; ++i) {
      s.p[i].v[0] += 0.5 * LI_SPH_DT * s.p[i].a[0];
      s.p[i].v[1] += 0.5 * LI_SPH_DT * s.p[i].a[1];
    }
    (void)step;
  }
  g_li_sph_dam_checksum = li_sph_sum_y(&s);
}

double li_sph_dam_2d_checksum(void) { return g_li_sph_dam_checksum; }
