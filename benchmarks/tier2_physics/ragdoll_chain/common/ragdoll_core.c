#include <math.h>

enum { LI_RG_JOINTS = 12, LI_RG_STEPS = 3600 };
#define LI_RG_DT (1.0 / 60.0)
#define LI_RG_LEN 0.35

static double g_li_ragdoll_checksum;

void li_ragdoll_chain_kernel(void) {
  double px[LI_RG_JOINTS];
  double py[LI_RG_JOINTS];
  double vx[LI_RG_JOINTS];
  double vy[LI_RG_JOINTS];
  for (int i = 0; i < LI_RG_JOINTS; ++i) {
    px[i] = 0.0;
    py[i] = 2.0 - (double)i * LI_RG_LEN;
    vx[i] = 0.0;
    vy[i] = 0.0;
  }
  px[0] = 0.0;
  py[0] = 2.0;
  for (int s = 0; s < LI_RG_STEPS; ++s) {
    for (int i = 1; i < LI_RG_JOINTS; ++i) {
      vy[i] += -9.81 * LI_RG_DT;
      px[i] += vx[i] * LI_RG_DT;
      py[i] += vy[i] * LI_RG_DT;
      double dx = px[i] - px[i - 1];
      double dy = py[i] - py[i - 1];
      double len = sqrt(dx * dx + dy * dy);
      if (len > 1e-12) {
        double scale = LI_RG_LEN / len;
        px[i] = px[i - 1] + dx * scale;
        py[i] = py[i - 1] + dy * scale;
      }
    }
  }
  g_li_ragdoll_checksum = py[LI_RG_JOINTS - 1];
}

double li_ragdoll_chain_checksum(void) { return g_li_ragdoll_checksum; }
