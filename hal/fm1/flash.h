/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's flash (flash.c), from RAM, polled. Reads wait for their DMA; an erase or a page write is started and
 * then polled (flash_busy), so the audio never waits for the flash: a sector erase can take 400 ms. */
#pragma once
#include <stdint.h>
int flash_open(void);                                    /* the flash found, writes allowed; 0 = OK */
uint32_t flash_jedec(void);
int flash_read(uint32_t addr, void *dst, uint32_t n);    /* 0 = OK (dst in RAM, word-aligned) */
int flash_busy(void);                                    /* 1 while an erase or a write runs */
int flash_erase_start(uint32_t addr);                    /* a 4 KB sector; 0 = started */
int flash_program_start(uint32_t addr, const void *src, uint32_t n);   /* within one 256-byte page */
/* the flash window (XIP): the code that is not in RAM runs from it. Each access above lends SPI0 the flash and gives
 * it back; an erase or a write keeps the window off until flash_busy() sees it done */
int flash_xip_on(void);                                  /* 0 while the window is off: only RAM code may run */
void flash_wait_idle(void);                              /* the flash done, the window back */
void flash_plain_from(uint32_t from);
void flash_xip_forget(uint32_t off, uint32_t len);       /* the window's cache: those flash bytes read again */              /* the window reads in clear from flash offset `from` (SLOOP's data) */
