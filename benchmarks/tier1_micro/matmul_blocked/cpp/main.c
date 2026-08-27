#include "../common/matmul_blocked_core.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  li_matmul_blocked_kernel();
  const double checksum = li_matmul_blocked_checksum();
  if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
    printf("%.17g\n", checksum);
    return 0;
  }
  (void)checksum;
  return 0;
}
