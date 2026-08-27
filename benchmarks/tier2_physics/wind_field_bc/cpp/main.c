#include <stdio.h>

extern void li_wind_field_kernel(void);
extern double li_wind_field_checksum(void);

int main(int argc, char** argv) {
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == '-') {
    li_wind_field_kernel();
    printf("%.17g\n", li_wind_field_checksum());
    return 0;
  }
  li_wind_field_kernel();
  return 0;
}
