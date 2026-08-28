#include <stdio.h>

extern void li_ragdoll_chain_kernel(void);
extern double li_ragdoll_chain_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_ragdoll_chain_kernel();
    printf("%.17g\n", li_ragdoll_chain_checksum());
    return 0;
  }
  li_ragdoll_chain_kernel();
  return 0;
}
