#include "windbc_core.h"

#include <math.h>
#include <string.h>

/* Scalar transport with inflow (left) + open top wind shear BC. */
enum {
  LI_WB_NX = 128,
  LI_WB_NY = 128,
  LI_WB_STEPS = 16000,
};
#define LI_WB_DX 0.01
#define LI_WB_DT 0.0002
#define LI_WB_VX 1.1
#define LI_WB_VY 0.15
#define LI_WB_DIFF 0.02
#define LI_WB_INFLOW 1.0

typedef struct LiWbState {
  double u[LI_WB_NX][LI_WB_NY];
  double v[LI_WB_NX][LI_WB_NY];
} LiWbState;

static double g_li_wind_field_bc_checksum;

static void li_wb_init(LiWbState* s) {
  memset(s, 0, sizeof(*s));
}

static double li_wb_sum(const LiWbState* s) {
  double acc = 0.0;
  for (int i = 0; i < LI_WB_NX; ++i) {
    for (int j = 0; j < LI_WB_NY; ++j) {
      acc += s->u[i][j];
    }
  }
  return acc;
}

__attribute__((noinline)) void li_wind_field_bc_kernel(void) {
  LiWbState s;
  li_wb_init(&s);
  const double r = LI_WB_DIFF * LI_WB_DT / (LI_WB_DX * LI_WB_DX);
  const double cfx = LI_WB_VX * LI_WB_DT / LI_WB_DX;
  const double cfy = LI_WB_VY * LI_WB_DT / LI_WB_DX;
  for (int step = 0; step < LI_WB_STEPS; ++step) {
    for (int j = 0; j < LI_WB_NY; ++j) {
      s.u[0][j] = LI_WB_INFLOW;
    }
    for (int i = 1; i < LI_WB_NX - 1; ++i) {
      for (int j = 1; j < LI_WB_NY - 1; ++j) {
        const double u_c = s.u[i][j];
        const double du_x =
            cfx > 0.0 ? u_c - s.u[i - 1][j] : s.u[i + 1][j] - u_c;
        const double du_y =
            cfy > 0.0 ? u_c - s.u[i][j - 1] : s.u[i][j + 1] - u_c;
        const double lap = s.u[i + 1][j] + s.u[i - 1][j] + s.u[i][j + 1] + s.u[i][j - 1]
                           - 4.0 * u_c;
        const double shear = 0.001 * (double)j;
        s.v[i][j] = u_c - (cfx + shear) * du_x - cfy * du_y + r * lap;
      }
    }
    for (int i = 1; i < LI_WB_NX - 1; ++i) {
      for (int j = 1; j < LI_WB_NY - 1; ++j) {
        s.u[i][j] = s.v[i][j];
      }
    }
    (void)step;
  }
  g_li_wind_field_bc_checksum = li_wb_sum(&s);
}

double li_wind_field_bc_checksum(void) { return g_li_wind_field_bc_checksum; }
