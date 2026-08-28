/* Studio viewport capture stub — draws grid + particles under SDL2 for Xvfb evidence.
 * Not the shipped li-studio binary; honest native_pixels probe for plan loop. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void draw_viewport(SDL_Renderer* r, int w, int h, int frame) {
  SDL_SetRenderDrawColor(r, 13, 17, 23, 255);
  SDL_RenderClear(r);
  SDL_SetRenderDrawColor(r, 48, 54, 61, 255);
  for (int x = 0; x < w; x += 64) {
    SDL_RenderDrawLine(r, x, 0, x, h);
  }
  for (int y = 0; y < h; y += 64) {
    SDL_RenderDrawLine(r, 0, y, w, y);
  }
  SDL_SetRenderDrawColor(r, 56, 189, 248, 255);
  for (int i = 0; i < 120; i++) {
    int px = (i * 37 + frame * 11) % (w - 40) + 20;
    int py = (i * 53 + frame * 7) % (h - 40) + 20;
    SDL_Rect dot = {px, py, 4, 4};
    SDL_RenderFillRect(r, &dot);
  }
  SDL_SetRenderDrawColor(r, 251, 146, 60, 255);
  SDL_Rect sel = {w / 4, h / 4, w / 2, h / 2};
  SDL_RenderDrawRect(r, &sel);
  SDL_SetRenderDrawColor(r, 34, 197, 94, 200);
  SDL_Rect hud = {12, 12, 220, 28};
  SDL_RenderFillRect(r, &hud);
}

static int save_ppm(SDL_Renderer* r, int w, int h, const char* path) {
  Uint32* pixels = NULL;
  int pitch = 0;
  if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ABGR8888, NULL, 0) != 0) {
    return -1;
  }
  pixels = (Uint32*)malloc((size_t)w * (size_t)h * sizeof(Uint32));
  if (!pixels) {
    return -1;
  }
  if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_ABGR8888, pixels, w * 4) != 0) {
    free(pixels);
    return -1;
  }
  FILE* f = fopen(path, "wb");
  if (!f) {
    free(pixels);
    return -1;
  }
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      Uint32 p = pixels[y * w + x];
      Uint8 b = (Uint8)(p & 0xff);
      Uint8 g = (Uint8)((p >> 8) & 0xff);
      Uint8 rv = (Uint8)((p >> 16) & 0xff);
      fputc(rv, f);
      fputc(g, f);
      fputc(b, f);
    }
  }
  fclose(f);
  free(pixels);
  return 0;
}

int main(int argc, char** argv) {
  const char* out_dir = ".";
  int width = 1280;
  int height = 720;
  int frames = 3;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
      out_dir = argv[++i];
    } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
      width = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
      height = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
      frames = atoi(argv[++i]);
    }
  }
  if (frames < 1) {
    frames = 1;
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 2;
  }
  SDL_Window* win =
      SDL_CreateWindow("Li Studio viewport capture", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_HIDDEN);
  if (!win) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 2;
  }
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  if (!ren) {
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!ren) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 2;
  }
  int saved = 0;
  for (int f = 0; f < frames; f++) {
    draw_viewport(ren, width, height, f);
    SDL_RenderPresent(ren);
    SDL_Delay(50);
    char path[512];
    snprintf(path, sizeof(path), "%s/frame-%03d.ppm", out_dir, f);
    if (save_ppm(ren, width, height, path) == 0) {
      saved++;
    }
  }
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  printf("{\"native_pixels\":%s,\"frames_saved\":%d,\"width\":%d,\"height\":%d,\"out\":\"%s\"}\n",
         saved > 0 ? "true" : "false", saved, width, height, out_dir);
  return saved > 0 ? 0 : 3;
}
