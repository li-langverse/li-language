#pragma once

#include <stdint.h>

typedef struct TetrisCtx TetrisCtx;

TetrisCtx* tetris_open(int width_px, int height_px);
void tetris_close(TetrisCtx* ctx);
uint32_t tetris_ticks(void);
void tetris_delay(int ms);
int tetris_poll_key(TetrisCtx* ctx);
void tetris_begin_frame(TetrisCtx* ctx);
void tetris_draw_cell(TetrisCtx* ctx, int col, int row, int color, int cell_px);
void tetris_end_frame(TetrisCtx* ctx);
