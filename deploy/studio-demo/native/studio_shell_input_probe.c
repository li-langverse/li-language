/* Studio shell keyboard ingest probe — SDL2 poll or argv mock → InputState JSON.
 * Matches li-ui InputState: pointer_*, key_escape, key_cmd_k, key_digit (0–5).
 * Not the shipped li-studio binary; honest UX-09 host bridge evidence. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int pointer_down;
  float pointer_x;
  float pointer_y;
  int key_escape;
  int key_cmd_k;
  int key_digit;
  int mock;
} InputStateJson;

static void input_state_reset(InputStateJson* s) {
  memset(s, 0, sizeof(*s));
}

static int parse_mock_token(InputStateJson* s, const char* tok) {
  if (!tok || !*tok) {
    return 0;
  }
  if (strcmp(tok, "escape") == 0 || strcmp(tok, "key_escape") == 0) {
    s->key_escape = 1;
    return 1;
  }
  if (strcmp(tok, "cmd_k") == 0 || strcmp(tok, "key_cmd_k") == 0) {
    s->key_cmd_k = 1;
    return 1;
  }
  if (strncmp(tok, "digit=", 6) == 0) {
    int d = atoi(tok + 6);
    if (d >= 1 && d <= 5) {
      s->key_digit = d;
      return 1;
    }
    return 0;
  }
  if (strncmp(tok, "key_digit=", 10) == 0) {
    int d = atoi(tok + 10);
    if (d >= 1 && d <= 5) {
      s->key_digit = d;
      return 1;
    }
    return 0;
  }
  return 0;
}

static int apply_mock_arg(InputStateJson* s, const char* spec) {
  char buf[256];
  size_t n = strlen(spec);
  if (n >= sizeof(buf)) {
    return -1;
  }
  memcpy(buf, spec, n + 1);
  s->mock = 1;
  char* save = NULL;
  for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
    while (*tok == ' ') {
      tok++;
    }
    if (!parse_mock_token(s, tok)) {
      return -1;
    }
  }
  return 0;
}

static int apply_mock_flags(InputStateJson* s, int argc, char** argv) {
  s->mock = 1;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--key-escape") == 0) {
      s->key_escape = 1;
    } else if (strcmp(argv[i], "--key-cmd-k") == 0) {
      s->key_cmd_k = 1;
    } else if (strcmp(argv[i], "--key-digit") == 0 && i + 1 < argc) {
      int d = atoi(argv[++i]);
      if (d < 1 || d > 5) {
        return -1;
      }
      s->key_digit = d;
    }
  }
  return 0;
}

static void map_sdl_keys(const Uint8* keys, InputStateJson* s) {
  if (keys[SDL_SCANCODE_ESCAPE]) {
    s->key_escape = 1;
  }
  SDL_Keymod mod = SDL_GetModState();
  int mod_ok = (mod & KMOD_GUI) || (mod & KMOD_CTRL);
  if (mod_ok && keys[SDL_SCANCODE_K]) {
    s->key_cmd_k = 1;
  }
  static const SDL_Scancode digit_sc[] = {
      SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
      SDL_SCANCODE_4, SDL_SCANCODE_5,
  };
  for (int d = 0; d < 5; d++) {
    if (keys[digit_sc[d]]) {
      s->key_digit = d + 1;
      break;
    }
  }
}

static int poll_sdl_once(InputStateJson* s, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 2;
  }
  SDL_Window* win = SDL_CreateWindow(
      "Li Studio input probe", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      width, height, SDL_WINDOW_HIDDEN);
  if (!win) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 2;
  }
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (!ren) {
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 2;
  }
  SDL_Event ev;
  while (SDL_PollEvent(&ev)) {
    (void)ev;
  }
  const Uint8* keys = SDL_GetKeyboardState(NULL);
  map_sdl_keys(keys, s);
  int mx = 0, my = 0;
  Uint32 buttons = SDL_GetMouseState(&mx, &my);
  s->pointer_x = (float)mx;
  s->pointer_y = (float)my;
  s->pointer_down = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) ? 1 : 0;
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}

static void print_json(const InputStateJson* s, const char* mode) {
  printf(
      "{\"pointer_down\":%d,\"pointer_x\":%.1f,\"pointer_y\":%.1f,"
      "\"key_escape\":%d,\"key_cmd_k\":%d,\"key_digit\":%d,"
      "\"mock\":%s,\"capture_mode\":\"%s\"}\n",
      s->pointer_down, s->pointer_x, s->pointer_y, s->key_escape, s->key_cmd_k,
      s->key_digit, s->mock ? "true" : "false", mode);
}

int main(int argc, char** argv) {
  InputStateJson state;
  input_state_reset(&state);
  const char* mode = "sdl_poll";
  int width = 1280;
  int height = 720;
  int mock_only = 0;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--mock") == 0 && i + 1 < argc) {
      if (apply_mock_arg(&state, argv[++i]) != 0) {
        fprintf(stderr, "invalid --mock spec (use cmd_k,digit=3)\n");
        return 1;
      }
      mock_only = 1;
      mode = "argv_mock";
    } else if (strcmp(argv[i], "--mock-only") == 0) {
      mock_only = 1;
      mode = "argv_mock";
    } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
      width = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
      height = atoi(argv[++i]);
    }
  }

  if (!mock_only && state.mock == 0) {
    if (apply_mock_flags(&state, argc, argv) == 0 &&
        (state.key_escape || state.key_cmd_k || state.key_digit)) {
      mock_only = 1;
      mode = "argv_mock";
    }
  }

  if (mock_only || state.mock) {
    if (!state.mock) {
      state.mock = 1;
    }
    print_json(&state, mode);
    return 0;
  }

  int rc = poll_sdl_once(&state, width, height);
  if (rc != 0) {
    return rc;
  }
  print_json(&state, mode);
  return 0;
}
