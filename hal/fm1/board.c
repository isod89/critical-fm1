/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1 board, first part: a time base (timer 0, free running), the panel (ST7789 on SPI1) with a text
 * console, and the PLAY / REC keys. Polled, no interrupt: enough for the bench and for a RAM app.
 * Written for this project from the wiring facts in fm1.h. */
#include "fm1.h"
#include "board.h"

static uint32_t lsb_mhz = 60;

/* ---------------------------------------------------------------- time */
void time_init(uint32_t lsb)
{
    lsb_mhz = lsb ? lsb : 60;
    T0_CON = 0x4000u; T0_CNT = 0; T0_PRD = 0xFFFFFFFFu; T0_CON = 1;
}
uint32_t time_ticks(void) { return T0_CNT; }
uint32_t time_mhz(void) { return lsb_mhz; }
void time_wait_us(uint32_t us)
{
    while (us > 1000u) { time_wait_us(1000u); us -= 1000u; }      /* steps of 1 ms: no long count */
    uint32_t t0 = T0_CNT, n = us * lsb_mhz;
    while ((uint32_t)(T0_CNT - t0) < n) ;
}

/* ---------------------------------------------------------------- the panel */
static void spi_wait(void)
{
    uint32_t t0 = T0_CNT;
    while (!(SPI1_CON & (1u << 15))) if ((uint32_t)(T0_CNT - t0) > 20000u * lsb_mhz) break;   /* 20 ms at most */
    SPI1_CON |= 1u << 14;
}
static void spi_byte(uint8_t b) { SPI1_BUF = b; spi_wait(); }
static void lcd_cmd(uint8_t c) { pin_set(LCD_DC, 0); pin_set(LCD_CS, 0); spi_byte(c); pin_set(LCD_CS, 1); }
static void lcd_data(const uint8_t *d, int n) { pin_set(LCD_DC, 1); pin_set(LCD_CS, 0); while (n--) spi_byte(*d++); pin_set(LCD_CS, 1); }
static void lcd_dma(const void *p, uint32_t n)                   /* n bytes from internal RAM, n > 0 */
{
    pin_set(LCD_DC, 1); pin_set(LCD_CS, 0);
    const uint8_t *b = (const uint8_t *)p;
    while (n) {
        uint32_t k = n > 0x8000u ? 0x8000u : n;
        SPI1_CON &= ~(1u << 12);                                  /* transmit */
        SPI1_ADR = (uint32_t)(uintptr_t)b; SPI1_CNT = k; spi_wait();
        b += k; n -= k;
    }
    pin_set(LCD_CS, 1);
}
static void lcd_window(int x, int y, int w, int h)
{
    uint8_t d[4];
    lcd_cmd(0x2A); d[0] = 0; d[1] = (uint8_t)x; d[2] = 0; d[3] = (uint8_t)(x + w - 1); lcd_data(d, 4);
    lcd_cmd(0x2B); d[0] = 0; d[1] = (uint8_t)y; d[2] = 0; d[3] = (uint8_t)(y + h - 1); lcd_data(d, 4);
    lcd_cmd(0x2C);
}
static uint16_t line[240];
void lcd_fill(int x, int y, int w, int h, uint16_t rgb565)
{
    for (int i = 0; i < w; i++) line[i] = (uint16_t)((rgb565 >> 8) | (rgb565 << 8));   /* high byte first */
    lcd_window(x, y, w, h);
    pin_set(LCD_DC, 1);
    for (int r = 0; r < h; r++) lcd_dma(line, (uint32_t)w * 2u);
}
/* a tile sent while the CPU goes on: the window set (a few bytes, polled), then the pixels by DMA */
static int tile_on;
void lcd_tile_start(int x, int y, int w, int h, const void *px)
{
    while (lcd_tile_busy()) ;
    lcd_window(x, y, w, h);
    pin_set(LCD_DC, 1); pin_set(LCD_CS, 0);
    SPI1_CON &= ~(1u << 12);                                      /* transmit */
    SPI1_ADR = (uint32_t)(uintptr_t)px; SPI1_CNT = (uint32_t)(w * h * 2);
    tile_on = 1;
}
int lcd_tile_busy(void)
{
    if (!tile_on) return 0;
    if (!(SPI1_CON & (1u << 15))) return 1;
    SPI1_CON |= 1u << 14;
    pin_set(LCD_CS, 1);
    tile_on = 0;
    return 0;
}
void lcd_init(void)
{
    pin_out(LCD_CS, 1); pin_out(LCD_DC, 0); pin_out(LCD_CLK, 0); pin_out(LCD_MOSI, 0);
    pin_out(LCD_BL, 1);                                            /* dark until the panel is clean */
    IOMAP_CON1 |= 1u << 4;
    SPI1_CON = 0x4021u; SPI1_BAUD = 4;                             /* 12 MHz at a 60 MHz LSB */
    lcd_cmd(0x01); time_wait_us(150000);
    lcd_cmd(0x11); time_wait_us(120000);
    uint8_t v = 0x55; lcd_cmd(0x3A); lcd_data(&v, 1);
    v = 0x00; lcd_cmd(0x36); lcd_data(&v, 1);
    lcd_cmd(0x21); lcd_cmd(0x13);
    lcd_fill(0, 0, 240, 240, 0x0000);
    lcd_cmd(0x29);
    pin_set(LCD_BL, 0);
}

/* our own 5 x 7 font: seven rows of five bits, the leftmost pixel in bit 4 */
static const char FONT_CH[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ .:-+/%=x()<>";
static const uint8_t FONT[][7] = {
    {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},
    {31,16,30,1,1,17,14},{6,8,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,2,12},
    {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{28,18,17,17,17,18,28},{31,16,16,30,16,16,31},
    {31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,2,18,12},
    {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,17,25,21,19,17,17},{14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},
    {31,1,2,4,8,16,31},{0,0,0,0,0,0,0},{0,0,0,0,0,12,12},{0,12,12,0,12,12,0},{0,0,0,31,0,0,0},
    {0,4,4,31,4,4,0},{1,2,2,4,8,8,16},{24,25,2,4,8,19,3},{0,0,31,0,31,0,0},{0,0,17,10,4,10,17},
    {2,4,8,8,8,4,2},{8,4,2,2,2,4,8},{2,4,8,16,8,4,2},{8,4,2,1,2,4,8},
};
static uint16_t cell[18 * 24];
/* one character at pixel (x, y), the 5 x 7 font scaled by 2 (a 12 x 16 cell) or 3 (18 x 24): one window, one DMA */
void lcd_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    uint16_t f = (uint16_t)((fg >> 8) | (fg << 8)), b = (uint16_t)((bg >> 8) | (bg << 8));
    int w = 6 * scale, h = 8 * scale;
    if (c >= 'a' && c <= 'z' && c != 'x') c = (char)(c - 32);
    int g = 36;
    for (int i = 0; FONT_CH[i]; i++) if (FONT_CH[i] == c) { g = i; break; }
    for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) {
        int on = 0, fx = (xx - (scale >> 1)) / scale, fy = (yy - (scale >> 1)) / scale;   /* a margin round the glyph */
        if (xx >= (scale >> 1) && yy >= (scale >> 1) && fx < 5 && fy < 7) on = (FONT[g][fy] >> (4 - fx)) & 1;
        cell[yy * w + xx] = on ? f : b;
    }
    lcd_window(x, y, w, h);
    lcd_dma(cell, (uint32_t)(w * h * 2));
}
void lcd_text(int col, int row, const char *s, uint16_t fg, uint16_t bg)
{
    for (; *s && col < 20; s++, col++) lcd_char(col * 12, row * 16, *s, fg, bg, 2);
}

/* ---------------------------------------------------------------- keys */
static void key_word(uint32_t w)
{
    for (int i = 15; i >= 0; i--) {
        pin_set(KEY_DATA, (int)((w >> i) & 1u));
        pin_set(KEY_CLK, 1); pin_set(KEY_CLK, 0);
    }
    pin_set(KEY_LATCH, 1); pin_set(KEY_LATCH, 0);
}
void keys_init(void)
{
    pin_out(KEY_DATA, 1); pin_out(KEY_CLK, 0); pin_out(KEY_LATCH, 0);
    pin_in_pullup(KEY_ROW_PLAY);
    pin_out(LCD_BL, 0);                                            /* PA2 low powers the matrix (and the backlight) */
    key_word(0xFFFFu);
}
int key_down(uint32_t col)
{
    key_word(col); time_wait_us(10);
    int down = !pin_read(KEY_ROW_PLAY);
    key_word(0xFFFFu);
    return down;
}
