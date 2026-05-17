#include "euler2d_core.h"

#include <math.h>
#include <string.h>

/* 2D velocity–pressure proxy (explicit advection + diffusion + divergence damping). */
enum {
  LI_EU_NX = 96,
  LI_EU_NY = 96,
  LI_EU_STEPS = 12000,
};
#define LI_EU_DX 0.01
#define LI_EU_DT 0.0004
#define LI_EU_NU 0.02
#define LI_EU_DIV 0.15

typedef struct LiEuState {
  double vx[LI_EU_NX][LI_EU_NY];
  double vy[LI_EU_NX][LI_EU_NY];
  double wx[LI_EU_NX][LI_EU_NY];
  double wy[LI_EU_NX][LI_EU_NY];
} LiEuState;

static double g_li_euler_2d_checksum;

static void li_eu_init(LiEuState* s) {
  for (int i = 0; i < LI_EU_NX; ++i) {
    for (int j = 0; j < LI_EU_NY; ++j) {
      const double x = (double)i * LI_EU_DX;
      const double y = (double)j * LI_EU_DX;
      const double bump = exp(-((x - 0.5) * (x - 0.5) + (y - 0.5) * (y - 0.5)) / 0.02);
      s->vx[i][j] = 0.4 * bump * sin(6.283185307179586 * x);
      s->vy[i][j] = 0.4 * bump * cos(6.283185307179586 * y);
    }
  }
}

static double li_eu_kinetic_sum(const LiEuState* s) {
  double acc = 0.0;
  for (int i = 0; i < LI_EU_NX; ++i) {
    for (int j = 0; j < LI_EU_NY; ++j) {
      acc += s->vx[i][j] * s->vx[i][j] + s->vy[i][j] * s->vy[i][j];
    }
  }
  return acc;
}

__attribute__((noinline)) void li_euler_2d_kernel(void) {
  LiEuState s;
  li_eu_init(&s);
  const double r = LI_EU_NU * LI_EU_DT / (LI_EU_DX * LI_EU_DX);
  const double adv = LI_EU_DT / LI_EU_DX;
  for (int step = 0; step < LI_EU_STEPS; ++step) {
    for (int i = 1; i < LI_EU_NX - 1; ++i) {
      for (int j = 1; j < LI_EU_NY - 1; ++j) {
        const double div = (s.vx[i + 1][j] - s.vx[i - 1][j] + s.vy[i][j + 1] - s.vy[i][j - 1])
                           / (2.0 * LI_EU_DX);
        const double adv_x = s.vx[i][j] > 0.0
                                 ? s.vx[i][j] - s.vx[i - 1][j]
                                 : s.vx[i + 1][j] - s.vx[i][j];
        const double adv_y = s.vy[i][j] > 0.0
                                 ? s.vy[i][j] - s.vy[i][j - 1]
                                 : s.vy[i][j + 1] - s.vy[i][j];
        const double lap_vx = s.vx[i + 1][j] + s.vx[i - 1][j] + s.vx[i][j + 1] + s.vx[i][j - 1]
                              - 4.0 * s.vx[i][j];
        const double lap_vy = s.vy[i + 1][j] + s.vy[i - 1][j] + s.vy[i][j + 1] + s.vy[i][j - 1]
                              - 4.0 * s.vy[i][j];
        s.wx[i][j] = s.vx[i][j] - adv * (s.vx[i][j] * adv_x + s.vy[i][j] * adv_y) + r * lap_vx
                     - LI_EU_DIV * div * LI_EU_DT;
        s.wy[i][j] = s.vy[i][j] - adv * (s.vx[i][j] * adv_x + s.vy[i][j] * adv_y) + r * lap_vy
                     - LI_EU_DIV * div * LI_EU_DT;
      }
    }
    for (int i = 1; i < LI_EU_NX - 1; ++i) {
      for (int j = 1; j < LI_EU_NY - 1; ++j) {
        s.vx[i][j] = s.wx[i][j];
        s.vy[i][j] = s.wy[i][j];
      }
    }
    (void)step;
  }
  g_li_euler_2d_checksum = li_eu_kinetic_sum(&s);
}

double li_euler_2d_checksum(void) { return g_li_euler_2d_checksum; }
