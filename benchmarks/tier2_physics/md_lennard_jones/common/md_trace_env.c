#include "md_core.h"

#include <stdlib.h>

void li_md_trace_from_env(void) {
  const char* path = getenv("LI_MD_TRACE");
  if (path != NULL && path[0] != '\0') {
    li_md_run_trace(path);
    return;
  }
  li_md_kernel();
}
