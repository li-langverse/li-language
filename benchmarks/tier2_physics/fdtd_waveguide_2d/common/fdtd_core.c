enum { LI_FD_N = 32, LI_FD_STEPS = 2000 };
#define LI_FD_DT 0.001
#define LI_FD_DX 0.01
#define LI_FD_C 1.0

static double g_li_fdtd_checksum;

void li_fdtd_waveguide_kernel(void) {
  double ex[LI_FD_N];
  double hz[LI_FD_N];
  for (int i = 0; i < LI_FD_N; ++i) {
    ex[i] = 0.0;
    hz[i] = (i == LI_FD_N / 2) ? 1.0 : 0.0;
  }
  for (int s = 0; s < LI_FD_STEPS; ++s) {
    for (int i = 0; i < LI_FD_N - 1; ++i) {
      ex[i] += (LI_FD_DT / LI_FD_DX) * (hz[i + 1] - hz[i]);
    }
    for (int i = 1; i < LI_FD_N; ++i) {
      hz[i] += (LI_FD_DT / LI_FD_DX) * (ex[i] - ex[i - 1]);
    }
  }
  double e = 0.0;
  for (int i = 0; i < LI_FD_N; ++i) e += ex[i] * ex[i] + hz[i] * hz[i];
  g_li_fdtd_checksum = e;
}

double li_fdtd_waveguide_checksum(void) { return g_li_fdtd_checksum; }
