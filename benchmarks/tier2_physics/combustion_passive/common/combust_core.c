#include "combust_core.h"

#include <math.h>
#include <string.h>

/* Passive combustion proxy: reaction–diffusion u_t = D∇²u + k u(1-u). */
enum {
  LI_CB_NX = 128,
  LI_CB_NY = 128,
  LI_CB_STEPS = 18000,
};
#define LI_CB_DX 0.01
#define LI_CB_DT 0.00015
#define LI_CB_DIFF 0.08
#define LI_CB_K 2.5

typedef struct LiCbState {
  double u[LI_CB_NX][LI_CB_NY];
  double v[LI_CB_NX][LI_CB_NY];
} LiCbState;

static double g_li_combustion_passive_checksum;

static void li_cb_init(LiCbState* s) {
  for (int i = 0; i < LI_CB_NX; ++i) {
    for (int j = 0; j < LI_CB_NY; ++j) {
      const double x = (double)i * LI_CB_DX;
      const double y = (double)j * LI_CB_DX;
      const double r2 = (x - 0.25) * (x - 0.25) + (y - 0.5) * (y - 0.5);
      s->u[i][j] = 0.05 + 0.9 * exp(-r2 / 0.003);
    }
  }
}

static double li_cb_sum(const LiCbState* s) {
  double acc = 0.0;
  for (int i = 0; i < LI_CB_NX; ++i) {
    for (int j = 0; j < LI_CB_NY; ++j) {
      acc += s->u[i][j];
    }
  }
  return acc;
}

__attribute__((noinline)) void li_combustion_passive_kernel(void) {
  LiCbState s;
  li_cb_init(&s);
  const double r = LI_CB_DIFF * LI_CB_DT / (LI_CB_DX * LI_CB_DX);
  for (int step = 0; step < LI_CB_STEPS; ++step) {
    for (int i = 1; i < LI_CB_NX - 1; ++i) {
      for (int j = 1; j < LI_CB_NY - 1; ++j) {
        const double u_c = s.u[i][j];
        const double lap = s.u[i + 1][j] + s.u[i - 1][j] + s.u[i][j + 1] + s.u[i][j - 1]
                           - 4.0 * u_c;
        const double react = LI_CB_K * u_c * (1.0 - u_c);
        s.v[i][j] = u_c + r * lap + LI_CB_DT * react;
        if (s.v[i][j] < 0.0) {
          s.v[i][j] = 0.0;
        }
        if (s.v[i][j] > 1.0) {
          s.v[i][j] = 1.0;
        }
      }
    }
    for (int i = 1; i < LI_CB_NX - 1; ++i) {
      for (int j = 1; j < LI_CB_NY - 1; ++j) {
        s.u[i][j] = s.v[i][j];
      }
    }
    (void)step;
  }
  g_li_combustion_passive_checksum = li_cb_sum(&s);
}

double li_combustion_passive_checksum(void) { return g_li_combustion_passive_checksum; }
