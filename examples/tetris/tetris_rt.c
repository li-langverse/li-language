#include "tetris_rt.h"

#include <SDL.h>
#include <stdlib.h>

struct TetrisCtx {
  SDL_Window* window;
  SDL_Renderer* renderer;
  int width_px;
  int height_px;
};

TetrisCtx* tetris_open(int width_px, int height_px) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    return NULL;
  }
  TetrisCtx* ctx = (TetrisCtx*)calloc(1, sizeof(TetrisCtx));
  if (!ctx) {
    return NULL;
  }
  ctx->width_px = width_px;
  ctx->height_px = height_px;
  ctx->window = SDL_CreateWindow("Li Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 width_px, height_px, SDL_WINDOW_SHOWN);
  if (!ctx->window) {
    free(ctx);
    return NULL;
  }
  ctx->renderer = SDL_CreateRenderer(ctx->window, -1, SDL_RENDERER_ACCELERATED);
  if (!ctx->renderer) {
    SDL_DestroyWindow(ctx->window);
    free(ctx);
    return NULL;
  }
  return ctx;
}

void tetris_close(TetrisCtx* ctx) {
  if (!ctx) {
    return;
  }
  if (ctx->renderer) {
    SDL_DestroyRenderer(ctx->renderer);
  }
  if (ctx->window) {
    SDL_DestroyWindow(ctx->window);
  }
  free(ctx);
  SDL_Quit();
}

uint32_t tetris_ticks(void) { return SDL_GetTicks(); }

void tetris_delay(int ms) { SDL_Delay((Uint32)ms); }

int tetris_poll_key(TetrisCtx* ctx) {
  (void)ctx;
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    if (ev.type == SDL_QUIT) {
      return -1;
    }
    if (ev.type == SDL_KEYDOWN) {
      return (int)ev.key.keysym.sym;
    }
  }
  return 0;
}

void tetris_begin_frame(TetrisCtx* ctx) {
  SDL_SetRenderDrawColor(ctx->renderer, 12, 12, 18, 255);
  SDL_RenderClear(ctx->renderer);
}

static void color_rgb(int color, Uint8* r, Uint8* g, Uint8* b) {
  switch (color) {
    case 1:
      *r = 0;
      *g = 240;
      *b = 240;
      break;
    case 2:
      *r = 240;
      *g = 240;
      *b = 0;
      break;
    case 3:
      *r = 160;
      *g = 0;
      *b = 240;
      break;
    case 4:
      *r = 0;
      *g = 240;
      *b = 0;
      break;
    case 5:
      *r = 240;
      *g = 0;
      *b = 0;
      break;
    case 6:
      *r = 0;
      *g = 0;
      *b = 240;
      break;
    case 7:
      *r = 240;
      *g = 160;
      *b = 0;
      break;
    default:
      *r = 40;
      *g = 40;
      *b = 48;
      break;
  }
}

void tetris_draw_cell(TetrisCtx* ctx, int col, int row, int color, int cell_px) {
  const int pad = 1;
  SDL_Rect rect = {col * cell_px + pad, row * cell_px + pad, cell_px - pad * 2,
                   cell_px - pad * 2};
  Uint8 r = 0;
  Uint8 g = 0;
  Uint8 b = 0;
  color_rgb(color, &r, &g, &b);
  SDL_SetRenderDrawColor(ctx->renderer, r, g, b, 255);
  SDL_RenderFillRect(ctx->renderer, &rect);
}

void tetris_end_frame(TetrisCtx* ctx) { SDL_RenderPresent(ctx->renderer); }
