#include "pendulum_core.h"

#include <math.h>
#include <string.h>

enum { LI_DP_STEPS = 3000000 };
#define LI_DP_DT 0.0005
#define LI_DP_M1 1.0
#define LI_DP_M2 1.0
#define LI_DP_L1 1.0
#define LI_DP_L2 1.0
#define LI_DP_G 9.81

static double g_li_pendulum_checksum;

typedef struct LiDpState {
  double y[4];
} LiDpState;

static void li_dp_derivs(const double y[4], double dydt[4]) {
  const double t1 = y[0];
  const double t2 = y[1];
  const double w1 = y[2];
  const double w2 = y[3];
  const double delta = t1 - t2;
  const double c = cos(delta);
  const double s = sin(delta);
  const double den1 = (LI_DP_M1 + LI_DP_M2) * LI_DP_L1 - LI_DP_M2 * LI_DP_L1 * c * c;
  const double den2 = (LI_DP_L2 / LI_DP_L1) * den1;
  dydt[0] = w1;
  dydt[1] = w2;
  dydt[2] = (-LI_DP_M2 * LI_DP_L1 * w1 * w1 * s * c + LI_DP_M2 * LI_DP_G * sin(t2) * c
             + LI_DP_M2 * LI_DP_L2 * w2 * w2 * s - (LI_DP_M1 + LI_DP_M2) * LI_DP_G * sin(t1))
            / den1;
  dydt[3] = (-LI_DP_M2 * LI_DP_L2 * w2 * w2 * s * c
             + (LI_DP_M1 + LI_DP_M2)
                   * (LI_DP_G * sin(t1) * c - LI_DP_L1 * w1 * w1 * s - LI_DP_G * sin(t2)))
            / den2;
}

static void li_dp_rk4_step(LiDpState* s) {
  const double dt = LI_DP_DT;
  const double h = dt;
  double k1[4], k2[4], k3[4], k4[4], tmp[4];

  li_dp_derivs(s->y, k1);
  for (int i = 0; i < 4; ++i) tmp[i] = s->y[i] + 0.5 * h * k1[i];
  li_dp_derivs(tmp, k2);
  for (int i = 0; i < 4; ++i) tmp[i] = s->y[i] + 0.5 * h * k2[i];
  li_dp_derivs(tmp, k3);
  for (int i = 0; i < 4; ++i) tmp[i] = s->y[i] + h * k3[i];
  li_dp_derivs(tmp, k4);
  for (int i = 0; i < 4; ++i) {
    s->y[i] += (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
  }
}

static double li_dp_energy(const LiDpState* s) {
  const double t1 = s->y[0];
  const double t2 = s->y[1];
  const double w1 = s->y[2];
  const double w2 = s->y[3];
  const double delta = t1 - t2;
  const double v1_sq = LI_DP_L1 * LI_DP_L1 * w1 * w1;
  const double v2_sq = LI_DP_L1 * LI_DP_L1 * w1 * w1 + LI_DP_L2 * LI_DP_L2 * w2 * w2
                        + 2.0 * LI_DP_L1 * LI_DP_L2 * w1 * w2 * cos(delta);
  const double ke = 0.5 * LI_DP_M1 * v1_sq + 0.5 * LI_DP_M2 * v2_sq;
  const double pe = -(LI_DP_M1 + LI_DP_M2) * LI_DP_G * LI_DP_L1 * cos(t1)
                    - LI_DP_M2 * LI_DP_G * LI_DP_L2 * cos(t2);
  return ke + pe;
}

__attribute__((noinline)) void li_double_pendulum_kernel(void) {
  LiDpState s;
  s.y[0] = 2.0;
  s.y[1] = 2.2;
  s.y[2] = 0.0;
  s.y[3] = 0.0;
  for (int step = 0; step < LI_DP_STEPS; ++step) {
    li_dp_rk4_step(&s);
    (void)step;
  }
  g_li_pendulum_checksum = li_dp_energy(&s);
}

double li_double_pendulum_checksum(void) { return g_li_pendulum_checksum; }
