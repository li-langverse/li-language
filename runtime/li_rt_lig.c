#include "li_rt_lig.h"

static float g_ratio = 1.0f;

int32_t li_rt_lig_kernel_run(int32_t kid, int32_t bid) {
  (void)kid;
  (void)bid;
  g_ratio = 1.0f;
  return 0;
}

float li_rt_lig_kernel_last_validity_ratio(void) { return g_ratio; }
