// C-only driver for md_lennard_jones (fair baseline vs Li).
#include "../common/md_core.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  li_md_kernel();
  const double drift = li_md_checksum();
  if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
    printf("%.17g\n", drift);
    return 0;
  }
  (void)drift;
  return 0;
}
