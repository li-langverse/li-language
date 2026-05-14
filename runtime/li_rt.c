#include "li_rt.h"

void li_panic(const char* msg) {
  fprintf(stderr, "li panic: %s\n", msg);
  abort();
}

void li_bounds_fail(void) { li_panic("array index out of bounds"); }

void li_rt_print_int(int32_t value) { printf("%d\n", value); }

void li_rt_print_str(const char* s) { puts(s); }

static int li_argc = 0;
static char** li_argv = NULL;

void li_rt_set_args(int argc, char** argv) {
  li_argc = argc;
  li_argv = argv;
}

int li_rt_argc(void) { return li_argc; }

const char* li_rt_argv(int index) {
  if (index < 0 || index >= li_argc || li_argv == NULL) {
    return "";
  }
  return li_argv[index];
}
