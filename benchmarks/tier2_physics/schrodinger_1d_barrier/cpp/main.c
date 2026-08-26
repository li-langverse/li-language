#include <stdio.h>

extern void li_schrodinger_1d_barrier_kernel(void);
extern double li_schrodinger_1d_barrier_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_schrodinger_1d_barrier_kernel();
    printf("%.17g\n", li_schrodinger_1d_barrier_checksum());
    return 0;
  }
  li_schrodinger_1d_barrier_kernel();
  return 0;
}
