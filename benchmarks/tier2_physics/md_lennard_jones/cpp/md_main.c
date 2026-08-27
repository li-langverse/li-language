// C-only driver for md_lennard_jones (shared md_core.c — same kernel as Li).
#include "../common/md_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_traj_from_env(void) {
  const char* path = getenv("LI_MD_TRAJ");
  if (path == NULL || path[0] == '\0') {
    return 0;
  }
  int stride = LI_MD_TRACE_INTERVAL;
  int max_steps = LI_MD_STEPS;
  const char* stride_s = getenv("LI_MD_TRAJ_STRIDE");
  const char* steps_s = getenv("LI_MD_TRAJ_STEPS");
  if (stride_s != NULL && stride_s[0] != '\0') {
    stride = atoi(stride_s);
  }
  if (steps_s != NULL && steps_s[0] != '\0') {
    max_steps = atoi(steps_s);
  }
  return li_md_export_trajectory(path, stride, max_steps) == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
  if (run_traj_from_env() == 0 && getenv("LI_MD_TRAJ") != NULL) {
    return 0;
  }
  for (int i = 1; i + 1 < argc; ++i) {
    if (strcmp(argv[i], "--trace") == 0) {
      const double drift = li_md_run_trace(argv[i + 1]);
      if (argc > 1) {
        for (int j = 1; j < argc; ++j) {
          if (strcmp(argv[j], "--verify") == 0) {
            printf("%.17g\n", drift);
            return 0;
          }
        }
      }
      return 0;
    }
  }
  li_md_kernel();
  const double drift = li_md_checksum();
  if (argc > 1 && strcmp(argv[1], "--verify") == 0) {
    printf("%.17g\n", drift);
    return 0;
  }
  (void)drift;
  return 0;
}
