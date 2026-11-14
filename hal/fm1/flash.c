/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's flash on SPI0, single-bit, polled, lent by the flash window (XIP) for each access and given back.
 * Written for this project from facts: m-bootloader's SPEC 3.4 (the pins, the command set of the FM-1's
 * Winbond-style parts, the status bits); the window's controller as the FM-1's public firmwares and the AC79 SDK's
 * flash driver handle it at run time (wait for the cache and the controller to be idle, keep its control word, turn
 * it off, give the pins to SPI0; then the pins back and the control word back), checked on Sam's FM-1 (DIAG XIP 1,
 * 9 Nov 2026): the decryption's registers are never touched (its key reads 0: written back, the window reads wrong).
 * While an erase or a write runs the flash cannot be read: the window stays off until it is done (flash_busy gives
 * it back), and only code in RAM may run (fw/main.c). This file is in RAM. */
#include "fm1.h"
#include "board.h"
#include "flash.h"

static uint32_t capacity;
static void settle(void) { for (volatile int i = 0; i < 40; i++) ; }
static void cs(int high) { pin_set(PD, 0, high); settle(); }
static int wait_done(void)
{
    uint32_t t0 = time_ticks(), lim = 20000u * time_mhz();
    while (!(SPI0_CON & (1u << 15))) if ((uint32_t)(time_ticks() - t0) > lim) return -1;
    return 0;
}
static uint8_t xfer(uint8_t v, int rx)
{
    if (rx) SPI0_CON |= 1u << 12; else SPI0_CON &= ~(1u << 12);
    SPI0_BUF = v;
    CSYNC();
    wait_done();
    uint8_t r = (uint8_t)SPI0_BUF;
    SPI0_CON |= 1u << 14;
    return r;
}
static void single(void) { SPI0_CON = (SPI0_CON & ~((3u << 10) | (1u << 12))) | (1u << 3); }
static void command(uint8_t op) { single(); cs(0); xfer(op, 0); }
static void addr24(uint32_t a) { xfer((uint8_t)(a >> 16), 0); xfer((uint8_t)(a >> 8), 0); xfer((uint8_t)a, 0); }
static uint8_t status(uint8_t op) { command(op); uint8_t v = xfer(0xFF, 1); cs(1); return v; }
static void wren(void) { command(0x06); cs(1); }
static int wait_ready(uint32_t ms)
{
    uint32_t t0 = time_ticks(), lim = ms * 1000u * time_mhz();
    while (status(0x05) & 1u) if ((uint32_t)(time_ticks() - t0) > lim) return -1;
    return 0;
}

/* the window off / on (one owner at a time: the window, or SPI0) */
#define SFC_CON0     REG32(0x40200u)                    /* bit 31: busy */
#define CACHE_CON    REG32(0x01EEE008u)                 /* bit 14: idle */
static int xip_off_now;
static uint32_t sfc_saved, spi_saved, map1_saved, pd_saved[6];
static uint32_t flash_jedec_raw(void);
static const uint8_t PD_REG[6] = { G_OUT, G_DIR, G_DIE, G_PU, G_PD, 0x1Cu };
int flash_xip_on(void) { return !xip_off_now; }
static void spi_setup(void)
{
    pin_out(PD, 0, 1); pin_out(PD, 4, 0); pin_out(PD, 5, 0);
    GPIO(PD, 0x1Cu) |= 1u << 4;                               /* the clock pin's drive */
    GPIO(PD, G_DIR) |= 1u << 1; GPIO(PD, G_DIE) |= 1u << 1; GPIO(PD, G_PU) &= ~(1u << 1); GPIO(PD, G_PD) &= ~(1u << 1);
    pin_out(PD, 2, 1); pin_out(PD, 3, 1);                     /* the quad lines idle high (WP#, HOLD#) */
    SPI0_ENC = 0;
    SPI0_CON = 0x28u;
    SPI0_BAUD = 2;                                            /* 20 MHz at LSB 60 */
    SPI0_CON |= (1u << 14) | 1u;
    cs(1);
}
static void xip_off(void)
{
    if (xip_off_now) return;
    for (uint32_t n = 0; n < 2000000u && !(CACHE_CON & (1u << 14)); n++) { }
    CSYNC();
    for (uint32_t n = 0; n < 2000000u && (SFC_CON0 & 0x80000000u); n++) { }
    sfc_saved = SFC_CON0; spi_saved = SPI0_CON;
    for (int i = 0; i < 6; i++) pd_saved[i] = GPIO(PD, PD_REG[i]);
    map1_saved = IOMAP_CON1;
    SFC_CON0 = 0;                                             /* the window off from here */
    IOMAP_CON0 &= ~(1u << 5);                                 /* the pins: SPI0's */
    IOMAP_CON1 &= ~(1u << 1);                                 /* (as our SPI0 access has always set it) */
    xip_off_now = 1;
    spi_setup();
    command(0xFF); xfer(0xFF, 0); cs(1);                      /* (out of a continuous read) */
    CSYNC();
}
static void xip_on(void)
{
#ifdef FLASH_UPDATER
    return;                                                   /* the update loader: no window, SPI0 keeps the flash */
#endif
    if (!xip_off_now) return;
    cs(1);
    SPI0_CON = spi_saved; SPI0_ENC = 0;
    for (int i = 0; i < 6; i++) GPIO(PD, PD_REG[i]) = pd_saved[i];
    IOMAP_CON1 = map1_saved;
    IOMAP_CON0 |= 1u << 5;                                    /* the pins: the window's */
    CSYNC();
    for (uint32_t n = 0; n < 2000000u && (SFC_CON0 & 0x80000000u); n++) { }
    SFC_CON0 = sfc_saved;                                     /* the window back */
    CSYNC();
    xip_off_now = 0;
}

/* the window in clear from flash offset `from` up (SLOOP's data there is not enciphered): the decipher's plain range */
#define SFCENC_CON     (*(volatile uint8_t *)0x40300u)       /* bit 0 decipher, bit 1 the plain range */
#define SFCENC_UNENC_H REG32(0x40308u)                      /* (write only: window addresses) */
#define SFCENC_UNENC_L REG32(0x4030Cu)
void flash_plain_from(uint32_t from)
{
    SFCENC_CON &= (uint8_t)~2u;
    SFCENC_UNENC_L = 0x02000000u + from - 0x4000u;
    SFCENC_UNENC_H = 0x07FFFFFFu;
    SFCENC_CON |= 2u;
    CSYNC();
}
int flash_open(void)
{
    xip_off();
    uint32_t id = flash_jedec_raw(), e = id & 0xFFu;
    capacity = e >= 16 && e <= 24 ? 1u << e : 0;
    int r = 0;
    if (!capacity) r = -1;
    else {                                                    /* writes allowed: the protection bits cleared */
        uint8_t s1 = status(0x05), s2 = status(0x35);
        if ((s1 & 0x7Cu) || (s2 & 0x40u)) {
            wren(); command(0x01); xfer((uint8_t)(s1 & ~0x7Fu), 0); xfer((uint8_t)(s2 & ~0x40u), 0); cs(1);
            if (wait_ready(100)) r = -1;
            else if ((status(0x05) & 0x7Cu) || (status(0x35) & 0x40u)) r = -1;
        }
    }
    if (wait_ready(100)) r = -1;
    xip_on();
    return r;
}
static uint32_t flash_jedec_raw(void)
{
    command(0x9F);
    uint32_t id = 0;
    for (int i = 0; i < 3; i++) id = (id << 8) | xfer(0xFF, 1);
    cs(1);
    return id;
}
uint32_t flash_jedec(void) { int was = xip_off_now; xip_off(); uint32_t id = flash_jedec_raw(); if (!was) xip_on(); return id; }
/* an erase or a write still running; once done, the window is given back */
int flash_busy(void)
{
    if (!xip_off_now) return 0;
    if (status(0x05) & 1u) return 1;
    xip_on();
    return 0;
}
void flash_wait_idle(void) { while (flash_busy()) { } }

int flash_read(uint32_t a, void *dst, uint32_t n)
{
    uint8_t *p = (uint8_t *)dst;
    if (!capacity || a > capacity || n > capacity - a) return -1;
    int was = xip_off_now;
    xip_off();
    if (wait_ready(1000)) { if (!was) xip_on(); return -1; }
    while (n) {
        uint32_t k = n > 4096u ? 4096u : n;
        command(0x03); addr24(a);
        SPI0_ENC = 2;
        SPI0_CON |= (1u << 12) | (1u << 14);
        SPI0_ADR = (uint32_t)(uintptr_t)p;
        CSYNC();
        SPI0_CNT = k;
        int r = wait_done();
        SPI0_CON |= 1u << 14;
        SPI0_ENC = 0;
        cs(1); single();
        if (r) { if (!was) xip_on(); return -1; }
        a += k; p += k; n -= k;
    }
    CSYNC();
    if (!was) xip_on();                                       /* (the window as it was) */
    return 0;
}
int flash_erase_start(uint32_t a)
{
    if (!capacity || (a & 4095u) || a >= capacity || flash_busy()) return -1;
    xip_off();                                                /* (off until flash_busy sees it done) */
    wren(); command(0x20); addr24(a); cs(1);
    return 0;
}
int flash_program_start(uint32_t a, const void *src, uint32_t n)
{
    const uint8_t *p = (const uint8_t *)src;
    if (!capacity || !n || (a & 255u) + n > 256u || a + n > capacity || flash_busy()) return -1;
    xip_off();
    wren(); command(0x02); addr24(a);
    for (uint32_t i = 0; i < n; i++) xfer(p[i], 0);
    cs(1);
    return 0;
}

/* lines of the window's cache dropped (a range of flash rewritten that the window reads: the sample slots), so that
 * the window reads the flash again */
void flash_xip_forget(uint32_t off, uint32_t len)
{
    uint32_t a = (0x02000000u + off - 0x4000u) & ~31u, e = 0x02000000u + off - 0x4000u + len;
    for (uint32_t n = 0; n < 2000000u && !(CACHE_CON & (1u << 14)); n++) { }
    for (; a < e; a += 32u) {
        CSYNC();
        __asm__ volatile("flushinv [%0]" :: "r"(a) : "memory");
        CSYNC();
        for (uint32_t n = 0; n < 200000u && !(CACHE_CON & (1u << 14)); n++) { }
    }
}
