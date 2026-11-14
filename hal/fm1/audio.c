/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's audio out: ALNK0 (I2S) in DMA to the external codec, polled. Written for this project from the
 * register facts in fm1.h. The codec needs no setup of its own: it follows the I2S clocks. */
#include "fm1.h"
#include "board.h"
#include "audio.h"

static int32_t *a_buf;
static uint32_t a_half;
static uint32_t saved_clk2, saved_iomap5;

int audio_start(int32_t *buf, uint32_t half_words)
{
    uintptr_t a = (uintptr_t)buf;
    if (!buf || (a & 3u) || !half_words || (half_words & 1u) || half_words > 65534u ||
        a < 0x01C00000u || a + 8u * half_words > 0x01C7FD50u)
        return AUDIO_ERR_BUF;
    /* the rate is the stock audio PLL's: refuse rather than play at an unknown rate */
    if (APLL_CON0 != APLL_STOCK0 || (APLL_CON1 & 0x07FFFFFFu) != (APLL_STOCK1 & 0x07FFFFFFu)) return AUDIO_ERR_PLL;
    if (ALNK_CON0 & 0x800u) return AUDIO_ERR_BUSY;
    a_buf = buf; a_half = half_words;
    for (uint32_t i = 0; i < 2u * half_words; i++) buf[i] = 0;          /* both halves start silent */
    saved_clk2 = CLK_CON2; saved_iomap5 = IOMAP_CON5;
    pin_out(AUDIO_DATA, 0);
    time_wait_us(5000);
    ALNK_CON0 = 0; ALNK_CON1 = 0; ALNK_CON2 = 0; ALNK_CON3 = 0;
    IOMAP_CON5 &= ~0xC0u;
    ALNK_CON3 |= 3u;
    pin_out(AUDIO_MCLK, 1); ALNK_CON0 |= 0x100u;                       /* the master clock out */
    pin_out(AUDIO_LRCK, 1); pin_out(AUDIO_BCLK, 1);
    ALNK_CON0 |= 0x80u; ALNK_CON0 &= ~0x40u;
    ALNK_LEN = (uint16_t)half_words;
    ALNK_CON0 &= ~0x400u; ALNK_CON0 &= ~0x200u;
    CLK_CON2 &= 0xFFFFF0FFu;                                           /* the ALNK clock from the audio PLL */
    ALNK_CON3 &= ~0x1Cu;
    ALNK_CON3 = (uint8_t)((ALNK_CON3 & 0x1Fu) | 0x80u);
    ALNK_CON1 |= 1u << 12; ALNK_CON1 |= 1u << 14;                      /* channel 3 out, 24-bit */
    ALNK_ADR3 = (uint32_t)a;
    pin_out(AUDIO_DATA, 1);
    ALNK_CON1 &= ~(1u << 15);
    ALNK_CON2 = 0x0Fu;                                                 /* no stale event */
    CSYNC();                                                           /* the silent halves before the DMA */
    ALNK_CON0 |= 0x800u;
    time_wait_us(5000);
    pin_out(AUDIO_DATA, 1);
    return AUDIO_OK;
}

int audio_half_free(void)
{
    uint8_t p = ALNK_CON2;
    if (p & 0x70u) {                                                   /* the other events: acknowledged, unused */
        uint8_t ack = (uint8_t)(((p & 0x10u) ? 1u : 0u) | ((p & 0x20u) ? 2u : 0u) | ((p & 0x40u) ? 4u : 0u));
        ALNK_CON2 |= ack;
    }
    return (p & 0x80u) != 0;
}
int32_t *audio_free_half(void) { return a_buf + (((ALNK_CON0 >> 15) & 1u) ^ 1u) * a_half; }
int audio_playing_half(void) { return (int)((ALNK_CON0 >> 15) & 1u); }
void audio_half_done(void) { CSYNC(); ALNK_CON2 |= 0x08u; }

void audio_stop(void)
{
    if (!(ALNK_CON0 & 0x800u)) return;
    ALNK_CON0 &= ~0x800u;
    CSYNC();
    ALNK_CON2 = 0x0Fu;
    pin_out(AUDIO_DATA, 0);
    CLK_CON2 = (CLK_CON2 & ~0xF00u) | (saved_clk2 & 0xF00u);
    IOMAP_CON5 = (IOMAP_CON5 & ~0xC0u) | (saved_iomap5 & 0xC0u);
}
uint32_t audio_rate_milli(void) { return (uint32_t)((uint64_t)AUDIO_RATE_NUM * 1000u / AUDIO_RATE_DEN); }
