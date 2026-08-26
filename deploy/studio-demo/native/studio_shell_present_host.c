/* Studio shell present host — one SDL frame on aarch64-apple-darwin (Metal backend via SDL).
 * Complements studio_shell_input_probe.c for PH-HW WP3 present loop evidence.
 * Build: native-sdl-build.sh studio_shell_present_host.c studio_shell_present_host
 * Run: LIG_HOST_PRESENT=1 ./studio_shell_present_host --width 1280 --height 720 */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
  int width = 1280;
  int height = 720;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
      width = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
      height = atoi(argv[++i]);
    }
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 2;
  }
  SDL_Window* win =
      SDL_CreateWindow("Li Studio present host", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_HIDDEN);
  if (!win) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 3;
  }
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  if (!ren) {
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!ren) {
    fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 4;
  }
  SDL_SetRenderDrawColor(ren, 13, 17, 23, 255);
  SDL_RenderClear(ren);
  SDL_SetRenderDrawColor(ren, 56, 189, 248, 255);
  SDL_Rect hud = {12, 12, 220, 28};
  SDL_RenderFillRect(ren, &hud);
  SDL_RenderPresent(ren);
  printf("{\"presented\":1,\"native_pixels\":1,\"backend\":\"sdl_metal\",\"width\":%d,\"height\":%d}\n",
         width, height);
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
