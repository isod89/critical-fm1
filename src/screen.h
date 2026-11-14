/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: the screen (screen.c), drawn a tile at a time with app/gfx.c: scr_begin() once a pass, then
 * scr_draw() for each tile (clipped to it). The desk; the board (HOME); the rods (HOME again). */
#ifndef SCREEN_H
#define SCREEN_H
#include "inst.h"
#define SCR_BOOT_MS 6400                       /* the terminal's login, from power-on (inst.splash_until: now + this) */
void scr_begin(const inst *g);
void scr_draw(const inst *g);
#endif
