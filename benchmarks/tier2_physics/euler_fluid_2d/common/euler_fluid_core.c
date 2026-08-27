#include <math.h>

enum { LI_EF_N = 64, LI_EF_STEPS = 2000 };
#define LI_EF_DT 0.001
#define LI_EF_DX 0.05
#define LI_EF_C 0.5

static double g_li_euler_fluid_checksum;

void li_euler_fluid_2d_kernel(void) {
  double u[LI_EF_N];
  double un[LI_EF_N];
  for (int i = 0; i < LI_EF_N; ++i) {
    u[i] = 0.5 + 0.5 * sin(0.2 * (double)i);
    un[i] = u[i];
  }
  for (int s = 0; s < LI_EF_STEPS; ++s) {
    for (int i = 1; i < LI_EF_N - 1; ++i) {
      un[i] = u[i] - LI_EF_C * LI_EF_DT / LI_EF_DX * (u[i] - u[i - 1]);
    }
    for (int i = 0; i < LI_EF_N; ++i) {
      u[i] = un[i];
    }
  }
  g_li_euler_fluid_checksum = u[LI_EF_N / 2];
}

double li_euler_fluid_2d_checksum(void) { return g_li_euler_fluid_checksum; }
