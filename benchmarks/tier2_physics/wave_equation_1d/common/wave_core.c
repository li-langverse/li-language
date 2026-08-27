#include "wave_core.h"

#include <math.h>
#include <string.h>

enum { LI_WV_N = 8192, LI_WV_STEPS = 400000 };
#define LI_WV_C 1.0
#define LI_WV_DX 0.01
#define LI_WV_DT 0.004
#define LI_WV_R (LI_WV_C * LI_WV_DT / LI_WV_DX)

typedef struct LiWvState {
  double u0[LI_WV_N];
  double u1[LI_WV_N];
  double u2[LI_WV_N];
} LiWvState;

static double g_li_wave_checksum;

static void li_wv_init(LiWvState* s) {
  const double center = 0.5 * (double)(LI_WV_N - 1) * LI_WV_DX;
  const double width = 0.15;
  for (int i = 0; i < LI_WV_N; ++i) {
    const double x = (double)i * LI_WV_DX;
    const double d = (x - center) / width;
    s->u1[i] = exp(-d * d);
    s->u0[i] = s->u1[i];
    s->u2[i] = s->u1[i];
  }
  s->u0[0] = s->u0[LI_WV_N - 1] = 0.0;
  s->u1[0] = s->u1[LI_WV_N - 1] = 0.0;
}

static double li_wv_energy(const LiWvState* s) {
  double e = 0.0;
  const double r2 = LI_WV_R * LI_WV_R;
  for (int i = 1; i < LI_WV_N - 1; ++i) {
    const double v = (s->u1[i] - s->u0[i]) / LI_WV_DT;
    const double du = (s->u1[i + 1] - s->u1[i - 1]) / (2.0 * LI_WV_DX);
    e += 0.5 * (v * v + LI_WV_C * LI_WV_C * du * du);
  }
  (void)r2;
  return e;
}

__attribute__((noinline)) void li_wave_1d_kernel(void) {
  LiWvState s;
  li_wv_init(&s);
  const double r2 = LI_WV_R * LI_WV_R;
  for (int step = 0; step < LI_WV_STEPS; ++step) {
    for (int i = 1; i < LI_WV_N - 1; ++i) {
      s.u2[i] = 2.0 * s.u1[i] - s.u0[i] + r2 * (s.u1[i + 1] - 2.0 * s.u1[i] + s.u1[i - 1]);
    }
    s.u2[0] = 0.0;
    s.u2[LI_WV_N - 1] = 0.0;
    memcpy(s.u0, s.u1, sizeof(s.u0));
    memcpy(s.u1, s.u2, sizeof(s.u1));
    (void)step;
  }
  g_li_wave_checksum = li_wv_energy(&s);
}

double li_wave_1d_checksum(void) { return g_li_wave_checksum; }
