/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1 board, first part (board.c): time, the panel and its text console (20 x 15 cells), PLAY / REC. */
#pragma once
#include <stdint.h>

void time_init(uint32_t lsb_mhz);
uint32_t time_ticks(void);               /* the LSB clock (60 MHz from the bootloader) */
uint32_t time_mhz(void);
void time_wait_us(uint32_t us);

void lcd_init(void);
void lcd_fill(int x, int y, int w, int h, uint16_t rgb565);
void lcd_text(int col, int row, const char *s, uint16_t fg, uint16_t bg);   /* 12 x 16 cells, 20 x 15 */
void lcd_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale);  /* scale 2: 12 x 16, 3: 18 x 24 */
void lcd_tile_start(int x, int y, int w, int h, const void *px);   /* w x h pixels (bytes ready for the panel) by
                                                                     * DMA, at most 32 KB; returns at once */
int lcd_tile_busy(void);                 /* 1 while a tile is being sent (then: nothing else on the panel) */
#ifndef RGB
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#endif

void keys_init(void);
int key_down(uint32_t col_word);         /* KEY_COL_PLAY, KEY_COL_REC */
