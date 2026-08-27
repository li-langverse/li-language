#include "heat_core.h"

#include <math.h>
#include <string.h>

enum {
  LI_HT_NX = 128,
  LI_HT_NY = 128,
  LI_HT_STEPS = 20000,
};
#define LI_HT_ALPHA 0.25
#define LI_HT_DX 0.01
#define LI_HT_DT 0.0001

typedef struct LiHtState {
  double u[LI_HT_NX][LI_HT_NY];
  double v[LI_HT_NX][LI_HT_NY];
} LiHtState;

static double g_li_heat_checksum;

static void li_ht_init(LiHtState* s) {
  for (int i = 0; i < LI_HT_NX; ++i) {
    for (int j = 0; j < LI_HT_NY; ++j) {
      const double x = (double)i * LI_HT_DX;
      const double y = (double)j * LI_HT_DX;
      s->u[i][j] = sin(3.141592653589793 * x) * sin(3.141592653589793 * y);
    }
  }
}

static double li_ht_sum(const LiHtState* s) {
  double acc = 0.0;
  for (int i = 0; i < LI_HT_NX; ++i) {
    for (int j = 0; j < LI_HT_NY; ++j) {
      acc += s->u[i][j];
    }
  }
  return acc;
}

__attribute__((noinline)) void li_heat_2d_kernel(void) {
  LiHtState s;
  li_ht_init(&s);
  const double r = LI_HT_ALPHA * LI_HT_DT / (LI_HT_DX * LI_HT_DX);
  for (int step = 0; step < LI_HT_STEPS; ++step) {
    for (int i = 1; i < LI_HT_NX - 1; ++i) {
      for (int j = 1; j < LI_HT_NY - 1; ++j) {
        s.v[i][j] = s.u[i][j]
                    + r * (s.u[i + 1][j] + s.u[i - 1][j] + s.u[i][j + 1] + s.u[i][j - 1]
                           - 4.0 * s.u[i][j]);
      }
    }
    for (int i = 1; i < LI_HT_NX - 1; ++i) {
      for (int j = 1; j < LI_HT_NY - 1; ++j) {
        s.u[i][j] = s.v[i][j];
      }
    }
    (void)step;
  }
  g_li_heat_checksum = li_ht_sum(&s);
}

double li_heat_2d_checksum(void) { return g_li_heat_checksum; }
