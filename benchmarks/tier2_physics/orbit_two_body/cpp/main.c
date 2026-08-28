#include <stdio.h>

extern void li_orbit_two_body_kernel(void);
extern double li_orbit_two_body_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_orbit_two_body_kernel();
    printf("%.17g\n", li_orbit_two_body_checksum());
    return 0;
  }
  li_orbit_two_body_kernel();
  return 0;
}
