/* SPDX-License-Identifier: GPL-3.0-only
 * Drawing into one tile of the screen (gfx.c). The screen (240 x 240) is drawn a tile at a time into a small
 * buffer: the whole interface is drawn for each tile, clipped to it, so nothing keeps a picture of the screen.
 * A tile that comes out the same as last time is not sent (ui.c compares a hash). Colours are RGB565.
 * Text: UTF-8, capitals (small letters are drawn as capitals, but m b d i s u for chord names), the accented
 * capitals French needs, '→', '·'.
 * Text is placed by the top of its capitals: a box of height h centres it with y = box_y + (h - font->cap) / 2.
 * Portable: the PC renders the same screens (tests/ui_shots.c). */
#pragma once
#include <stdint.h>

typedef struct { uint8_t w, h; int8_t x, y; uint8_t adv; uint16_t off; } gfx_glyph;   /* x, y: from the pen */
typedef struct { const gfx_glyph *g; const uint8_t *bits; uint8_t n, asc, height, cap, cap_top, bpp; } gfx_font;
extern const gfx_font font_lbl, font_hdr, font_dat, font_val;   /* labels, headers (Barlow Condensed), data, values (B612 Mono): tools/mkfont.py */

#define GFX_W 240
#define GFX_H 240
#ifndef RGB
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#endif

void gfx_target(uint16_t *px, int x, int y, int w, int h);   /* the tile drawn into now */
int gfx_visible(int x, int y, int w, int h);                 /* does the box touch the tile */
void gfx_fill(int x, int y, int w, int h, uint16_t c);
void gfx_frame(int x, int y, int w, int h, int t, uint16_t c);   /* an outline t px thick, inside the box */
void gfx_rfill(int x, int y, int w, int h, int r, uint16_t c);   /* rounded corners of radius r */
void gfx_rframe(int x, int y, int w, int h, int r, int t, uint16_t c);
void gfx_disc(int cx, int cy, int r, uint16_t c);
void gfx_play(int x, int y, int s, uint16_t c);              /* a triangle pointing right in an s x s box */
int gfx_text_w(const gfx_font *f, const char *s);
void gfx_spacing(int sp);                                    /* extra px between letters (< 0: tighter), until reset */
int gfx_text(const gfx_font *f, int x, int y, const char *s, uint16_t c);   /* returns where the text ends */
void gfx_text_c(const gfx_font *f, int cx, int y, const char *s, uint16_t c);   /* centred on cx */
void gfx_text_r(const gfx_font *f, int rx, int y, const char *s, uint16_t c);   /* ending at rx */
uint16_t gfx_mix(uint16_t bg, uint16_t fg, unsigned a32);     /* fg over bg, a32 0..32 */
uint32_t gfx_finish(uint16_t *px, int n);                    /* bytes swapped for the panel; a hash of the tile */
