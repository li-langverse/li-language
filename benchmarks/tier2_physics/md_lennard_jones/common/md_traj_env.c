#include "md_core.h"

#include <stdlib.h>

void li_md_traj_from_env(void) {
  const char* path = getenv("LI_MD_TRAJ");
  if (path != NULL && path[0] != '\0') {
    int stride = 25;
    int max_steps = LI_MD_STEPS;
    const char* stride_s = getenv("LI_MD_TRAJ_STRIDE");
    const char* steps_s = getenv("LI_MD_TRAJ_STEPS");
    if (stride_s != NULL && stride_s[0] != '\0') {
      stride = atoi(stride_s);
    }
    if (steps_s != NULL && steps_s[0] != '\0') {
      max_steps = atoi(steps_s);
    }
    li_md_export_trajectory(path, stride, max_steps);
    return;
  }
  li_md_kernel();
}
