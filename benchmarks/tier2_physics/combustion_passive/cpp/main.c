#include <stdio.h>

extern void li_combustion_passive_kernel(void);
extern double li_combustion_passive_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_combustion_passive_kernel();
    printf("%.17g\n", li_combustion_passive_checksum());
    return 0;
  }
  li_combustion_passive_kernel();
  return 0;
}
