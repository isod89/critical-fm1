/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: the screen (screen.h). A reactor's control desk, as its instruments would show it:
 *   THE DESK   (any page but HOME) the core's cartogram: the sixteen channels round the ring, each cell lit by its
 *              channel's neutrons, as a detector counts them: a few counts are cells that blink, at power the cells
 *              glow, bluer and whiter as Cherenkov light does, amber past full power; the rod channels show their
 *              rods (lit: withdrawn). Beside it the neutron instruments: the power on eight decades, the period and
 *              the startup rate, the reactivity in dollars (prompt critical: one dollar), the operating margin,
 *              the xenon, the steam, the bank and its regulator. Under it the four knobs of the page, twelve
 *              annunciators, the eleven black keys
 *   THE BOARD  (HOME) all that is drawn, in two pages (the notes, the plant)
 *   THE RODS   (HOME again) the wall of rod position indicators: a dial a rod, round the ring as the channels are,
 *              the bank's in the middle; the recorder: the power, the reactivity, the heat, the xenon
 * Nothing here is a score or a goal: the screen shows what sounds, and what the hands can do about it. */
#include "screen.h"
#include "../app/gfx.h"
#include "../core/cmath.h"

#define VERSION "4.0"
#define PAINT  RGB(46, 54, 50)                  /* the desk's paint (a Soviet control room's grey-green) */
#define PAINT2 RGB(36, 42, 39)
#define BEZEL  RGB(12, 14, 14)
#define SCREW  RGB(92, 98, 92)
#define FACE   RGB(226, 216, 186)               /* a dial's face, the recorder's paper */
#define FACE2  RGB(200, 190, 160)
#define INK    RGB(34, 30, 26)
#define NEEDLE RGB(170, 28, 20)
#define PLATE  RGB(16, 18, 18)
#define ENGR   RGB(222, 214, 190)
#define OFFW   RGB(60, 58, 50)
#define OFFT   RGB(124, 118, 98)
#define AMB    RGB(255, 178, 36)
#define RED    RGB(236, 56, 40)
#define GRN    RGB(84, 214, 110)
#define DIMG   RGB(34, 92, 50)
#define WHT    RGB(244, 240, 222)
#define GLASS  RGB(6, 9, 12)
#define CHER   RGB(70, 150, 255)
#define SEL    RGB(70, 52, 16)

static int blink, tick4;
static char *s_cat(char *b, const char *s) { while (*s) *b++ = *s++; *b = 0; return b; }
static char *s_int(char *b, int v)
{
    char t[12]; int n = 0;
    if (v < 0) { *b++ = '-'; v = -v; }
    do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *b++ = t[--n];
    *b = 0; return b;
}
static char *s_fix(char *b, float v, int dec, int sign)    /* a number with dec decimals (and its sign, if asked) */
{
    int m = dec == 0 ? 1 : dec == 1 ? 10 : dec == 2 ? 100 : 1000, x = (int)(c_absf(v) * (float)m + 0.5f);
    if (v < 0 && x) *b++ = '-'; else if (sign) *b++ = '+';
    b = s_int(b, x / m);
    if (dec) { *b++ = '.'; int p = m / 10; x %= m; while (p) { *b++ = (char)('0' + x / p); x %= p; p /= 10; } }
    *b = 0; return b;
}
static char *s_sci(char *b, float v)          /* 2.2E-03 */
{
    if (v < 1e-9f) return s_cat(b, "0.0E+00");
    int e = 0; while (v >= 10.0f) { v *= 0.1f; e++; } while (v < 1.0f) { v *= 10.0f; e--; }
    b = s_fix(b, v, 1, 0); *b++ = 'E'; *b++ = e < 0 ? '-' : '+'; if (e < 0) e = -e; *b++ = (char)('0' + e / 10); *b++ = (char)('0' + e % 10); *b = 0; return b;
}
static void ln(int x0, int y0, int x1, int y1, int t, uint16_t c)
{
    int dx = x1 - x0, dy = y1 - y0, n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    if (n == 0) { gfx_fill(x0, y0, t, t, c); return; }
    for (int i = 0; i <= n; i++) gfx_fill(x0 + dx * i / n, y0 + dy * i / n, t, t, c);
}
static void screws(int x, int y, int w, int h) { gfx_fill(x + 2, y + 2, 2, 2, SCREW); gfx_fill(x + w - 4, y + 2, 2, 2, SCREW); gfx_fill(x + 2, y + h - 4, 2, 2, SCREW); gfx_fill(x + w - 4, y + h - 4, 2, 2, SCREW); }
static float lg(float n) { return c_log2(n > 1e-12f ? n : 1e-12f) * 0.30103f; }
/* a channel's light: dark, deep blue, Cherenkov blue, white; past full power amber, then red */
static uint16_t cher(float n)
{
    if (n > 1.2f) { float u = ((n - 1.2f) / 1.5f > 1.0f ? 1.0f : (n - 1.2f) / 1.5f); return RGB(255, (int)(200 - 140 * u), (int)(120 - 100 * u)); }
    float L = (lg(n) + 7.0f) / 7.0f; L = L < 0 ? 0 : L > 1 ? 1 : L;
    int r, gg, b;
    if (L < 0.35f) { float u = L / 0.35f; r = 6 + (int)(6 * u); gg = 10 + (int)(22 * u); b = 18 + (int)(70 * u); }
    else if (L < 0.7f) { float u = (L - 0.35f) / 0.35f; r = 12 + (int)(28 * u); gg = 32 + (int)(78 * u); b = 88 + (int)(132 * u); }
    else if (L < 0.9f) { float u = (L - 0.7f) / 0.2f; r = 40 + (int)(70 * u); gg = 110 + (int)(90 * u); b = 220 + (int)(35 * u); }
    else { float u = (L - 0.9f) / 0.1f; r = 110 + (int)(120 * u); gg = 200 + (int)(45 * u); b = 255; }
    return RGB(r, gg, b);
}
static uint32_t hash2(uint32_t a, uint32_t b) { a = a * 2654435761u ^ b * 40503u; a ^= a >> 13; a *= 2246822519u; a ^= a >> 16; return a; }
static int since(const inst *g, int sg, uint32_t ms) { return g->sig_ms[sg] && (uint32_t)(g->now_ms - g->sig_ms[sg]) < ms; }

/* ---------------------------------------------------------------- the nameplate */
static void nameplate(const inst *g)
{
    if (!gfx_visible(0, 0, GFX_W, 22)) return;
    gfx_fill(0, 0, GFX_W, 22, PAINT2);
    const char *nm = g->sel == PG_BOARD ? (g->page ? "THE RODS" : g->cur < NROW_NOTES ? "THE NOTES" : "THE PLANT") : inst_page_name[g->sel];
    gfx_rfill(3, 2, 92, 18, 2, PLATE); gfx_frame(3, 2, 92, 18, 1, g->rx.trip ? RED : CHER);
    gfx_text_c(&font_hdr, 49, 4, nm, ENGR);
    if (g->toast[0] && (int32_t)(g->toast_until - g->now_ms) > 0) {
        gfx_rfill(98, 2, 139, 18, 2, SEL); gfx_text_c(&font_lbl, 167, 5, g->toast, AMB);
    } else {
        char b[40], *e = s_cat(b, inst_note_name[g->root]); *e++ = ' '; s_cat(e, inst_scale_name[g->scale]);
        gfx_text_r(&font_dat, 236, 1, inst_scene_name[g->scene], ENGR);
        gfx_text_r(&font_dat, 236, 11, b, OFFT);
    }
}

/* ---------------------------------------------------------------- the cartogram: the ring of channels */
static int ring_pos(float dx, float dy)      /* the ring position (0..15) of a point round the middle: clockwise from the right, as the rods' wall */
{
    float ax = c_absf(dx), ay = c_absf(dy), t = ax > ay ? ay / (ax + 1e-6f) : ax / (ay + 1e-6f);
    float a = t * (0.7854f + 0.2732f * (1 - t));                         /* atan t on [0, 1] */
    if (ay > ax) a = 1.5708f - a;
    if (dx < 0) a = 3.14159f - a;
    if (dy < 0) a = 6.28318f - a;
    int p = (int)((a + 0.19635f) * (16.0f / 6.28318f)); return p & 15;
}
#define CN 14
#define CP 8
static void cartogram(const inst *g, int x0, int y0)
{
    const int W = CN * CP;
    if (!gfx_visible(x0 - 3, y0 - 3, W + 6, W + 6)) return;
    gfx_rfill(x0 - 3, y0 - 3, W + 6, W + 6, 3, BEZEL);
    for (int cy = 0; cy < CN; cy++) for (int cx = 0; cx < CN; cx++) {
        float dx = (float)cx - (CN - 1) * 0.5f, dy = (float)cy - (CN - 1) * 0.5f, rr = dx * dx + dy * dy;
        int px = x0 + cx * CP, py = y0 + cy * CP;
        if (rr > 6.9f * 6.9f || !gfx_visible(px, py, CP, CP)) continue;
        if (rr < 2.3f * 2.3f) {                                          /* the middle: graphite, and the regulator's rods */
            uint16_t c = RGB(30, 32, 30);
            if ((cx + cy) % 2 == 0) c = g->rx.ar ? (blink && g->tension > 0.2f ? AMB : DIMG) : OFFW;
            if (g->az) c = RED;
            gfx_fill(px, py, CP - 1, CP - 1, c); continue;
        }
        int p = ring_pos(dx, dy), j = (3 * p) & 15;                       /* (the ring is laid out in fourths: position p holds channel 3p) */
        float n = g->rx.n[j];
        uint16_t c;
        if (n < 2e-3f) {                                                  /* few counts: cells blink, one by one */
            uint32_t h = hash2((uint32_t)(cx * 31 + cy), (uint32_t)tick4);
            float pr = n * 400.0f; c = (float)(h & 1023u) < pr * 1023.0f ? cher(n * 60.0f) : cher(n * 0.2f);
        } else {
            uint32_t h = hash2((uint32_t)(cx * 31 + cy), (uint32_t)tick4);
            c = cher(n * (0.75f + 0.5f * (float)(h & 255u) * (1.0f / 255.0f)));
        }
        gfx_fill(px, py, CP - 1, CP - 1, c);
        if ((cx + 2 * cy) % 5 == 0) {                                     /* a rod channel: its rod, lit when withdrawn */
            float r = g->rx.rod[j]; gfx_fill(px + 1, py + 1, CP - 3, CP - 3, BEZEL);
            gfx_fill(px + 2, py + 2, CP - 5, CP - 5, r > 0.5f ? (g->rx.trip ? RED : WHT) : r > 0.02f ? AMB : RGB(54, 54, 50));
        }
    }
}

/* ---------------------------------------------------------------- the neutron instruments */
static void box(int x, int y, int w, int h) { gfx_rfill(x, y, w, h, 2, BEZEL); }
static void instruments(const inst *g, int x, int y)
{
    const int w = 120; char b[24], *e;
    if (gfx_visible(x, y, w, 31)) {                                      /* the power: eight decades */
        box(x, y, w, 31);
        gfx_text(&font_lbl, x + 5, y + 2, "N", OFFT);
        s_sci(b, g->rx.power); gfx_text_r(&font_dat, x + w - 5, y + 3, b, g->rx.power > 1.0f ? RED : ENGR);
        int bx = x + 6, bw = w - 12, by = y + 18;
        gfx_fill(bx, by, bw, 7, GLASS);
        float u = (lg(g->rx.power) + 7.0f) / 8.0f; u = u < 0 ? 0 : u > 1 ? 1 : u;
        gfx_fill(bx, by + 1, (int)((float)bw * u), 5, cher(g->rx.power));
        for (int d = 0; d <= 8; d++) gfx_fill(bx + bw * d / 8, by + (d == 7 ? -2 : 0), 1, d == 7 ? 11 : 9, d == 7 ? RED : OFFT);    /* (full power: the red tick) */
    }
    if (gfx_visible(x, y + 33, w, 24)) {                                 /* the period, the startup rate */
        box(x, y + 33, w, 24);
        float p = g->rx.period; e = b;
        if (p == 0 || c_absf(p) > 999.0f) s_cat(b, "STEADY"); else { e = s_fix(b, p, c_absf(p) < 10 ? 1 : 0, 1); s_cat(e, " S"); }
        gfx_text(&font_lbl, x + 5, y + 35, "PERIOD", OFFT); gfx_text_r(&font_dat, x + w - 5, y + 35, b, p > 0 && p < 3 ? (blink ? RED : AMB) : ENGR);
        { float sr = g->rx.sur; if (sr > 99.9f) sr = 99.9f; if (sr < -99.9f) sr = -99.9f; e = s_fix(b, sr, 1, 1); s_cat(e, " DPM"); }
        gfx_text(&font_lbl, x + 5, y + 46, "SUR", OFFT); gfx_text_r(&font_dat, x + w - 5, y + 46, b, OFFT);
    }
    if (gfx_visible(x, y + 59, w, 24)) {                                 /* the reactivity, in dollars: one is prompt critical */
        box(x, y + 59, w, 24);
        e = s_cat(b, "$ "); s_fix(e, g->rx.rho, 2, 1);
        gfx_text(&font_lbl, x + 5, y + 60, "RHO", OFFT); gfx_text_r(&font_dat, x + w - 5, y + 61, b, g->rx.prompt ? RED : ENGR);
        int bx = x + 6, bw = w - 12, by = y + 74, mid = bx + bw * 2 / 3;
        gfx_fill(bx, by, bw, 5, GLASS);
        float r = g->rx.rho; if (r < -2) r = -2; if (r > 1) r = 1;
        int px = mid + (int)(r * (float)(bw / 3)); if (px < bx) px = bx;
        if (px > mid) gfx_fill(mid, by + 1, px - mid, 3, r > 0.8f ? RED : AMB); else gfx_fill(px, by + 1, mid - px, 3, CHER);
        gfx_fill(mid, by - 2, 1, 9, OFFT); gfx_fill(bx + bw - 1, by - 2, 1, 9, RED);
    }
    if (gfx_visible(x, y + 85, w, 24)) {                                 /* the margin, the xenon, the steam, the bank */
        box(x, y + 85, w, 24);
        float xe = 0; for (int j = 0; j < SY_NZ; j++) xe += g->rx.xe[j]; xe /= SY_NZ;
        e = s_cat(b, "ORM "); s_fix(e, g->rx.orm, 0, 0); gfx_text(&font_dat, x + 5, y + 87, b, g->rx.orm < 8 ? (blink ? RED : AMB) : ENGR);
        e = s_cat(b, "XE "); s_fix(e, xe, 2, 0); gfx_text_r(&font_dat, x + w - 5, y + 87, b, xe > 0.5f ? AMB : OFFT);
        gfx_text(&font_lbl, x + 5, y + 97, "VOID", OFFT); s_fix(b, g->rx.voids, 2, 0); gfx_text(&font_dat, x + 30, y + 98, b, g->rx.voids > 0.15f ? AMB : OFFT);
        gfx_text(&font_lbl, x + 67, y + 97, g->rx.ar ? "AR" : "BK", g->rx.ar ? GRN : OFFT); s_fix(b, g->rx.bank, 2, 1); gfx_text_r(&font_dat, x + w - 5, y + 98, b, g->rx.ar ? GRN : OFFT);
    }
}

/* ---------------------------------------------------------------- the four knobs: small dials */
static void dial(int x, int y, const char *label, float v, int hot)
{
    const int w = 57, h = 37;
    if (!gfx_visible(x, y, w, h)) return;
    gfx_rfill(x, y, w, h, 3, BEZEL); if (hot) gfx_rframe(x, y, w, h, 3, 1, AMB);
    if (!label[0] || v < 0) { gfx_fill(x + 3, y + 3, w - 6, h - 6, PAINT2); return; }
    gfx_text_c(&font_lbl, x + w / 2, y + 2, label, hot ? AMB : OFFT);
    int cx = x + 15, cy = y + 25; gfx_disc(cx, cy, 10, FACE);
    for (int i = 0; i <= 8; i++) { float a = (-135.0f + 33.75f * (float)i) * (C_PI / 180.0f); ln(cx + (int)(c_sin(a) * 7.0f), cy - (int)(c_cos(a) * 7.0f), cx + (int)(c_sin(a) * 9.0f), cy - (int)(c_cos(a) * 9.0f), 1, INK); }
    { float vv = v < 0 ? 0 : v > 1 ? 1 : v, a = (-135.0f + 270.0f * vv) * (C_PI / 180.0f); ln(cx, cy, cx + (int)(c_sin(a) * 8.0f), cy - (int)(c_cos(a) * 8.0f), 1, NEEDLE); }
    gfx_disc(cx, cy, 2, INK);
    char b[8]; s_int(b, (int)(v * 100.0f + 0.5f)); gfx_text_r(&font_dat, x + w - 4, y + 20, b, hot ? AMB : ENGR);
}

/* ---------------------------------------------------------------- the annunciators */
static void tile(int i, const char *a, uint16_t lit)
{
    int x = 2 + (i % 6) * 39, y = 177 + (i / 6) * 12;
    if (!gfx_visible(x, y, 38, 11)) return;
    gfx_fill(x, y, 38, 11, lit ? lit : OFFW);
    gfx_text_c(&font_lbl, x + 19, y + 1, a, lit ? INK : OFFT);
}
static void annunciators(const inst *g)
{
    if (!gfx_visible(0, 176, GFX_W, 25)) return;
    gfx_fill(0, 176, GFX_W, 25, PAINT2);
    const sy_rx *x = &g->rx; float pk = 0, xe = 0; for (int j = 0; j < SY_NZ; j++) { if (x->n[j] > pk) pk = x->n[j]; xe += x->xe[j]; }
    int gen = g->P.gest[F_GEN] > 0.01f, gsnd = 0; for (int j = 0; j < SY_NZ; j++) if (g->gen_off[j]) gsnd = 1;
    tile(0, "AR", x->ar ? GRN : 0);
    tile(1, "PER", x->period > 0 && x->period < 3.0f ? (blink ? RED : AMB) : x->period > 0 && x->period < 10.0f ? AMB : 0);
    tile(2, "OVER", pk > 1.2f ? (blink ? RED : AMB) : pk > 0.9f ? AMB : 0);
    tile(3, "PRMT", x->prompt ? RED : 0);
    tile(4, "VOID", g->P.rx[1] > 0.55f ? (x->voids > 0.15f ? (blink ? RED : AMB) : AMB) : 0);
    tile(5, "XENON", xe / SY_NZ > 0.5f ? AMB : xe / SY_NZ > 0.2f ? DIMG : 0);
    tile(6, "ORM", x->orm < 8 ? (blink ? RED : AMB) : 0);
    tile(7, "AZ-5", g->az || x->trip ? RED : g->rec_held ? AMB : 0);
    tile(8, g->P.blackout ? "LOST" : "GRID", g->P.blackout ? RED : g->plant[P_HUM] > 0.3f ? GRN : 0);
    tile(9, since(g, SIG_TRIP, 5000) ? "RUN" : "TURB", since(g, SIG_TRIP, 5000) ? RED : g->heat > 0.4f ? AMB : DIMG);
    tile(10, g->hold ? "HOLD" : "GEN", g->hold ? GRN : gen ? (gsnd ? WHT : AMB) : 0);
    tile(11, "LIMIT", g->limit > 0.04f ? RED : 0);
}

/* ---------------------------------------------------------------- the black keys, as they lie: 3, 2, 3, 2, 1 */
static int key_lit(const inst *g, int b) { return g->bk_held == b || since(g, b, 700) || (b == SIG_BLACKOUT && g->P.blackout) || (b == SIG_STEPS && g->walk > 0.01f); }
static void keys(const inst *g)
{
    static const char *const AB[NSIG] = { "STP", "DOR", "GRN", "RLY", "BLK", "RSR", "SWL", "DRP", "BRM", "STG", "TRP" };
    static const uint8_t GAP[NSIG] = { 0, 1, 1, 5, 1, 5, 1, 1, 5, 1, 5 };
    if (!gfx_visible(0, 203, GFX_W, 15)) return;
    gfx_fill(0, 202, GFX_W, 16, PAINT);
    int x = 1;
    for (int b = 0; b < NSIG; b++) {
        x += GAP[b]; int lit = key_lit(g, b), f = inst_sig_fam(b);
        gfx_rfill(x, 203, 19, 14, 2, lit ? (b == SIG_BLACKOUT || b == SIG_TRIP ? RED : AMB) : BEZEL);
        gfx_text_c(&font_lbl, x + 10, 206, AB[b], lit ? INK : f >= 0 && g->lock[f] ? GRN : OFFT);
        x += 19;
    }
}
static void keycard(const inst *g, int b)     /* a black key's card: its kinds, the one it has, free or locked */
{
    const int x = 2, y = 137, w = 236, h = 63;
    if (!gfx_visible(x, y, w, h)) return;
    int f = inst_sig_fam(b);
    gfx_rfill(x, y, w, h, 3, BEZEL); screws(x, y, w, h);
    gfx_text(&font_hdr, x + 9, y + 6, inst_sig_name[b], ENGR);
    if (f < 0) { gfx_text(&font_lbl, x + 9, y + 34, "THE STEAM IS CUT: THE TURBINE RUNS DOWN", OFFT); return; }
    gfx_text_r(&font_lbl, x + w - 9, y + 5, g->lock[f] ? "LOCKED" : "FREE: A NEW PLANT DRAWS IT", g->lock[f] ? GRN : OFFT);
    gfx_text_r(&font_lbl, x + w - 9, y + 16, "HOLD THE KEY, TURN PRESETS", OFFT);
    int n = sy_nkind[f], tw = (w - 12 - (n - 1) * 3) / n;
    for (int k = 0; k < n; k++) {
        int tx = x + 6 + k * (tw + 3), on = g->P.kind[f] == k;
        gfx_fill(tx, y + 38, tw, 15, on ? (g->lock[f] ? GRN : AMB) : OFFW);
        gfx_text_c(&font_lbl, tx + tw / 2, y + 41, sy_kind_name[f][k], on ? INK : OFFT);
    }
}

/* ---------------------------------------------------------------- the status line */
static void status(const inst *g)
{
    if (!gfx_visible(0, 219, GFX_W, 21)) return;
    gfx_fill(0, 219, GFX_W, 21, PAINT2);
    char b[48], *e = s_cat(b, VERSION);
    if (g->load_pct > 0) { e = s_cat(e, "  CPU "); e = s_int(e, (int)g->load_pct); e = s_cat(e, "%  PEAK "); e = s_int(e, (int)g->load_peak); s_cat(e, "%"); }
    gfx_text(&font_lbl, 5, 223, b, OFFT);
    for (int i = 0; i < 10; i++) { int on = g->out * 11.0f > (float)i + 0.5f; gfx_fill(172 + i * 6, 224, 4, 9, on ? (i >= 8 ? RED : i >= 6 ? AMB : GRN) : OFFW); }
}

/* ---------------------------------------------------------------- THE DESK */
static void desk(const inst *g)
{
    if (gfx_visible(0, 22, GFX_W, 180)) gfx_fill(0, 22, GFX_W, 180, PAINT);
    cartogram(g, 6, 27);
    instruments(g, 118, 24);
    if (g->key_ms && (g->bk_held >= 0 || (uint32_t)(g->now_ms - g->key_ms) < 3000u)) keycard(g, g->key_last);
    else {
        int hot = (uint32_t)(g->now_ms - g->knob_ms) < 1200u && g->knob_ms ? g->knob_i : -1;
        for (int i = 0; i < 4; i++) dial(3 + i * 59, 137, inst_knob_name(g, i), inst_knob_value(g, i), i == hot);
        annunciators(g);
    }
    keys(g);
}

/* ---------------------------------------------------------------- THE BOARD */
static uint16_t row_lamp(const inst *g, int r)
{
    static const int8_t SG[SY_NFAM] = { -1, -1, -1, -1, SIG_RELAYS, SIG_BLACKOUT, SIG_GROAN, SIG_STEPS, SIG_DOOR, SIG_RISER, SIG_SWELL, SIG_DROP, SIG_BRAAM, SIG_STINGER, -1, -1, -1 };
    int f = inst_row_fam[r], act;
    if (f == F_FUEL || f == -4 || f == -3) { int s = f == F_FUEL ? SRC_FUEL : f == -4 ? SRC_MOD : SRC_HALL; return !g->P.on[s] || inst_row_level(g, r) < 0.01f ? OFFW : g->level[s] > 0.03f ? AMB : DIMG; }
    if (inst_row_level(g, r) < 0.01f) return OFFW;
    if (f == F_GEN) { for (int j = 0; j < SY_NZ; j++) if (g->gen_off[j]) return AMB; return DIMG; }
    if (f == F_BREAKER && g->P.blackout) return RED;
    act = f == F_HUM ? g->heat > 0.45f : f == F_TURBINE || f == F_SHIFT ? g->heat > 0.3f : f == F_PUMP ? g->plant[P_PUMP] > 0.25f : f == F_GEIGER ? g->plant[P_GEIGER] > 0.12f
        : f == F_RELAY ? g->plant[P_RELAY] > 0.05f : f == F_GROAN ? g->plant[P_GROAN] > 0.05f : f == -2 ? g->plant[P_GHOST] > 0.05f : f == -1 ? g->heat > 0.5f : 0;
    if (f >= 0 && SG[f] >= 0 && key_lit(g, SG[f])) act = 1;
    return act ? AMB : DIMG;
}
static const char *const WHAT_FUEL[4][3] = { { "THE COUNTS, STRUCK", "INTO EACH CHANNEL'S", "HARMONICS" }, { "BOWED METAL: PARTIALS", "THAT ARE NO SERIES", "(A WATERPHONE)" },
    { "A BAR STRUCK: GLASS", "PARTIALS, HIGH", "AND FAR APART" }, { "EACH NOTE PULLED", "ONTO THE MAINS'", "HARMONICS (50 HZ)" } };
static const char *const WHAT_SHIFT[3][3] = { { "EVERY FREQUENCY", "MOVED DOWN: MORE", "AS IT HEATS" }, { "EVERY FREQUENCY", "MOVED UP: MORE", "AS IT HEATS" }, { "DOWN IN ONE EAR,", "UP IN THE OTHER", "" } };
static const char *const WHAT_GEN[7][3] = { { "TWO NOTES", "A STEP APART,", "CLOSING IN" }, { "A SLOW LINE", "THAT KEEPS", "FALLING" }, { "A NURSERY TUNE,", "REMEMBERED", "WRONG" }, { "THE OLD CHANT", "OF THE DAY", "OF WRATH" },
    { "THE ROOT,", "AND ITS TRITONE", "" }, { "A STEP AGAINST", "EACH CHANNEL", "YOU HOLD" }, { "ONE NOTE, IN FIVE,", "FASTER AS IT", "HEATS" } };
static void row_tile(const inst *g, int r, int x, int y)
{
    const int w = 117, h = 18; int f = inst_row_fam[r], cur = r == g->cur;
    if (!gfx_visible(x, y, w, h)) return;
    gfx_rfill(x, y, w, h, 2, cur ? SEL : PLATE); if (cur) gfx_rframe(x, y, w, h, 2, 1, AMB);
    gfx_fill(x + 4, y + 4, 4, 10, row_lamp(g, r));
    gfx_text(&font_lbl, x + 12, y + 5, inst_row_name(r), cur ? AMB : ENGR);
    if (f >= 0) {
        gfx_text_r(&font_lbl, x + w - 11, y + 5, sy_kind_name[f][g->P.kind[f]], g->lock[f] ? GRN : OFFT);
        if (g->lock[f]) gfx_fill(x + w - 8, y + 7, 4, 4, GRN);
    } else { char b[8]; s_int(b, (int)(inst_row_level(g, r) * 100.0f + 0.5f)); gfx_text_r(&font_lbl, x + w - 11, y + 5, b, OFFT); }
}
static void board(const inst *g)
{
    if (gfx_visible(0, 22, GFX_W, 197)) gfx_fill(0, 22, GFX_W, 197, PAINT);
    if (g->cur < NROW_NOTES) {
        for (int r = 0; r < NROW_NOTES; r++) row_tile(g, r, 2, 24 + r * 19);
        const int x = 121, y = 24, w = 117, h = 151; int f = inst_row_fam[g->cur];
        if (gfx_visible(x, y, w, h)) {
            gfx_rfill(x, y, w, h, 3, BEZEL); screws(x, y, w, h);
            gfx_text_c(&font_hdr, x + w / 2, y + 8, inst_row_name(g->cur), ENGR);
            if (f >= 0) {
                int k = g->P.kind[f], n = sy_nkind[f]; const char *const *wt = f == F_FUEL ? WHAT_FUEL[k] : f == F_SHIFT ? WHAT_SHIFT[k] : WHAT_GEN[k];
                gfx_text_c(&font_lbl, x + w / 2, y + 29, sy_kind_name[f][k], g->lock[f] ? GRN : AMB);
                for (int i = 0; i < 3; i++) gfx_text_c(&font_lbl, x + w / 2, y + 46 + i * 11, wt[i], OFFT);
                for (int i = 0; i < n; i++) gfx_fill(x + w / 2 - n * 5 + i * 10, y + 83, 7, 4, i == k ? (g->lock[f] ? GRN : AMB) : OFFW);
                gfx_text_c(&font_lbl, x + w / 2, y + 93, g->lock[f] ? "LOCKED: IT STAYS" : "FREE: PLAY DRAWS IT", g->lock[f] ? GRN : OFFT);
                gfx_text_c(&font_lbl, x + w / 2, y + 106, f == F_GEN ? "LEVEL: HOW MUCH" : f == F_SHIFT ? "LEVEL: HOW FAR" : "VARIANT: THE LOADING", OFFT);
                if (f == F_GEN) gfx_text_c(&font_lbl, x + w / 2, y + 117, "SEQ: ON, OFF", OFFT);
                if (f == F_FUEL) gfx_text_c(&font_lbl, x + w / 2, y + 117, "OF THE CORE", OFFT);
            } else {
                static const char *const MD[3] = { "ONE DEEP VOICE", "UNDER THE HOTTEST", "CHANNEL" }, *const HL[3] = { "SIX SECONDS, PLAYED", "BACK INTO A ROOM", "THAT KEEPS ITS OWN" };
                for (int i = 0; i < 3; i++) gfx_text_c(&font_lbl, x + w / 2, y + 46 + i * 11, f == -4 ? MD[i] : HL[i], OFFT);
            }
        }
    } else for (int r = NROW_NOTES; r < NROW; r++) row_tile(g, r, 2 + ((r - NROW_NOTES) / 8) * 119, 24 + ((r - NROW_NOTES) % 8) * 19);
    int f = inst_row_fam[g->cur], hot = (uint32_t)(g->now_ms - g->knob_ms) < 1200u && g->knob_ms ? g->knob_i : -1;
    for (int i = 0; i < 4; i++) {
        int x = 3 + i * 59, y = 178, w = 57, h = 39; float v = -1; const char *t = 0; char b[8];
        if (!gfx_visible(x, y, w, h)) continue;
        gfx_rfill(x, y, w, h, 3, BEZEL); if (i == hot) gfx_rframe(x, y, w, h, 3, 1, AMB);
        gfx_text_c(&font_lbl, x + w / 2, y + 5, inst_knob_name(g, i), OFFT);
        if (g->mixer) v = inst_knob_value(g, i);
        else if (i == 0) t = inst_row_name(g->cur);
        else if (i == 3) v = inst_row_level(g, g->cur);
        else if (f < 0) t = "-";
        else if (i == 1) t = g->lock[f] ? sy_kind_name[f][g->P.kind[f]] : "FREE";
        else if (!inst_row_variant(g->cur)) t = "-";
        else v = (float)g->P.gene[f] * (1.0f / 255.0f);
        if (t) gfx_text_c(&font_lbl, x + w / 2, y + 19, t, i == hot ? AMB : i == 1 && f >= 0 && g->lock[f] ? GRN : ENGR);
        else { s_int(b, (int)(v * 100.0f + 0.5f)); gfx_text_c(&font_dat, x + w / 2, y + 17, b, i == hot ? AMB : ENGR);
               gfx_fill(x + 6, y + 31, w - 12, 3, OFFW); gfx_fill(x + 6, y + 31, (int)((float)(w - 12) * (v < 0 ? 0 : v > 1 ? 1 : v)), 3, i == hot ? AMB : OFFT); }
    }
}

/* ---------------------------------------------------------------- THE RODS: the wall of position indicators, the recorder */
static void rod_dial(int cx, int cy, int r, float pos, float n, const char *nm, int trip)
{
    if (!gfx_visible(cx - r - 3, cy - r - 3, 2 * r + 6, 2 * r + 16)) return;
    gfx_disc(cx, cy, r + 2, cher(n));                                   /* (its ring: its channel's light) */
    gfx_disc(cx, cy, r, FACE);
    for (int i = 0; i <= 4; i++) { float a = (-135.0f + 67.5f * (float)i) * (C_PI / 180.0f); ln(cx + (int)(c_sin(a) * (float)(r - 3)), cy - (int)(c_cos(a) * (float)(r - 3)), cx + (int)(c_sin(a) * (float)(r - 1)), cy - (int)(c_cos(a) * (float)(r - 1)), 1, INK); }
    float a = (-135.0f + 270.0f * (1.0f - pos)) * (C_PI / 180.0f);      /* (out: the needle to the left, at the top; in: to the right) */
    ln(cx, cy, cx + (int)(c_sin(a) * (float)(r - 2)), cy - (int)(c_cos(a) * (float)(r - 2)), 1, trip ? RED : NEEDLE);
    gfx_disc(cx, cy, 1, INK);
    if (nm) gfx_text_c(&font_lbl, cx, cy + r + 2, nm, pos > 0.5f ? WHT : OFFT);
}
static void recorder(const inst *g, int x, int y, int w, int h)
{
    static const uint16_t PN[NPEN] = { RGB(60, 140, 255), RGB(220, 150, 30), RGB(214, 52, 40), RGB(150, 70, 170) };
    const int px0 = x + 5, pw = NHIST * 2 > w - 16 ? w - 16 : NHIST * 2, py0 = y + 4, ph = h - 8;
    if (!gfx_visible(x, y, w, h)) return;
    gfx_rfill(x, y, w, h, 3, BEZEL);
    gfx_fill(px0, py0, pw, ph, FACE);
    for (int i = 1; i < 4; i++) gfx_fill(px0, py0 + ph * i / 4, pw, 1, FACE2);
    for (int i = 0; i < NHIST; i += 8) { int gx = px0 + ((i * 2 - (g->hist_i % 8) * 2) % pw + pw) % pw; gfx_fill(gx, py0, 1, ph, FACE2); }
    int n0 = NHIST - pw / 2;
    for (int e = 0; e < NPEN; e++) {
        int py = 0;
        for (int i = n0; i < NHIST; i++) {
            int v = g->hist[e][(g->hist_i + i) % NHIST], yy = py0 + ph - 2 - v * (ph - 4) / 255, xx = px0 + (i - n0) * 2;
            if (i > n0) ln(xx - 2, py, xx, yy, e == 0 ? 2 : 1, PN[e]);
            py = yy;
        }
        gfx_fill(px0 + pw + 2, py - 2, 5, 5, PN[e]);
    }
    gfx_fill(px0 + pw + 1, py0, 1, ph, INK);
}
static void counter(int x, int y, const char *label, const char *value, uint16_t c)
{
    const int w = 74, h = 34;
    if (!gfx_visible(x, y, w, h)) return;
    gfx_rfill(x, y, w, h, 2, BEZEL);
    gfx_text(&font_lbl, x + 5, y + 2, label, OFFT);
    gfx_fill(x + 4, y + 14, w - 8, 17, GLASS);
    gfx_text_r(&font_dat, x + w - 7, y + 18, value, c);
}
static void rods(const inst *g)
{
    if (gfx_visible(0, 22, GFX_W, 197)) gfx_fill(0, 22, GFX_W, 197, PAINT);
    const int cx = 80, cy = 100, R = 56;
    if (gfx_visible(cx - R - 18, cy - R - 18, 2 * R + 36, 2 * R + 40)) {
        gfx_rfill(cx - R - 24, 24, 2 * R + 48, 146, 4, PAINT2); screws(cx - R - 24, 24, 2 * R + 48, 146);
        for (int p = 0; p < SY_NZ; p++) {
            int j = (3 * p) & 15; float a = C_TWO_PI * (float)p / SY_NZ;
            rod_dial(cx + (int)(c_cos(a) * (float)R), cy + (int)(c_sin(a) * (float)R) - 4, 9, g->rx.rod[j], g->rx.n[j], 0, g->rx.trip);
            if (gfx_visible(0, 22, GFX_W, 150)) gfx_text_c(&font_lbl, cx + (int)(c_cos(a) * (float)(R + 17)), cy + (int)(c_sin(a) * (float)(R + 17)) - 9, inst_key_note(g, j), g->rx.rod[j] > 0.5f ? WHT : OFFT);
        }
        /* the bank, in the middle: its reactivity from -2 to +0.6 dollars; its lamp: the regulator at work */
        float b = (g->rx.bank + 2.0f) / 2.6f; b = b < 0 ? 0 : b > 1 ? 1 : b;
        gfx_disc(cx, cy - 4, 24, g->rx.ar ? DIMG : OFFW); gfx_disc(cx, cy - 4, 21, FACE);
        for (int i = 0; i <= 10; i++) { float a = (-135.0f + 27.0f * (float)i) * (C_PI / 180.0f); ln(cx + (int)(c_sin(a) * 16.0f), cy - 4 - (int)(c_cos(a) * 16.0f), cx + (int)(c_sin(a) * 20.0f), cy - 4 - (int)(c_cos(a) * 20.0f), 1, INK); }
        float a = (-135.0f + 270.0f * b) * (C_PI / 180.0f); ln(cx, cy - 4, cx + (int)(c_sin(a) * 18.0f), cy - 4 - (int)(c_cos(a) * 18.0f), 2, NEEDLE);
        gfx_disc(cx, cy - 4, 2, INK);
        gfx_text_c(&font_lbl, cx, cy + 6, g->rx.ar ? "AR" : "BANK", INK);
    }
    char b[24], *e; float pk = 0; for (int j = 0; j < SY_NZ; j++) if (g->rx.n[j] > pk) pk = g->rx.n[j];
    e = s_fix(b, g->rx.power * 100.0f, g->rx.power < 0.1f ? 2 : 1, 0); s_cat(e, " %"); counter(164, 24, "POWER", b, g->rx.power > 1.0f ? RED : CHER);
    e = s_fix(b, pk * 100.0f, 1, 0); s_cat(e, " %"); counter(164, 60, "HOTTEST", b, pk > 1.2f ? RED : pk > 0.9f ? AMB : ENGR);
    e = s_fix(b, g->rx.rho, 2, 1); counter(164, 96, "RHO $", b, g->rx.prompt ? RED : g->rx.rho > 0.5f ? AMB : ENGR);
    s_fix(b, g->rx.orm, 1, 0); counter(164, 132, "ORM RODS", b, g->rx.orm < 8 ? RED : GRN);
    recorder(g, 2, 172, 236, 45);
}

/* ---------------------------------------------------------------- the card before PLAY: the plant's terminal
 * A green phosphor terminal, as the night shift's operator finds it: the self-check, the login typed, the access
 * granted, then the session's menu. SCR_BOOT_MS after power-on (inst.splash_until is set then, that far ahead) it
 * stands still; a card drawn without a splash time (the PC's screens) is the menu at once. */
#define PH_BG  RGB(2, 12, 6)
#define PH_SCN RGB(0, 6, 2)
#define PH_HI  RGB(140, 255, 160)
#define PH_TX  RGB(60, 230, 100)
#define PH_DIM RGB(26, 110, 52)
static void term_line(int row, const char *s, int n, uint16_t c)     /* n characters of s (-1: all) at a row of the terminal */
{
    int y = 10 + row * 13; if (!gfx_visible(0, y - 2, GFX_W, 15)) return;
    char b[40]; int i = 0; for (; s[i] && i < 39 && (n < 0 || i < n); i++) b[i] = s[i]; b[i] = 0;
    gfx_text(&font_dat, 8, y, b, c);
}
static void term_cursor(int row, const char *s, int n, int on)      /* a block after n characters of s */
{
    if (!on) return;
    char b[40]; int i = 0; for (; s[i] && i < 39 && i < n; i++) b[i] = s[i]; b[i] = 0;
    gfx_fill(8 + gfx_text_w(&font_dat, b) + 1, 10 + row * 13 - 1, gfx_text_w(&font_dat, "M") - 1, 11, PH_TX);
}
static void term_pair(int row, const char *a, const char *b, int bx, uint16_t ca, uint16_t cb)   /* a, a leader of dots, b from bx (bx < 0: b ends at -bx) */
{
    int y = 10 + row * 13; if (!gfx_visible(0, y - 2, GFX_W, 15)) return;
    int x0 = gfx_text(&font_dat, 8, y, a, ca) + 5, x1 = bx >= 0 ? bx : -bx - gfx_text_w(&font_dat, b);
    for (int x = x0; x < x1 - 5; x += 5) gfx_fill(x, y + 7, 2, 2, PH_DIM);
    gfx_text(&font_dat, x1, y, b, cb);
}
static int typed(int t, int t0, int per, int n) { int k = t < t0 ? 0 : (t - t0) / per; return k > n ? n : k; }
static void card(const inst *g)
{
    if (gfx_visible(0, 0, GFX_W, GFX_H)) gfx_fill(0, 0, GFX_W, GFX_H, PH_BG);
    for (int y = 1; y < GFX_H; y += 3) if (gfx_visible(0, y, GFX_W, 1)) gfx_fill(0, y, GFX_W, 1, PH_SCN);     /* (the scan lines) */
    int left = (int32_t)(g->splash_until - g->now_ms), t = left > 0 && left <= SCR_BOOT_MS ? SCR_BOOT_MS - left : 100000;
    int cur = (g->now_ms / 330u) & 1u;
    if (t < 6200) {                            /* the self-check, then the login */
        static const char *const CHK[6][2] = { { "MEMORY 640K", "OK" }, { "CORE MONITOR 16 CH", "OK" }, { "DELAYED NEUTRONS", "OK" },
                                               { "RODS 16/16 IN", "OK" }, { "SOURCE COUNTING", "OK" }, { "AZ-5 CIRCUIT", "ARMED" } };
        term_line(0, "TACET LABORATORIES  TC-14", t >= 0 ? -1 : 0, PH_HI);
        term_line(1, "REACTOR DIVISION - UNIT 4", t >= 250 ? -1 : 0, PH_DIM);
        for (int i = 0; i < 6; i++) if (t >= 600 + i * 260) term_pair(3 + i, CHK[i][0], CHK[i][1], -228, PH_TX, i == 5 ? PH_HI : PH_TX);
        if (t >= 2400) {
            int n = typed(t, 2800, 110, 8); term_line(10, "LOGIN: ", -1, PH_TX); term_line(10, "       OPERATOR", 7 + n, PH_HI);
            if (t < 3800) term_cursor(10, "LOGIN: OPERATOR", 7 + n, cur || n < 8);
        }
        if (t >= 3800) {
            int n = typed(t, 4100, 120, 8); term_line(11, "PASSWORD: ", -1, PH_TX); term_line(11, "          ********", 10 + n, PH_HI);
            if (t < 5200) term_cursor(11, "PASSWORD: ********", 10 + n, cur || n < 8);
        }
        if (t >= 5300) {
            int y = 10 + 13 * 13 - 2; if (gfx_visible(0, y, GFX_W, 14)) gfx_fill(6, y, gfx_text_w(&font_dat, "ACCESS GRANTED") + 4, 13, PH_TX);
            term_line(13, "ACCESS GRANTED", -1, PH_BG);
        }
        if (t >= 5700) term_line(14, "LAST LOGIN 26.04.86 01:23", -1, PH_DIM);
        return;
    }
    term_line(0, "UNIT 4 CONTROL      " VERSION, -1, PH_HI);
    term_line(1, (int32_t)(g->toast_until - g->now_ms) > 0 ? g->toast : "OPERATOR ON DUTY", -1, PH_DIM);
    term_line(3, "CRITICAL", -1, PH_HI);
    term_line(4, "A SYNTHESIZER", -1, PH_TX);
    term_line(5, "POWERED BY A REACTOR", -1, PH_TX);
    static const char *const MENU[6][2] = { { "KEYS", "THE RODS" }, { "HOLD PLAY", "A NEW CORE" }, { "HOME", "EVERYTHING" },
                                            { "SEQ", "IT PLAYS ITSELF" }, { "HOLD SAVE", "IT COMES BACK" }, { "HOLD REC", "AZ-5" } };
    for (int i = 0; i < 6; i++) term_pair(7 + i, MENU[i][0], MENU[i][1], 106, PH_TX, PH_DIM);
    term_line(14, "> PRESS PLAY", -1, PH_HI); term_cursor(14, "> PRESS PLAY ", 13, cur);
    (void)blink;
}

void scr_begin(const inst *g) { blink = (int)((g->now_ms / 400u) & 1u); tick4 = (int)(g->now_ms / 120u); }
void scr_draw(const inst *g)
{
    if (!g->run) { card(g); return; }
    nameplate(g);
    if (g->sel == PG_BOARD) { if (g->page) rods(g); else board(g); } else desk(g);
    status(g);
}
