#include <stdio.h>

extern void li_cloth_swing_kernel(void);
extern double li_cloth_swing_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_cloth_swing_kernel();
    printf("%.17g\n", li_cloth_swing_checksum());
    return 0;
  }
  li_cloth_swing_kernel();
  return 0;
}
