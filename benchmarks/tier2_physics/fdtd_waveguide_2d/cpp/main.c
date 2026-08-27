#include <stdio.h>

extern void li_fdtd_waveguide_kernel(void);
extern double li_fdtd_waveguide_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_fdtd_waveguide_kernel();
    printf("%.17g\n", li_fdtd_waveguide_checksum());
    return 0;
  }
  li_fdtd_waveguide_kernel();
  return 0;
}
