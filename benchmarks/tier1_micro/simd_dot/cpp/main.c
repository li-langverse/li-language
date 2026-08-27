#include "../common/dot_core.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  li_simd_dot_kernel();
  const double checksum = li_simd_dot_checksum();
  if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
    printf("%.17g\n", checksum);
    return 0;
  }
  (void)checksum;
  return 0;
}
