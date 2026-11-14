/* SPDX-License-Identifier: GPL-3.0-only
 * The FM-1's panel, polled: one column a call. Written for this project from the facts in fm1.h. */
#include "fm1.h"
#include "board.h"
#include "panel.h"

static uint8_t col, raw[PANEL_NCOL], cnt[PANEL_NKEY], led[PANEL_NCOL];
static uint64_t held;                                   /* debounced, bit = id */
static uint64_t pressed, released_;
static uint8_t enc_state[7], enc_rest[7];
static int8_t enc_acc[7];
static int16_t enc_steps[7];
static uint16_t enc_still[7];
static int8_t key_col[PANEL_NKEY], key_row[PANEL_NKEY];
/* the calibration (panel_map): which matrix button is each button's id, which matrix encoder each encoder's, and
 * its direction; as built: itself */
static uint8_t b_raw[14] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 }, b_id[14] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
static uint8_t e_raw[7] = { 0, 1, 2, 3, 4, 5, 6 };
static int8_t e_dir[7] = { 1, 1, 1, 1, 1, 1, 1 };
static uint32_t to_ids(uint32_t raw14)
{
    uint32_t o = 0;
    for (int i = 0; i < 14; i++) if ((raw14 >> i) & 1u) o |= 1u << b_id[i];
    return o;
}
int panel_map(const uint8_t btn[14], const uint8_t enc[7], const int8_t dir[7])
{
    uint8_t seen[14] = { 0 }, eseen[7] = { 0 };
    for (int i = 0; i < 14; i++) { if (btn[i] >= 14 || seen[btn[i]]) return -1; seen[btn[i]] = 1; }
    for (int i = 0; i < 7; i++) { if (enc[i] >= 7 || eseen[enc[i]] || (dir[i] != 1 && dir[i] != -1)) return -1; eseen[enc[i]] = 1; }
    for (int i = 0; i < 14; i++) { b_raw[i] = btn[i]; b_id[btn[i]] = (uint8_t)i; }
    for (int i = 0; i < 7; i++) { e_raw[i] = enc[i]; e_dir[i] = dir[i]; }
    return 0;
}
uint32_t panel_raw_buttons(void) { return (uint32_t)held & 0x3FFFu; }
int panel_raw_enc_take(int e) { int s = enc_steps[e]; enc_steps[e] = 0; return s; }

static void sr_word(uint32_t w)
{
    for (int i = 15; i >= 0; i--) {
        pin_set(KEY_DATA, (int)((w >> i) & 1u));
        pin_set(KEY_CLK, 1); pin_set(KEY_CLK, 0);
    }
    pin_set(KEY_LATCH, 1); pin_set(KEY_LATCH, 0);
}
static uint32_t col_word(int c) { return 0xFFFFu ^ (1u << c) ^ (c < 2 ? 1u << (11 + c) : 0u); }
static void leds_out(uint8_t m)                         /* packed row bits 1..4 -> PA9, PA10, PH6, PH9 */
{
    pin_set(PA, 9, (m >> 1) & 1); pin_set(PA, 10, (m >> 2) & 1);
    pin_set(PH, 6, (m >> 3) & 1); pin_set(PH, 9, (m >> 4) & 1);
}
static uint8_t rows_read(void)                           /* packed, 1 = closed */
{
    uint32_t a = GPIO(PA, G_IN), b = GPIO(PB, G_IN);
    return (uint8_t)((!(a & 1u)) | (!(a & (1u << 5))) << 1 | (!(a & (1u << 6))) << 2 |
                     (!(a & (1u << 7))) << 3 | (!(a & (1u << 8))) << 4 | (!(b & (1u << 7))) << 5);
}

void panel_init(void)
{
    pin_out(KEY_DATA, 1); pin_out(KEY_CLK, 0); pin_out(KEY_LATCH, 0);
    pin_out(LCD_BL, 0);                                  /* PA2 low: the matrix powered */
    pin_out(PA, 9, 0); pin_out(PA, 10, 0); pin_out(PH, 6, 0); pin_out(PH, 9, 0);
    pin_in_pullup(PA, 0); pin_in_pullup(PA, 5); pin_in_pullup(PA, 6); pin_in_pullup(PA, 7); pin_in_pullup(PA, 8);
    pin_in_pullup(PB, 7);
    for (int i = 0; i < PANEL_NKEY; i++) key_col[i] = -1;
    for (int r = 0; r < 6; r++) for (int c = 0; c < PANEL_NCOL; c++)
        if (PANEL_MAP[r][c] >= 0) { key_col[PANEL_MAP[r][c]] = (int8_t)c; key_row[PANEL_MAP[r][c]] = (int8_t)r; }
    /* one silent frame: the encoders' rest states */
    for (int c = 0; c < PANEL_NCOL; c++) { sr_word(col_word(c)); time_wait_us(10); raw[c] = rows_read(); }
    sr_word(0xFFFFu);
    for (int e = 0; e < 7; e++) {
        const uint8_t *m = PANEL_ENC[e];
        uint8_t s = (uint8_t)((((raw[m[0]] >> m[1]) & 1u) << 1) | ((raw[m[2]] >> m[3]) & 1u));
        enc_state[e] = enc_rest[e] = s;
    }
    col = 0;
}

static void frame_done(void)
{
    /* keys: asymmetric debounce, a press after 2 frames closed, a release after 8 open */
    for (int id = 0; id < PANEL_NKEY; id++) {
        int now = (raw[key_col[id]] >> key_row[id]) & 1, was = (int)((held >> id) & 1u);
        if (now == was) { cnt[id] = 0; continue; }
        if (++cnt[id] >= (now ? 2 : 8)) {
            cnt[id] = 0;
            if (now) { held |= 1ull << id; pressed |= 1ull << id; } else { held &= ~(1ull << id); released_ |= 1ull << id; }
        }
    }
    /* encoders: Gray-code transitions counted; a click when the knob comes back to its rest state having gone at
     * least half a cycle (a lost transition is forgiven, bounce and back-and-forth cancel out) */
    static const int8_t Q[16] = { 0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0 };
    for (int e = 0; e < 7; e++) {
        const uint8_t *m = PANEL_ENC[e];
        uint8_t s = (uint8_t)((((raw[m[0]] >> m[1]) & 1u) << 1) | ((raw[m[2]] >> m[3]) & 1u));
        if (s != enc_state[e]) {
            enc_acc[e] = (int8_t)(enc_acc[e] + Q[(enc_state[e] << 2) | s]);
            enc_state[e] = s; enc_still[e] = 0;
            if (s == enc_rest[e]) {
                if (enc_acc[e] >= 2) enc_steps[e]++; else if (enc_acc[e] <= -2) enc_steps[e]--;
                enc_acc[e] = 0;
            }
        } else if (s != enc_rest[e] && ++enc_still[e] > 900) {   /* ~1 s still elsewhere: that is its rest */
            enc_rest[e] = s; enc_acc[e] = 0; enc_still[e] = 0;
        }
    }
}

int panel_step(void)
{
    leds_out(0);                                          /* dark while the column changes and is read */
    sr_word(col_word(col));
    time_wait_us(10);
    raw[col] = rows_read();
    leds_out(led[col]);
    int done = 0;
    if (++col == PANEL_NCOL) { col = 0; frame_done(); done = 1; }
    else { /* the next call reads the next column: the LEDs of this one stay lit until then */ }
    return done;
}
void panel_dark(void) { leds_out(0); }

uint32_t panel_keys(void) { return (uint32_t)(held >> 14) & 0x7FFFFFFu; }
uint32_t panel_buttons(void) { return to_ids((uint32_t)held & 0x3FFFu); }
uint32_t panel_key_edges(uint32_t *rel)
{
    uint32_t p = (uint32_t)(pressed >> 14) & 0x7FFFFFFu;
    if (rel) *rel = (uint32_t)(released_ >> 14) & 0x7FFFFFFu;
    pressed &= 0x3FFFull; released_ &= 0x3FFFull;
    return p;
}
uint32_t panel_button_edges(uint32_t *rel)
{
    uint32_t p = to_ids((uint32_t)pressed & 0x3FFFu);
    if (rel) *rel = to_ids((uint32_t)released_ & 0x3FFFu);
    pressed &= ~0x3FFFull; released_ &= ~0x3FFFull;
    return p;
}
int panel_enc_take(int e) { int r = e_raw[e], s = enc_steps[r]; enc_steps[r] = 0; return s * e_dir[e]; }
void panel_led(int id, int on)
{
    if (id >= 0 && id < 14) id = b_raw[id];
    if (id < 0 || id >= PANEL_NKEY || key_row[id] < 1 || key_row[id] > 4) return;
    uint8_t b = (uint8_t)(1u << key_row[id]);
    if (on) led[key_col[id]] |= b; else led[key_col[id]] &= (uint8_t)~b;
}
