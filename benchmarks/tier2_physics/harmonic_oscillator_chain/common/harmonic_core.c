#include "harmonic_core.h"

#include <string.h>

enum {
  LI_HC_N = 64,
  LI_HC_STEPS = 2000000,
};
#define LI_HC_DT 0.001
#define LI_HC_K 1.0
#define LI_HC_MASS 1.0
#define LI_HC_SPACING 1.0

typedef struct LiHcState {
  double x[LI_HC_N];
  double v[LI_HC_N];
  double f[LI_HC_N];
} LiHcState;

static double g_li_harmonic_checksum;

static void li_hc_init(LiHcState* s) {
  for (int i = 0; i < LI_HC_N; ++i) {
    s->x[i] = (double)i * LI_HC_SPACING;
    s->v[i] = 0.0;
  }
  s->x[LI_HC_N / 2] += 0.1;
}

static void li_hc_forces(const LiHcState* s, LiHcState* out) {
  memset(out->f, 0, sizeof(out->f));
  for (int i = 0; i < LI_HC_N - 1; ++i) {
    const double stretch = s->x[i + 1] - s->x[i] - LI_HC_SPACING;
    const double force = LI_HC_K * stretch;
    out->f[i] += force;
    out->f[i + 1] -= force;
  }
  out->f[0] = 0.0;
  out->f[LI_HC_N - 1] = 0.0;
}

static double li_hc_energy(const LiHcState* s) {
  double ke = 0.0;
  double pe = 0.0;
  for (int i = 0; i < LI_HC_N; ++i) {
    ke += 0.5 * LI_HC_MASS * s->v[i] * s->v[i];
  }
  for (int i = 0; i < LI_HC_N - 1; ++i) {
    const double stretch = s->x[i + 1] - s->x[i] - LI_HC_SPACING;
    pe += 0.5 * LI_HC_K * stretch * stretch;
  }
  return ke + pe;
}

__attribute__((noinline)) void li_harmonic_chain_kernel(void) {
  LiHcState s, f;
  li_hc_init(&s);
  li_hc_forces(&s, &f);
  for (int step = 0; step < LI_HC_STEPS; ++step) {
    for (int i = 0; i < LI_HC_N; ++i) {
      s.v[i] += 0.5 * LI_HC_DT * f.f[i] / LI_HC_MASS;
    }
    for (int i = 0; i < LI_HC_N; ++i) {
      s.x[i] += LI_HC_DT * s.v[i];
    }
    s.x[0] = 0.0;
    s.x[LI_HC_N - 1] = (double)(LI_HC_N - 1) * LI_HC_SPACING;
    s.v[0] = 0.0;
    s.v[LI_HC_N - 1] = 0.0;
    li_hc_forces(&s, &f);
    for (int i = 0; i < LI_HC_N; ++i) {
      s.v[i] += 0.5 * LI_HC_DT * f.f[i] / LI_HC_MASS;
    }
    (void)step;
  }
  g_li_harmonic_checksum = li_hc_energy(&s);
}

double li_harmonic_chain_checksum(void) { return g_li_harmonic_checksum; }
