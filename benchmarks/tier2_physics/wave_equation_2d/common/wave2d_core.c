#include "wave2d_core.h"

#include <math.h>
#include <string.h>

enum {
  LI_W2_NX = 128,
  LI_W2_NY = 128,
  LI_W2_STEPS = 25000,
};
#define LI_W2_C 1.0
#define LI_W2_DX 0.01
#define LI_W2_DT 0.004

typedef struct LiW2State {
  double u0[LI_W2_NX][LI_W2_NY];
  double u1[LI_W2_NX][LI_W2_NY];
  double u2[LI_W2_NX][LI_W2_NY];
} LiW2State;

static double g_li_wave2d_checksum;

static void li_w2_init(LiW2State* s) {
  const double cx = 0.5 * (double)(LI_W2_NX - 1) * LI_W2_DX;
  const double cy = 0.5 * (double)(LI_W2_NY - 1) * LI_W2_DX;
  const double width = 0.12;
  for (int i = 0; i < LI_W2_NX; ++i) {
    for (int j = 0; j < LI_W2_NY; ++j) {
      const double x = (double)i * LI_W2_DX;
      const double y = (double)j * LI_W2_DX;
      const double dx = (x - cx) / width;
      const double dy = (y - cy) / width;
      const double pulse = exp(-(dx * dx + dy * dy));
      s->u1[i][j] = pulse;
      s->u0[i][j] = pulse;
      s->u2[i][j] = pulse;
    }
  }
}

static double li_w2_energy(const LiW2State* s) {
  double e = 0.0;
  const double r2 = (LI_W2_C * LI_W2_DT / LI_W2_DX) * (LI_W2_C * LI_W2_DT / LI_W2_DX);
  for (int i = 1; i < LI_W2_NX - 1; ++i) {
    for (int j = 1; j < LI_W2_NY - 1; ++j) {
      const double v = (s->u1[i][j] - s->u0[i][j]) / LI_W2_DT;
      const double ux = (s->u1[i + 1][j] - s->u1[i - 1][j]) / (2.0 * LI_W2_DX);
      const double uy = (s->u1[i][j + 1] - s->u1[i][j - 1]) / (2.0 * LI_W2_DX);
      e += 0.5 * (v * v + LI_W2_C * LI_W2_C * (ux * ux + uy * uy));
    }
  }
  (void)r2;
  return e;
}

__attribute__((noinline)) void li_wave_2d_kernel(void) {
  LiW2State s;
  li_w2_init(&s);
  const double r2 = (LI_W2_C * LI_W2_DT / LI_W2_DX) * (LI_W2_C * LI_W2_DT / LI_W2_DX);
  for (int step = 0; step < LI_W2_STEPS; ++step) {
    for (int i = 1; i < LI_W2_NX - 1; ++i) {
      for (int j = 1; j < LI_W2_NY - 1; ++j) {
        s.u2[i][j] = 2.0 * s.u1[i][j] - s.u0[i][j]
                     + r2 * (s.u1[i + 1][j] - 2.0 * s.u1[i][j] + s.u1[i - 1][j]
                             + s.u1[i][j + 1] - 2.0 * s.u1[i][j] + s.u1[i][j - 1]);
      }
    }
    for (int i = 0; i < LI_W2_NX; ++i) {
      s.u2[i][0] = 0.0;
      s.u2[i][LI_W2_NY - 1] = 0.0;
      s.u0[i][0] = s.u0[i][LI_W2_NY - 1] = 0.0;
      s.u1[i][0] = s.u1[i][LI_W2_NY - 1] = 0.0;
    }
    for (int j = 0; j < LI_W2_NY; ++j) {
      s.u2[0][j] = 0.0;
      s.u2[LI_W2_NX - 1][j] = 0.0;
      s.u0[0][j] = s.u0[LI_W2_NX - 1][j] = 0.0;
      s.u1[0][j] = s.u1[LI_W2_NX - 1][j] = 0.0;
    }
    memcpy(s.u0, s.u1, sizeof(s.u0));
    memcpy(s.u1, s.u2, sizeof(s.u1));
    (void)step;
  }
  g_li_wave2d_checksum = li_w2_energy(&s);
}

double li_wave_2d_checksum(void) { return g_li_wave2d_checksum; }
