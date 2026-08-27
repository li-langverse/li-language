enum { LI_CB_N = 32, LI_CB_STEPS = 500 };
#define LI_CB_DT 0.02
#define LI_CB_BURN 0.1

static double g_li_combust_checksum;

void li_combustion_passive_kernel(void) {
  double fuel[LI_CB_N];
  double temp[LI_CB_N];
  for (int i = 0; i < LI_CB_N; ++i) {
    fuel[i] = 1.0;
    temp[i] = 300.0;
  }
  for (int s = 0; s < LI_CB_STEPS; ++s) {
    for (int i = 0; i < LI_CB_N; ++i) {
      double burned = LI_CB_BURN * LI_CB_DT * fuel[i];
      if (burned > fuel[i]) burned = fuel[i];
      fuel[i] -= burned;
      temp[i] += burned * 100.0;
    }
  }
  g_li_combust_checksum = temp[0];
}

double li_combustion_passive_checksum(void) { return g_li_combust_checksum; }
