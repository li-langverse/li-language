#include "matmul_blocked_core.h"

enum {
  LI_MB_N = 512,
  LI_MB_BK = 64,
};

static double g_li_matmul_blocked_checksum;

__attribute__((noinline)) void li_matmul_blocked_kernel(void) {
  static double a[LI_MB_N][LI_MB_N];
  static double b[LI_MB_N][LI_MB_N];
  static double c[LI_MB_N][LI_MB_N];
  for (int i = 0; i < LI_MB_N; ++i) {
    for (int j = 0; j < LI_MB_N; ++j) {
      a[i][j] = (double)((i + j) % 17) * 0.01;
      b[i][j] = (double)((i * 3 + j) % 13) * 0.02;
      c[i][j] = 0.0;
    }
  }
  for (int ii = 0; ii < LI_MB_N; ii += LI_MB_BK) {
    for (int kk = 0; kk < LI_MB_N; kk += LI_MB_BK) {
      for (int jj = 0; jj < LI_MB_N; jj += LI_MB_BK) {
        const int i_max = ii + LI_MB_BK < LI_MB_N ? ii + LI_MB_BK : LI_MB_N;
        const int k_max = kk + LI_MB_BK < LI_MB_N ? kk + LI_MB_BK : LI_MB_N;
        const int j_max = jj + LI_MB_BK < LI_MB_N ? jj + LI_MB_BK : LI_MB_N;
        for (int i = ii; i < i_max; ++i) {
          for (int k = kk; k < k_max; ++k) {
            const double aik = a[i][k];
            for (int j = jj; j < j_max; ++j) {
              c[i][j] += aik * b[k][j];
            }
          }
        }
      }
    }
  }
  double acc = 0.0;
  for (int i = 0; i < LI_MB_N; ++i) {
    for (int j = 0; j < LI_MB_N; ++j) {
      acc += c[i][j];
    }
  }
  g_li_matmul_blocked_checksum = acc;
}

double li_matmul_blocked_checksum(void) { return g_li_matmul_blocked_checksum; }
