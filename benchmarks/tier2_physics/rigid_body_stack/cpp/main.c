#include <stdio.h>

extern void li_rigid_stack_kernel(void);
extern double li_rigid_stack_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_rigid_stack_kernel();
    printf("%.17g\n", li_rigid_stack_checksum());
    return 0;
  }
  li_rigid_stack_kernel();
  return 0;
}
