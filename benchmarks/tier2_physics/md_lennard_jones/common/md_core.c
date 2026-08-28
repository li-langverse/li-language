#include "md_core.h"

static double g_li_md_checksum;

__attribute__((noinline)) void li_md_kernel(void) {
  g_li_md_checksum = li_md_run();
}

double li_md_checksum(void) { return g_li_md_checksum; }

void li_md_lj_run(void) { li_md_kernel(); }
