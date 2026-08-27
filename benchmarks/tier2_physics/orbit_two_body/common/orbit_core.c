#include <math.h>

enum { LI_ORB_STEPS = 100000 };
#define LI_ORB_DT 0.001
#define LI_ORB_MU 1.0
#define LI_ORB_R 1.0

static double g_li_orbit_checksum;

void li_orbit_two_body_kernel(void) {
  double px = LI_ORB_R;
  double py = 0.0;
  double vx = 0.0;
  double vy = sqrt(LI_ORB_MU / LI_ORB_R);
  for (int s = 0; s < LI_ORB_STEPS; ++s) {
    double rx = -LI_ORB_MU * px / (px * px + py * py + 1e-18);
    double ry = -LI_ORB_MU * py / (px * px + py * py + 1e-18);
    vx += rx * LI_ORB_DT;
    vy += ry * LI_ORB_DT;
    px += vx * LI_ORB_DT;
    py += vy * LI_ORB_DT;
  }
  g_li_orbit_checksum = px;
}

double li_orbit_two_body_checksum(void) { return g_li_orbit_checksum; }
