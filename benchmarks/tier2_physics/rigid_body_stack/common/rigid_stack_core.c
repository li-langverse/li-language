#include "rigid_stack_core.h"

#include <math.h>
#include <string.h>

/* Stacked AABBs with gravity, floor, and pairwise separation (game-physics proxy). */
enum { LI_RB_N = 12, LI_RB_STEPS = 400000 };
#define LI_RB_DT 0.002
#define LI_RB_G 9.81
#define LI_RB_FLOOR 0.0

typedef struct LiRbBody {
  double x;
  double y;
  double vx;
  double vy;
  double hw;
  double hh;
} LiRbBody;

static double g_li_rigid_body_stack_checksum;

static void li_rb_init(LiRbBody* b) {
  for (int i = 0; i < LI_RB_N; ++i) {
    b[i].hw = 0.08;
    b[i].hh = 0.06;
    b[i].x = 0.5;
    b[i].y = LI_RB_FLOOR + b[i].hh + (double)i * (2.0 * b[i].hh + 0.01);
    b[i].vx = 0.0;
    b[i].vy = 0.0;
  }
}

static double li_rb_sum_y(const LiRbBody* b) {
  double acc = 0.0;
  for (int i = 0; i < LI_RB_N; ++i) {
    acc += b[i].y;
  }
  return acc;
}

static void li_rb_resolve(LiRbBody* a, LiRbBody* b) {
  const double dx = b->x - a->x;
  const double dy = b->y - a->y;
  const double overlap_x = (a->hw + b->hw) - fabs(dx);
  const double overlap_y = (a->hh + b->hh) - fabs(dy);
  if (overlap_x > 0.0 && overlap_y > 0.0) {
    if (overlap_x < overlap_y) {
      const double push = 0.5 * overlap_x * (dx >= 0.0 ? 1.0 : -1.0);
      a->x -= push;
      b->x += push;
      const double tmp = a->vx;
      a->vx = b->vx;
      b->vx = tmp;
    } else {
      const double push = 0.5 * overlap_y * (dy >= 0.0 ? 1.0 : -1.0);
      a->y -= push;
      b->y += push;
      const double tmp = a->vy;
      a->vy = b->vy;
      b->vy = tmp;
    }
  }
}

__attribute__((noinline)) void li_rigid_body_stack_kernel(void) {
  LiRbBody bodies[LI_RB_N];
  li_rb_init(bodies);
  for (int step = 0; step < LI_RB_STEPS; ++step) {
    for (int i = 0; i < LI_RB_N; ++i) {
      bodies[i].vy -= LI_RB_G * LI_RB_DT;
      bodies[i].x += bodies[i].vx * LI_RB_DT;
      bodies[i].y += bodies[i].vy * LI_RB_DT;
      if (bodies[i].y - bodies[i].hh < LI_RB_FLOOR) {
        bodies[i].y = LI_RB_FLOOR + bodies[i].hh;
        bodies[i].vy = -0.35 * bodies[i].vy;
      }
    }
    for (int i = 0; i < LI_RB_N; ++i) {
      for (int j = i + 1; j < LI_RB_N; ++j) {
        li_rb_resolve(&bodies[i], &bodies[j]);
      }
    }
    (void)step;
  }
  g_li_rigid_body_stack_checksum = li_rb_sum_y(bodies);
}

double li_rigid_body_stack_checksum(void) { return g_li_rigid_body_stack_checksum; }
