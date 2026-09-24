#include "li_rt_lig.h"

static float g_ratio = 1.0f;

int32_t li_rt_lig_kernel_run(int32_t kid, int32_t bid) {
  (void)kid;
  (void)bid;
  g_ratio = 1.0f;
  return 0;
}

/* Li `float` is 64-bit (float64), so externs declared `-> float` must return
 * double here: a C `float` return would leave the f32 bits in the low half of
 * the f64 register (e.g. 1.0f read back as a 5.26e-315 denormal). */
double li_rt_lig_kernel_last_validity_ratio(void) { return (double)g_ratio; }
