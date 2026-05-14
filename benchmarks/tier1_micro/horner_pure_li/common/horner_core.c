#include "horner_core.h"

enum { LI_HORNER_STEPS = 5000000 };
#define LI_HORNER_X 1.1

static double g_li_horner_checksum;

__attribute__((noinline)) void li_horner_kernel(void) {
  double x = LI_HORNER_X;
  double acc = 0.0;
  for (int i = 0; i < LI_HORNER_STEPS; ++i) {
    acc = acc * x + 1.0;
    (void)i;
  }
  g_li_horner_checksum = acc;
}

double li_horner_checksum(void) { return g_li_horner_checksum; }
