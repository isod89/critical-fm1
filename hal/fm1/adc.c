/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's ADC, polled: one conversion at a time. Written for this project from the facts in fm1.h. */
#include "fm1.h"
#include "board.h"
#include "adc.h"

void adc_init(void)
{
    uint32_t m = (1u << 6) | (1u << 1);                    /* PB6 the pot, PB1 the battery: analog, no pulls */
    GPIO(PB, G_DIE) &= ~m; GPIO(PB, G_PU) &= ~m; GPIO(PB, G_PD) &= ~m; GPIO(PB, G_DIR) |= m;
}

int32_t adc_read(uint32_t ch)
{
    const uint32_t con = 6u | 8u | (15u << 12);              /* the SDK's divider and start delay */
    ADC_CON = 0;
    if (WLA_CON0 & (1u << 14)) WLA_CON0 &= ~(1u << 14);
    ADC_CON = (con & 0xF00Fu) | 0x40u | ((ch & 0xFu) << 8);
    ADC_CON |= 0x10u;
    ADC_CON |= 0x40u;
    uint32_t t0 = time_ticks(), lim = 2000u * time_mhz();    /* 2 ms at most */
    while (!(ADC_CON & 0x80u))
        if ((uint32_t)(time_ticks() - t0) > lim) { ADC_CON = 0x40u; ADC_CON = 0; return -1; }
    int32_t v = (int32_t)(ADC_RES & 0x3FFu);
    ADC_CON = 0x40u; ADC_CON = 0;
    return v;
}
