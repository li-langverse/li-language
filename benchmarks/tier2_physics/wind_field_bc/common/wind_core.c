enum { LI_WF_N = 64, LI_WF_STEPS = 1000 };
#define LI_WF_DT 0.01

static double g_li_wind_checksum;

void li_wind_field_kernel(void) {
  double u[LI_WF_N];
  for (int i = 0; i < LI_WF_N; ++i) {
    u[i] = 1.0;
  }
  for (int s = 0; s < LI_WF_STEPS; ++s) {
    for (int i = 1; i < LI_WF_N - 1; ++i) {
      u[i] = u[i] - LI_WF_DT * (u[i] - u[i - 1]);
    }
  }
  g_li_wind_checksum = u[LI_WF_N / 2];
}

double li_wind_field_checksum(void) { return g_li_wind_checksum; }
