#include "dot_core.h"

#include <stdlib.h>

enum { LI_DOT_N = 10000000 };

static double g_li_dot_checksum;

__attribute__((noinline)) void li_simd_dot_kernel(void) {
  double* a = (double*)malloc((size_t)LI_DOT_N * sizeof(double));
  double* b = (double*)malloc((size_t)LI_DOT_N * sizeof(double));
  if (!a || !b) {
    free(a);
    free(b);
    g_li_dot_checksum = 0.0;
    return;
  }
  for (int i = 0; i < LI_DOT_N; ++i) {
    a[i] = (double)(i & 255) * 0.001;
    b[i] = (double)((i * 7) & 255) * 0.002;
  }
  double acc = 0.0;
  for (int i = 0; i < LI_DOT_N; ++i) {
    acc += a[i] * b[i];
  }
  g_li_dot_checksum = acc;
  free(a);
  free(b);
}

double li_simd_dot_checksum(void) { return g_li_dot_checksum; }
