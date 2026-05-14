#include "matmul_core.h"

enum { LI_MM_N = 256 };

static double g_li_matmul_checksum;

__attribute__((noinline)) void li_matmul_naive_kernel(void) {
  double a[LI_MM_N][LI_MM_N];
  double b[LI_MM_N][LI_MM_N];
  double c[LI_MM_N][LI_MM_N];
  for (int i = 0; i < LI_MM_N; ++i) {
    for (int j = 0; j < LI_MM_N; ++j) {
      a[i][j] = (double)((i + j) % 17) * 0.01;
      b[i][j] = (double)((i * 3 + j) % 13) * 0.02;
      c[i][j] = 0.0;
    }
  }
  for (int i = 0; i < LI_MM_N; ++i) {
    for (int k = 0; k < LI_MM_N; ++k) {
      const double aik = a[i][k];
      for (int j = 0; j < LI_MM_N; ++j) {
        c[i][j] += aik * b[k][j];
      }
    }
  }
  double acc = 0.0;
  for (int i = 0; i < LI_MM_N; ++i) {
    for (int j = 0; j < LI_MM_N; ++j) {
      acc += c[i][j];
    }
  }
  g_li_matmul_checksum = acc;
}

double li_matmul_naive_checksum(void) { return g_li_matmul_checksum; }
