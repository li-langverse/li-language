/* One-shot CLI for li_rt_httpd_explain_config (CI golden vs Python). */
#include "li_rt.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: httpd-explain <config.toml>\n");
    return 2;
  }
  if (li_rt_httpd_explain_config(argv[1]) != 0) {
    fprintf(stderr, "config error: failed to load or validate %s\n", argv[1]);
    return 1;
  }
  return 0;
}
