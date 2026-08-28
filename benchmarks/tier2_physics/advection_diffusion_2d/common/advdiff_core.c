#include "advdiff_core.h"

#include <math.h>
#include <string.h>

/* Passive scalar: upwind advection + explicit diffusion (smoke / wind proxy). */
enum {
  LI_AD_NX = 128,
  LI_AD_NY = 128,
  LI_AD_STEPS = 15000,
};
#define LI_AD_DX 0.01
#define LI_AD_DT 0.0002
#define LI_AD_VX 0.8
#define LI_AD_VY 0.2
#define LI_AD_DIFF 0.05

typedef struct LiAdState {
  double u[LI_AD_NX][LI_AD_NY];
  double v[LI_AD_NX][LI_AD_NY];
} LiAdState;

static double g_li_advdiff_checksum;

static void li_ad_init(LiAdState* s) {
  for (int i = 0; i < LI_AD_NX; ++i) {
    for (int j = 0; j < LI_AD_NY; ++j) {
      const double x = (double)i * LI_AD_DX;
      const double y = (double)j * LI_AD_DX;
      const double r2 = (x - 0.35) * (x - 0.35) + (y - 0.35) * (y - 0.35);
      s->u[i][j] = exp(-r2 / 0.002);
    }
  }
}

static double li_ad_sum(const LiAdState* s) {
  double acc = 0.0;
  for (int i = 0; i < LI_AD_NX; ++i) {
    for (int j = 0; j < LI_AD_NY; ++j) {
      acc += s->u[i][j];
    }
  }
  return acc;
}

__attribute__((noinline)) void li_advdiff_2d_kernel(void) {
  LiAdState s;
  li_ad_init(&s);
  const double r = LI_AD_DIFF * LI_AD_DT / (LI_AD_DX * LI_AD_DX);
  const double cfx = LI_AD_VX * LI_AD_DT / LI_AD_DX;
  const double cfy = LI_AD_VY * LI_AD_DT / LI_AD_DX;
  for (int step = 0; step < LI_AD_STEPS; ++step) {
    for (int i = 1; i < LI_AD_NX - 1; ++i) {
      for (int j = 1; j < LI_AD_NY - 1; ++j) {
        const double u_c = s.u[i][j];
        const double du_x =
            cfx > 0.0 ? u_c - s.u[i - 1][j] : s.u[i + 1][j] - u_c;
        const double du_y =
            cfy > 0.0 ? u_c - s.u[i][j - 1] : s.u[i][j + 1] - u_c;
        const double lap = s.u[i + 1][j] + s.u[i - 1][j] + s.u[i][j + 1] + s.u[i][j - 1]
                           - 4.0 * u_c;
        s.v[i][j] = u_c - cfx * du_x - cfy * du_y + r * lap;
      }
    }
    for (int i = 1; i < LI_AD_NX - 1; ++i) {
      for (int j = 1; j < LI_AD_NY - 1; ++j) {
        s.u[i][j] = s.v[i][j];
      }
    }
    (void)step;
  }
  g_li_advdiff_checksum = li_ad_sum(&s);
}

double li_advdiff_2d_checksum(void) { return g_li_advdiff_checksum; }
