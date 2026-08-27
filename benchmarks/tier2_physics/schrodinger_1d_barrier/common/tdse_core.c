#include <math.h>

enum { LI_TDSE_N = 64, LI_TDSE_STEPS = 4000 };
#define LI_TDSE_DT 0.0001
#define LI_TDSE_DX 0.1

static double g_li_tdse_checksum;

void li_schrodinger_1d_barrier_kernel(void) {
  double re[LI_TDSE_N];
  double im[LI_TDSE_N];
  for (int i = 0; i < LI_TDSE_N; ++i) {
    re[i] = exp(-0.5 * ((i - LI_TDSE_N / 2) * 0.15) * ((i - LI_TDSE_N / 2) * 0.15));
    im[i] = 0.0;
  }
  for (int s = 0; s < LI_TDSE_STEPS; ++s) {
    for (int i = 1; i < LI_TDSE_N - 1; ++i) {
      double lap = re[i + 1] - 2.0 * re[i] + re[i - 1];
      re[i] += LI_TDSE_DT * lap;
      im[i] += LI_TDSE_DT * lap;
    }
  }
  double n2 = 0.0;
  for (int i = 0; i < LI_TDSE_N; ++i) n2 += re[i] * re[i] + im[i] * im[i];
  g_li_tdse_checksum = n2;
}

double li_schrodinger_1d_barrier_checksum(void) { return g_li_tdse_checksum; }
