#include "cloth_core.h"

#include <math.h>
#include <string.h>

/* Mass–spring cloth (semi-implicit Euler): pinned top corners. */
enum {
  LI_CL_NW = 24,
  LI_CL_NH = 10,
  LI_CL_STEPS = 35000,
};
#define LI_CL_DT 0.0005
#define LI_CL_G 9.81
#define LI_CL_KS 180.0
#define LI_CL_MASS 0.02
#define LI_CL_REST 0.04

typedef struct LiClPt {
  double x[2];
  double vx[2];
} LiClPt;

static double g_li_cloth_swing_checksum;

static int li_cl_idx(int i, int j) { return j * LI_CL_NW + i; }

static int li_cl_pinned(int k) {
  return k == li_cl_idx(0, 0) || k == li_cl_idx(LI_CL_NW - 1, 0);
}

static void li_cl_init(LiClPt* p) {
  const int n = LI_CL_NW * LI_CL_NH;
  for (int k = 0; k < n; ++k) {
    const int j = k / LI_CL_NW;
    const int i = k % LI_CL_NW;
    p[k].x[0] = (double)i * LI_CL_REST;
    p[k].x[1] = 1.0 - (double)j * LI_CL_REST;
    p[k].vx[0] = 0.0;
    p[k].vx[1] = 0.0;
  }
}

static double li_cl_sum_y(const LiClPt* p) {
  double acc = 0.0;
  const int n = LI_CL_NW * LI_CL_NH;
  for (int k = 0; k < n; ++k) {
    acc += p[k].x[1];
  }
  return acc;
}

static void li_cl_apply_spring(const LiClPt* p, double fx[], double fy[], int ia, int ib) {
  const double dx = p[ib].x[0] - p[ia].x[0];
  const double dy = p[ib].x[1] - p[ia].x[1];
  const double len = sqrt(dx * dx + dy * dy) + 1e-12;
  const double stretch = len - LI_CL_REST;
  const double sx = LI_CL_KS * stretch * (dx / len);
  const double sy = LI_CL_KS * stretch * (dy / len);
  fx[ia] += sx;
  fy[ia] += sy;
  fx[ib] -= sx;
  fy[ib] -= sy;
}

__attribute__((noinline)) void li_cloth_swing_kernel(void) {
  LiClPt pts[LI_CL_NW * LI_CL_NH];
  li_cl_init(pts);
  const int n = LI_CL_NW * LI_CL_NH;
  double fx[LI_CL_NW * LI_CL_NH];
  double fy[LI_CL_NW * LI_CL_NH];
  for (int step = 0; step < LI_CL_STEPS; ++step) {
    memset(fx, 0, sizeof(fx));
    memset(fy, 0, sizeof(fy));
    for (int j = 0; j < LI_CL_NH; ++j) {
      for (int i = 0; i < LI_CL_NW; ++i) {
        if (i + 1 < LI_CL_NW) {
          li_cl_apply_spring(pts, fx, fy, li_cl_idx(i, j), li_cl_idx(i + 1, j));
        }
        if (j + 1 < LI_CL_NH) {
          li_cl_apply_spring(pts, fx, fy, li_cl_idx(i, j), li_cl_idx(i, j + 1));
        }
      }
    }
    for (int k = 0; k < n; ++k) {
      if (li_cl_pinned(k)) {
        pts[k].vx[0] = 0.0;
        pts[k].vx[1] = 0.0;
        continue;
      }
      fy[k] -= LI_CL_MASS * LI_CL_G;
      pts[k].vx[0] += (fx[k] / LI_CL_MASS) * LI_CL_DT;
      pts[k].vx[1] += (fy[k] / LI_CL_MASS) * LI_CL_DT;
      pts[k].x[0] += pts[k].vx[0] * LI_CL_DT;
      pts[k].x[1] += pts[k].vx[1] * LI_CL_DT;
    }
    (void)step;
  }
  g_li_cloth_swing_checksum = li_cl_sum_y(pts);
}

double li_cloth_swing_checksum(void) { return g_li_cloth_swing_checksum; }
