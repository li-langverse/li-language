#include <stdio.h>
#include <string.h>

#include "../common/md_stress.h"

static void print_result(const LiMdStressResult* r) {
  printf("%s,%.12e,%.12e,%d\n", r->name, r->value, r->threshold, r->passed);
}

int main(int argc, char** argv) {
  if (argc >= 2 && strcmp(argv[1], "--all") == 0) {
    LiMdStressResult results[8];
    const int n = li_md_stress_run_all(results, 8);
    printf("name,value,threshold,passed\n");
    for (int i = 0; i < n; ++i) {
      print_result(&results[i]);
    }
    for (int i = 0; i < n; ++i) {
      if (!results[i].passed) return 1;
    }
    return 0;
  }
  if (argc >= 2 && strcmp(argv[1], "--harmonic") == 0) {
    LiMdStressResult r = li_md_stress_harmonic(0.004, 100000);
    print_result(&r);
    return r.passed ? 0 : 1;
  }
  if (argc >= 2 && strcmp(argv[1], "--nve-msd") == 0) {
    LiMdStressResult r = li_md_stress_nve_energy_msd(0.004, 40000);
    print_result(&r);
    return r.passed ? 0 : 1;
  }
  if (argc >= 2 && strcmp(argv[1], "--halving") == 0) {
    LiMdStressResult r = li_md_stress_timestep_halving(0.004, 20000);
    print_result(&r);
    return r.passed ? 0 : 1;
  }
  if (argc >= 2 && strcmp(argv[1], "--momentum") == 0) {
    LiMdStressResult r = li_md_stress_momentum(0.004, 10000);
    print_result(&r);
    return r.passed ? 0 : 1;
  }
  fprintf(stderr, "usage: md_stress --all | --harmonic | --nve-msd | --halving | --momentum\n");
  return 2;
}
