/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's registers, as facts (addresses, bits, wiring). Written for this project from the public facts listed
 * in docs/ANALYSE-ET-PLAN.md (m-bootloader's SPEC, the pi32v2 ISA reference, the AC79 SDK's register names, our
 * own measurements). No code of another firmware is used. */
#pragma once
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(uintptr_t)(a))

/* ---- clock (SPEC 2.2, 3.2) */
#define CLK_DIV       REG32(0x10008u)       /* HSB divider - 1: bits 17:16; LSB divider - 1: bits 10:8 */
#define CLK_CON0      REG32(0x1000Cu)
#define CLK_CON2      REG32(0x10014u)       /* low byte: the system clock's source (6 = 360 MHz) */
#define PLL1_CON0     REG32(0x119A8u)
#define PLL1_CON1     REG32(0x119ACu)

/* ---- timer 0: counts the LSB clock (SPEC 3.3) */
#define T0_CON        REG32(0x10400u)
#define T0_CNT        REG32(0x10404u)
#define T0_PRD        REG32(0x10408u)

/* ---- the core's cycle counter (AC79 SDK csfr.h, corex2 block; not yet verified on hardware: the bench checks it) */
#define C0_CYCLES     REG32(0x1EEE218u)
#define C0_STALL_IF   REG32(0x1EEE200u)
#define C0_STALL_RD   REG32(0x1EEE208u)
#define C0_STALL_WR   REG32(0x1EEE210u)

/* ---- GPIO: port p (A = 0 .. H = 7) at 0x50000 + 0x40 p */
#define GPIO(p, off)  REG32(0x50000u + 0x40u * (p) + (off))
#define G_OUT  0x00u
#define G_IN   0x04u
#define G_DIR  0x08u                         /* 1 = input */
#define G_DIE  0x0Cu
#define G_PU   0x10u
#define G_PD   0x14u
enum { PA, PB, PC, PD, PE, PF, PG, PH };
#define IOMAP_CON1    REG32(0x51020u)        /* bit 4: SPI1 on its group B pins (the FM-1's panel) */

/* ---- SPI1, the panel (SPEC 2.2, 8.4) */
#define SPI1_CON      REG32(0x11D00u)        /* bit 0 on, bit 12 RX, bit 14 clear done, bit 15 done */
#define SPI1_BAUD     REG32(0x11D04u)        /* clock = LSB / (baud + 1) */
#define SPI1_BUF      REG32(0x11D08u)
#define SPI1_ADR      REG32(0x11D0Cu)        /* DMA address (internal RAM) */
#define SPI1_CNT      REG32(0x11D10u)        /* DMA count: writing it starts the transfer */

/* ---- the FM-1's wiring (SPEC 8.2, 8.4) */
#define LCD_CS   PC, 7                        /* active low */
#define LCD_DC   PC, 8                        /* 0 command, 1 data */
#define LCD_CLK  PC, 9
#define LCD_MOSI PC, 10
#define LCD_BL   PA, 2                        /* backlight, active low; low also powers the key matrix */
#define KEY_DATA PA, 4                        /* two shift registers select the matrix columns */
#define KEY_CLK  PA, 3
#define KEY_LATCH PA, 1
#define KEY_ROW_PLAY PA, 6                    /* PLAY: column word 0xFBFF, row PA6, low = pressed */
#define KEY_COL_PLAY 0xFBFFu
#define KEY_COL_REC  0xFF7Fu                  /* REC: row PA6 too */

static inline void pin_out(int p, int n, int level)
{
    if (level) GPIO(p, G_OUT) |= 1u << n; else GPIO(p, G_OUT) &= ~(1u << n);
    GPIO(p, G_DIR) &= ~(1u << n);
}
static inline void pin_set(int p, int n, int level) { if (level) GPIO(p, G_OUT) |= 1u << n; else GPIO(p, G_OUT) &= ~(1u << n); }
static inline void pin_in_pullup(int p, int n)
{
    GPIO(p, G_DIR) |= 1u << n; GPIO(p, G_DIE) |= 1u << n; GPIO(p, G_PU) |= 1u << n; GPIO(p, G_PD) &= ~(1u << n);
}
static inline int pin_read(int p, int n) { return (int)((GPIO(p, G_IN) >> n) & 1u); }

/* ---- audio out: ALNK0 (I2S) in DMA to the FM-1's external codec. Facts: the AC79 SDK's register names (csfr.h),
 * the order checked against the public FM-1 projects' drivers (docs/ANALYSE-ET-PLAN.md 5.1), the pins below.
 * Each frame is two int32 words, left then right, the sample in the low 24 bits (signed, sign-extended). The DMA
 * plays a buffer of two halves; CON2 bit 7 says a half is free, CON0 bit 15 which half it is playing. The rate is
 * the stock audio PLL's 24 MHz / 544 = 44117.647 Hz (m-bootloader leaves it so, SPEC 3.2: PLL0 0x45400203). */
#define ALNK_CON0     (*(volatile uint16_t *)0x12E00u)   /* bit 11 run, bit 15 the half being played */
#define ALNK_CON1     (*(volatile uint16_t *)0x12E04u)
#define ALNK_CON2     (*(volatile uint8_t *)0x12E08u)    /* pending: bit 7 a half free (cleared by bit 3) */
#define ALNK_CON3     (*(volatile uint8_t *)0x12E0Cu)
#define ALNK_ADR3     (*(volatile uint32_t *)0x12E1Cu)   /* the buffer (channel 3, the FM-1's data line) */
#define ALNK_LEN      (*(volatile uint16_t *)0x12E20u)   /* a half's length in 32-bit words */
#define IOMAP_CON5    REG32(0x51030u)
#define APLL_CON0     REG32(0x119A0u)                    /* the audio PLL (PLL0) */
#define APLL_CON1     REG32(0x119A4u)
#define APLL_STOCK0   0x45400203u
#define APLL_STOCK1   0x3F503026u                        /* compared under 0x07FFFFFF */
#define AUDIO_MCLK    PC, 0
#define AUDIO_BCLK    PC, 1
#define AUDIO_LRCK    PC, 2
#define AUDIO_DATA    PC, 6
#define AUDIO_RATE_NUM 24000000u
#define AUDIO_RATE_DEN 544u

/* ---- the second core (CPU1). Facts: pi32v2-isa docs/CPU1.md (MIT-0, tested on an FM-1), AC79 SDK csfr.h. It
 * starts once per power-on (a start after a hold is not established); it boots through the ROM to the address in
 * the word at 0x01C7FFF8. */
#define C1_CON        REG32(0x1EEE004u)    /* bit 1 hold, bit 3 run */
#define SYS_DIV       REG32(0x10008u)      /* bit 3 set while CPU1 starts */
#define C1_ENTRY      REG32(0x01C7FFF8u)
#define C1_ICFG(i)    REG32(0x1EEF300u + 4u * (i))  /* CPU1's interrupt sources (CPU0's at 0x1EEF100) */
#define C1_EMU_CON    REG32(0x1EEF2D0u)
#define C1_EMU_SSP_H  REG32(0x1EEF2D8u)    /* CPU1's stack limits */
#define C1_EMU_SSP_L  REG32(0x1EEF2DCu)
#define C1_EMU_USP_H  REG32(0x1EEF2E0u)
#define C1_EMU_USP_L  REG32(0x1EEF2E4u)
#define DBG_WR_EN     REG32(0x1EEE240u)    /* bit 0: debug registers writable; writing 0xE7 toggles it */
#define DBG_MSG_CLR   REG32(0x1EEE248u)
#define DBG_EN        REG32(0x1EEE340u)
#define C0_WR_LIMIT_EN REG32(0x1EEE348u)   /* bit 2: the top of RAM write-protected */
#define PC_LIMIT0_H   REG32(0x1EEE380u)
#define PC_LIMIT0_L   REG32(0x1EEE384u)
#define PC_LIMIT1_H   REG32(0x1EEE388u)
#define PC_LIMIT1_L   REG32(0x1EEE38Cu)
#define DBG_BUS_BITS  0x003F0030u          /* the bus-error causes a hold can raise late */
#define CSYNC()       __asm__ volatile("csync" ::: "memory")

/* ---- the SAR ADC, 10 bits (AC79 SDK names; FM-1 wiring checked against the public FM-1 projects): the MASTER
 * pot on PB6 = channel 4, the battery divider on PB1 = channel 3 (PB1 stays an analog input: held low for 8 s it
 * resets the chip). CON: bits 0-2 clock divider, bit 3 set, bit 4 on, bit 6 start / clear, bit 7 done, bits 8-11
 * the channel, bits 12-15 the start delay. */
#define ADC_CON       REG32(0x13100u)
#define ADC_RES       REG32(0x13104u)
#define WLA_CON0      REG32(0x11900u)      /* bit 14 cleared before a conversion */
#define ADC_CH_MASTER 4u
#define ADC_CH_BATT   3u
#define POT_MASTER    PB, 6

/* ---- the panel matrix: 11 columns behind the two shift registers (PA4 data, PA3 clock, PA1 latch; a column is
 * selected low: word 0xFFFF ^ (1 << c), and for columns 0 and 1 also bit 11 + c), 6 rows read low when closed:
 * PA0, PA5, PA6, PA7, PA8, PB7, with pull-ups. PA2 low powers it (and the backlight). LED lines PA9, PA10, PH6,
 * PH9, high lit, light the LED of the selected column on rows PA5, PA6, PA7, PA8. Facts: m-bootloader SPEC 8.2
 * (OCT-, OCT+, PLAY, REC verified there), the rest of the map as the public FM-1 projects' default panel layout;
 * the prototype shows every press by name, to check it on each unit. Ids: 0 OCT-, 1 OCT+, 2..13 the buttons,
 * 14..40 the 27 note keys F3..G5. */
#define PANEL_NCOL 11
#define PANEL_NKEY 41
static const int8_t PANEL_MAP[6][PANEL_NCOL] = {           /* key id at (row bit, column), -1 none */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},              /* PA0: encoders */
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},              /* PA5 */
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},              /* PA6 */
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},              /* PA7 */
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},              /* PA8 */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},              /* PB7: encoder 6 */
};
/* the 7 encoders: A at (column, row bit), B at (column, row bit); a detent is a full quadrature cycle */
static const uint8_t PANEL_ENC[7][4] = {
    {0, 0, 1, 0}, {2, 0, 3, 0}, {8, 1, 9, 1}, {8, 0, 9, 0}, {6, 0, 7, 0}, {4, 0, 5, 0}, {0, 5, 1, 5},
};
enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_FX = 2, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO, BTN_HOME, BTN_SAVE,
       BTN_ARP, BTN_SEQ, BTN_PLAY, BTN_REC };
enum { ENC_SELECT = 0, ENC_ALGORITHM = 1, ENC_KNOB1 = 2, ENC_KNOB2 = 3, ENC_KNOB3 = 4, ENC_KNOB4 = 5, ENC_PRESETS = 6 };

/* ---- SPI0, the flash (m-bootloader SPEC 3.4, 3.6): CS on PD0 by software, PD4 clock, PD5 out, PD1 in. Taking SPI0
 * turns off the flash window (XIP, SFC at 0x40200): the firmware runs from RAM, so nothing reads it any more. */
#define SPI0_CON      REG32(0x11C00u)        /* bit 0 on, bit 3 one line, bit 12 RX, bit 14 clear, bit 15 done */
#define SPI0_BAUD     REG32(0x11C04u)
#define SPI0_BUF      REG32(0x11C08u)
#define SPI0_ADR      REG32(0x11C0Cu)
#define SPI0_CNT      REG32(0x11C10u)
#define SPI0_ENC      REG32(0x13000u)        /* 2: DMA passes through, 0: off */
#define SFC_CON       REG32(0x40200u)
#define SFC_ENC       REG32(0x40300u)
#define IOMAP_CON0    REG32(0x5101Cu)
#define SYS_RESET()   do { REG32(0x10000u) |= 1u << 4; for (;;) ; } while (0)
