#include "../common/harmonic_core.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  li_harmonic_chain_kernel();
  const double checksum = li_harmonic_chain_checksum();
  if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
    printf("%.17g\n", checksum);
    return 0;
  }
  (void)checksum;
  return 0;
}
