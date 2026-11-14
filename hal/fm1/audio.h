/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's audio out (audio.c): ALNK0 in DMA, a buffer of two halves, polled (a RAM app has no interrupts).
 * Frames are two int32 words, left then right, signed 24-bit in the low bits: what snd_render writes. */
#pragma once
#include <stdint.h>

enum { AUDIO_OK = 0, AUDIO_ERR_PLL = -1, AUDIO_ERR_BUSY = -2, AUDIO_ERR_BUF = -3 };

/* buf: 2 * half_words int32, word-aligned, in SRAM; half_words even (stereo), at most 65534 */
int audio_start(int32_t *buf, uint32_t half_words);
int audio_half_free(void);               /* 1: a half is free to fill (the DMA plays the other) */
int32_t *audio_free_half(void);          /* that half */
int audio_playing_half(void);            /* 0 / 1: the half the DMA plays now */
void audio_half_done(void);              /* filled: the stores made visible, the event cleared */
void audio_stop(void);                   /* DMA off, the pins and clocks as they were */
uint32_t audio_rate_milli(void);         /* the real rate, in mHz: 44117647 */
