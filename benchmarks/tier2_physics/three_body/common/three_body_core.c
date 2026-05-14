#include "three_body_core.h"

#include <math.h>
#include <string.h>

enum {
  LI_TB_N = 3,
  LI_TB_STEPS = 10000000,
};
#define LI_TB_DT 0.01
#define LI_TB_G 1.0
#define LI_TB_SOFT 1e-9
#define LI_TB_MASS 1.0

typedef struct LiTbState {
  double px[LI_TB_N];
  double py[LI_TB_N];
  double vx[LI_TB_N];
  double vy[LI_TB_N];
  double fx[LI_TB_N];
  double fy[LI_TB_N];
} LiTbState;

static double g_li_three_body_checksum;

static void li_tb_init(LiTbState* s) {
  const double r = 1.0;
  s->px[0] = 0.0;
  s->py[0] = r;
  s->px[1] = -0.8660254037844386 * r;
  s->py[1] = -0.5 * r;
  s->px[2] = 0.8660254037844386 * r;
  s->py[2] = -0.5 * r;
  for (int i = 0; i < LI_TB_N; ++i) {
    s->vx[i] = 0.0;
    s->vy[i] = 0.0;
  }
}

static void li_tb_forces(const LiTbState* s, LiTbState* out) {
  memset(out->fx, 0, sizeof(out->fx));
  memset(out->fy, 0, sizeof(out->fy));
  const double eps2 = LI_TB_SOFT * LI_TB_SOFT;
  for (int i = 0; i < LI_TB_N; ++i) {
    for (int j = i + 1; j < LI_TB_N; ++j) {
      const double dx = s->px[j] - s->px[i];
      const double dy = s->py[j] - s->py[i];
      const double r2 = dx * dx + dy * dy + eps2;
      const double inv_r = 1.0 / sqrt(r2);
      const double inv_r3 = inv_r * inv_r * inv_r;
      const double scale = LI_TB_G * LI_TB_MASS * LI_TB_MASS * inv_r3;
      const double fx = scale * dx;
      const double fy = scale * dy;
      out->fx[i] += fx;
      out->fy[i] += fy;
      out->fx[j] -= fx;
      out->fy[j] -= fy;
    }
  }
}

static double li_tb_energy(const LiTbState* s) {
  double ke = 0.0;
  double pe = 0.0;
  const double eps2 = LI_TB_SOFT * LI_TB_SOFT;
  for (int i = 0; i < LI_TB_N; ++i) {
    ke += 0.5 * LI_TB_MASS * (s->vx[i] * s->vx[i] + s->vy[i] * s->vy[i]);
  }
  for (int i = 0; i < LI_TB_N; ++i) {
    for (int j = i + 1; j < LI_TB_N; ++j) {
      const double dx = s->px[j] - s->px[i];
      const double dy = s->py[j] - s->py[i];
      const double r = sqrt(dx * dx + dy * dy + eps2);
      pe -= LI_TB_G * LI_TB_MASS * LI_TB_MASS / r;
    }
  }
  return ke + pe;
}

__attribute__((noinline)) void li_three_body_kernel(void) {
  LiTbState s, f;
  li_tb_init(&s);
  li_tb_forces(&s, &f);
  for (int step = 0; step < LI_TB_STEPS; ++step) {
    for (int i = 0; i < LI_TB_N; ++i) {
      s.vx[i] += 0.5 * LI_TB_DT * f.fx[i] / LI_TB_MASS;
      s.vy[i] += 0.5 * LI_TB_DT * f.fy[i] / LI_TB_MASS;
    }
    for (int i = 0; i < LI_TB_N; ++i) {
      s.px[i] += LI_TB_DT * s.vx[i];
      s.py[i] += LI_TB_DT * s.vy[i];
    }
    li_tb_forces(&s, &f);
    for (int i = 0; i < LI_TB_N; ++i) {
      s.vx[i] += 0.5 * LI_TB_DT * f.fx[i] / LI_TB_MASS;
      s.vy[i] += 0.5 * LI_TB_DT * f.fy[i] / LI_TB_MASS;
    }
    (void)step;
  }
  g_li_three_body_checksum = li_tb_energy(&s);
}

double li_three_body_checksum(void) { return g_li_three_body_checksum; }
