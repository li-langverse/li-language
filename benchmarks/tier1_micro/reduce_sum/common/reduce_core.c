#include "reduce_core.h"

#include <stdlib.h>

enum { LI_RED_N = 100000000 };

static double g_li_reduce_checksum;

__attribute__((noinline)) void li_reduce_sum_kernel(void) {
  double* a = (double*)malloc((size_t)LI_RED_N * sizeof(double));
  if (!a) {
    g_li_reduce_checksum = 0.0;
    return;
  }
  for (int i = 0; i < LI_RED_N; ++i) {
    a[i] = (double)(i & 1023) * 1e-6;
  }
  double acc = 0.0;
  for (int i = 0; i < LI_RED_N; ++i) {
    acc += a[i];
  }
  g_li_reduce_checksum = acc;
  free(a);
}

double li_reduce_sum_checksum(void) { return g_li_reduce_checksum; }
