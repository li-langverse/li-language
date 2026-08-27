# Phase 5: Tetris Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans.

**Goal:** Ship `examples/tetris/` — playable Tetris compiled with li, SDL2 window, keyboard input, 60 FPS loop.

**Architecture:** Game logic in li with fixed-size board types; thin SDL2 `extern` layer; no heap allocator required.

**Depends on:** Phase 4  
**Blocks:** Nothing (v1 milestone complete)

---

## Constants (compile-time shaped)

```nim
const BOARD_W = 10
const BOARD_H = 20
const CELL_PX = 24

type
  Color = enum
    Empty, Cyan, Yellow, Purple, Green, Red, Blue, Orange

  Cell = object
    color: Color

  Board = array[BOARD_H, array[BOARD_W, Cell]]
  PieceKind = enum
    I, O, T, S, Z, J, L
```

Board type errors (wrong row/col in indexing) must fail at **compile time** when indices are literals.

---

### Task 1: SDL2 extern bindings

**Files:**
- Create: `examples/tetris/sdl2.li` (extern block)
- Create: `examples/tetris/build.sh` (optional wrapper with `pkg-config sdl2`)

- [x] Extern: `SDL_Init`, `SDL_CreateWindow`, `SDL_CreateRenderer`, `SDL_SetRenderDrawColor`, `SDL_RenderClear`, `SDL_RenderFillRect`, `SDL_RenderPresent`, `SDL_PollEvent`, `SDL_Quit` (`examples/tetris/tetris_rt.c`)
- [x] Event struct as opaque pointer + field accessors OR minimal POD matching SDL layout (document choice: SDL hidden in C rt; keys via `tetris_poll_key`)

---

### Task 2: Board + pieces

**Files:**
- Create: `examples/tetris/board.li`
- Create: `examples/tetris/pieces.li`

- [ ] `new_board() -> Board` all Empty
- [ ] Tetromino shapes as `array[4, array[2, int]]` offsets per rotation
- [ ] `can_place(board, piece, ox, oy) -> bool`
- [ ] `lock_piece(board, piece, ox, oy)` mutates board

---

### Task 3: Game loop

**Files:**
- Create: `examples/tetris/game.li`
- Create: `examples/tetris/main.li`

- [x] `def run() raises IO` (`run_game` / `main` in `examples/tetris/main.li`)
- [x] Gravity tick every N frames (`last_drop` / 400 ms)
- [x] Line clear: scan row, shift down
- [x] Game over when spawn blocked

---

### Task 4: Input

- [ ] Left/Right/Down arrows, Up rotates, Space hard drop (L/R/Down in `main.li`; rotate + hard-drop not wired)
- [x] Map SDL keycodes in `handle_event` (via `tetris_poll_key` in `tetris_rt.c`)

---

### Task 5: Render

- [x] Draw grid: filled rects per non-Empty cell
- [x] Draw active piece
- [x] Simple border (`tetris_rt.c` grid padding)

---

### Task 6: Build + release

```bash
lic build examples/tetris/main.li -o tetris --release \
  $(pkg-config --libs --cflags sdl2)
```

- [x] 60 FPS stable on M-series Mac (`tetris_delay(16)` target frame pacing)
- [ ] No leaks/crashes over 10-minute play session

---

### Phase 5 exit gate (project milestone)

- [x] Playable Tetris binary (Phase 5 tracker `[x]`; `examples/tetris/build.sh`)
- [x] `lic check` catches wrong board dimensions in test fixture (`li-tests/tetris/board_oob.li`)
- [x] Master plan tracker updated

---

## Post-milestone demo script

1. `lic check examples/tetris/main.li`
2. `lic build examples/tetris/main.li -o tetris --release`
3. `./tetris`
