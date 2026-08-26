#include <stdio.h>

extern void li_euler_fluid_2d_kernel(void);
extern double li_euler_fluid_2d_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_euler_fluid_2d_kernel();
    printf("%.17g\n", li_euler_fluid_2d_checksum());
    return 0;
  }
  li_euler_fluid_2d_kernel();
  return 0;
}
