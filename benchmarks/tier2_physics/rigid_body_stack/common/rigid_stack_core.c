#include <math.h>

enum { LI_RS_N = 20, LI_RS_STEPS = 600 };
#define LI_RS_DT (1.0 / 60.0)
#define LI_RS_G 9.81

static double g_li_rigid_stack_checksum;

void li_rigid_stack_kernel(void) {
  double y[LI_RS_N];
  double v[LI_RS_N];
  for (int i = 0; i < LI_RS_N; ++i) {
    y[i] = 1.0 + (double)i * 0.1;
    v[i] = 0.0;
  }
  for (int s = 0; s < LI_RS_STEPS; ++s) {
    for (int i = 0; i < LI_RS_N; ++i) {
      v[i] += -LI_RS_G * LI_RS_DT;
      y[i] += v[i] * LI_RS_DT;
      if (y[i] < 0.0) {
        y[i] = 0.0;
        v[i] = 0.0;
      }
    }
  }
  g_li_rigid_stack_checksum = y[LI_RS_N - 1];
}

double li_rigid_stack_checksum(void) { return g_li_rigid_stack_checksum; }
