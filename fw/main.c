/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4 for the M-VAVE FM-1: the firmware (4: a synthesizer powered by a model of a reactor; the keys move its
 * rods; the sound is src/synth.c, the instrument src/inst.c, the panel src/screen.c). Installed like SLOOP 3 (m-bootloader's MIDI MODE), it runs from
 * RAM: m-bootloader calls the stub in the flash window (fw/xip.S), which copies this image to 0x01C30000 and calls
 * _start (hal/fm1/ramapp_start.S) → app_main. One loop, no interrupts, one core:
 *   - the sound: a half of the DMA buffer (1024 frames, 23 ms) free → filled one block of 64 at a time, with the
 *     panel and the screen between the blocks;
 *   - the panel (one column a call: buttons, keys, knobs, lights), the screen (tiles of 120 x 16 by DMA, only the
 *     ones that changed, a pass at most every 80 ms), the MASTER pot.
 * One sector of the flash is written, and only when SAVE is held: 0x77000, the last of the application area this
 * firmware owns (as CRITICAL 0.1's career; SLOOP 3's data starts at 0x78000, SLOOP 2.8's at 0x97000: never written).
 * SLOOP 2.8's panel calibration is read, if it has one. The hardware layer (hal/fm1, core) is SLOOP 3's, written for that project. */
#include "../core/gb.h"
#include "../hal/fm1/fm1.h"
#include "../hal/fm1/board.h"
#include "../hal/fm1/panel.h"
#include "../hal/fm1/audio.h"
#include "../hal/fm1/adc.h"
#include "../hal/fm1/flash.h"
#include "../app/gfx.h"
#include "../src/inst.h"
#include "../src/synth.h"
#include "../src/screen.h"

#define VERSION "4.0"
#define HALF_FRAMES 1024u

static uint32_t lsb;
static int32_t abuf[2 * HALF_FRAMES * 2] __attribute__((aligned(32), section(".bootbss")));
static uint16_t tilebuf[120 * 16] __attribute__((aligned(4), section(".bootbss")));
static uint8_t sbuf[4096] __attribute__((aligned(4), section(".bootbss")));
static inst G, V;                            /* the instrument; V: the copy a screen pass draws */
static float master, master_tgt;

static uint32_t ms_acc, tick_last, tick_rem;
static uint32_t now_ms(void)
{
    uint32_t t = time_ticks(), per = lsb * 1000u;
    tick_rem += t - tick_last; tick_last = t;
    ms_acc += tick_rem / per; tick_rem %= per;
    return ms_acc;
}

/* ---------------------------------------------------------------- SLOOP 2.8's panel calibration (read only) */
static uint32_t crc32(uint32_t c, const uint8_t *p, uint32_t n)
{
    c = ~c;
    while (n--) { c ^= *p++; for (int k = 0; k < 8; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1; }
    return ~c;
}
static uint32_t rd32(const uint8_t *b) { return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }
static void sloop_calibration(void)
{
    static const uint8_t ID[14] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1 }, EID[7] = { 0, 1, 6, 2, 3, 4, 5 };
    uint8_t *h = sbuf, *pl = sbuf + 64;
    uint32_t best = 0, best_seq = 0; int have = 0;
    for (int c = 0; c < 2; c++) {
        uint32_t at = 0xFC000u + (uint32_t)c * 4096u;
        if (flash_read(at, h, 32)) continue;
        if (rd32(h) != 0x554C4546u || (h[4] | h[5] << 8) != 0 || rd32(h + 12) > 3840u || rd32(h + 28) != crc32(0, h, 28)) continue;
        if (!have || rd32(h + 8) > best_seq) { best = at; best_seq = rd32(h + 8); have = 1; }
    }
    if (!have || flash_read(best, h, 32)) return;
    uint32_t n = rd32(h + 12);
    if (n < 52 || flash_read(best + 256u, pl, (n + 3u) & ~3u) || crc32(0, pl, n) != rd32(h + 16)) return;
    uint32_t m = rd32(pl);
    if (m != 0x50455233u && m != 0x50455232u) return;
    const uint8_t *pn = pl + 16;
    if (rd32(pn) != 0x50414E35u) return;
    uint8_t btn[14], enc[7]; int8_t dir[7];
    for (int i = 0; i < 14; i++) btn[ID[i]] = pn[4 + i];
    for (int i = 0; i < 7; i++) { enc[EID[i]] = pn[18 + i]; dir[EID[i]] = (int8_t)pn[25 + i]; }
    panel_map(btn, enc, dir);
}

/* ---------------------------------------------------------------- the screen */
#define TILE_W 120
#define TILE_H 16
#define NTILES ((GFX_W / TILE_W) * (GFX_H / TILE_H))
static uint32_t tile_hash[NTILES];
static uint8_t tile_sent[NTILES];
static int tile_i = -1;
static uint32_t t_pass;
static void ui_step(void)
{
    if (lcd_tile_busy()) return;
    uint32_t now = time_ticks();
    if (tile_i < 0) {
        if ((uint32_t)(now - t_pass) < 80000u * lsb) return;
        t_pass = now;
        memcpy(&V, &G, sizeof V);
        scr_begin(&V);
        tile_i = 0;
        return;                                              /* (the tiles from the next call: the sound first) */
    }
    for (int n = 0; n < 6 && tile_i < NTILES && (uint32_t)(time_ticks() - now) < 300u * lsb; n++) {
        int i = tile_i++, tx = (i % (GFX_W / TILE_W)) * TILE_W, ty = (i / (GFX_W / TILE_W)) * TILE_H;
        gfx_target(tilebuf, tx, ty, TILE_W, TILE_H);
        scr_draw(&V);
        uint32_t h = gfx_finish(tilebuf, TILE_W * TILE_H);
        if (!tile_sent[i] || h != tile_hash[i]) { tile_hash[i] = h; tile_sent[i] = 1; lcd_tile_start(tx, ty, TILE_W, TILE_H, tilebuf); break; }
    }
    if (tile_i >= NTILES) tile_i = -1;
}

/* ---------------------------------------------------------------- what SAVE keeps, in the flash (one 4 KB sector) */
#define SAVE_AT 0x77000u                     /* the last sector of the application area: ours (fw/build.sh keeps the firmware below it) */
#define SAVE_MAGIC 0x33545243u               /* "CRT3" */
static int flash_ok, save_state;             /* 0 idle; 1 erasing; 2.. writing page (state - 2) */
static uint32_t save_n;
static void wr32(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24); }
static void save_begin(void)
{
    if (!flash_ok) { inst_say(&G, "NO FLASH: NOT SAVED"); return; }
    if (save_state) return;
    uint32_t L = (uint32_t)inst_save(&G, sbuf + 16);
    wr32(sbuf, SAVE_MAGIC); wr32(sbuf + 4, L); wr32(sbuf + 8, crc32(0, sbuf + 16, L)); wr32(sbuf + 12, 0);
    save_n = 16u + L;
    if (flash_erase_start(SAVE_AT)) { inst_say(&G, "SAVE FAILED"); return; }
    save_state = 1;
}
static void save_step(void)                  /* started, then polled: the sound never waits for the flash */
{
    if (!save_state || flash_busy()) return;
    uint32_t off = (uint32_t)(save_state - 1) * 256u;
    if (off >= save_n) { save_state = 0; inst_say(&G, "SAVED"); return; }
    uint32_t n = save_n - off > 256u ? 256u : save_n - off;
    if (flash_program_start(SAVE_AT + off, sbuf + off, n)) { save_state = 0; inst_say(&G, "SAVE FAILED"); return; }
    save_state++;
}
static int load_saved(void)
{
    if (!flash_ok || flash_read(SAVE_AT, sbuf, 16)) return 0;
    uint32_t L = rd32(sbuf + 4);
    if (rd32(sbuf) != SAVE_MAGIC || L < 8u || L > 2048u) return 0;
    if (flash_read(SAVE_AT, sbuf, (16u + L + 3u) & ~3u)) return 0;
    if (crc32(0, sbuf + 16, L) != rd32(sbuf + 8)) return 0;
    return inst_load(&G, sbuf + 16, (int)L);
}

/* ---------------------------------------------------------------- the panel */
static uint64_t leds_shown = ~0ull;
static uint32_t t_enc[7];
static void panel_frame(void)
{
    uint32_t ms = now_ms(), rel, pr = panel_button_edges(&rel);
    G.now_ms = ms;
    for (int b = 0; b < 14; b++) { if (pr & (1u << b)) inst_button(&G, b, 1); if (rel & (1u << b)) inst_button(&G, b, 0); }
    uint32_t krel, kpr = panel_key_edges(&krel);
    for (int k = 0; k < 27; k++) { if (kpr & (1u << k)) inst_key(&G, k, 1); if (krel & (1u << k)) inst_key(&G, k, 0); }
    for (int e = 0; e < 7; e++) {
        int d = panel_enc_take(e);
        if (!d) continue;
        uint32_t tn = time_ticks(), us = (tn - t_enc[e]) / lsb; t_enc[e] = tn;
        inst_enc(&G, e, d * (us < 25000u ? 4 : us < 60000u ? 2 : 1));
    }
    inst_tick(&G, ms);
    uint64_t m = inst_leds(&G);
    if (m != leds_shown) {
        for (int id = 0; id < 41; id++) if (((m ^ leds_shown) >> id) & 1u) panel_led(id, (int)((m >> id) & 1u));
        leds_shown = m;
    }
}

/* ---------------------------------------------------------------- the sound */
static float gate;                                       /* PLAY: the sound fades in (3 s); stopped: out (1.5 s) */
static void render_block(int32_t *h, uint32_t b)
{
    static float l[SY_BLOCK], r[SY_BLOCK];
    int32_t *o = h + b * SY_BLOCK * 2u;
    gate += ((G.run ? 1.0f : 0.0f) - gate) * (G.run ? 0.0005f : 0.001f);
    if (gate < 1e-6f) gate = 0;
    if (!G.run && gate == 0) { for (int n = 0; n < 2 * SY_BLOCK; n++) o[n] = 0; return; }   /* (stopped: silence, nothing computed) */
    static sy_ctl c; inst_ctl(&G, &c);
    sy_block(&c, l, r);
    inst_meters(&G);
    master += (master_tgt - master) * 0.02f;
    if (master < 1e-6f) master = 0;
    float mg = master * gate * gate * 2.2f;
    for (int n = 0; n < SY_BLOCK; n++) {
        float a = l[n] * mg, cc = r[n] * mg;
        a = a > 1.5f ? 1.0f : a < -1.5f ? -1.0f : a - a * a * a * (4.0f / 27.0f);   /* (a soft knee to ±1) */
        cc = cc > 1.5f ? 1.0f : cc < -1.5f ? -1.0f : cc - cc * cc * cc * (4.0f / 27.0f);
        o[2 * n] = (int32_t)(a * 8388607.0f); o[2 * n + 1] = (int32_t)(cc * 8388607.0f);
    }
}

int app_main(uint32_t arg)
{
    lsb = arg && arg < 200u ? arg : 60u;
    time_init(lsb);
    tick_last = time_ticks();
    lcd_init();
    lcd_fill(0, 0, 240, 240, RGB(6, 10, 22));
    lcd_text(5, 6, "CRITICAL " VERSION, RGB(232, 236, 240), RGB(6, 10, 22));
    panel_init();
    adc_init();
    flash_ok = flash_open() == 0;
    if (flash_ok) sloop_calibration();
    for (int i = 0; i < 20 * 11; i++) { panel_step(); time_wait_us(60); }   /* (what is held at power-on) */
    G.now_ms = now_ms();
    inst_new(&G, time_ticks() * 2654435761u ^ (uint32_t)adc_read(ADC_CH_MASTER) * 40503u);
    if (load_saved()) inst_say(&G, "YOUR PLANT IS BACK");
    G.now_ms = now_ms(); G.splash_until = G.now_ms + SCR_BOOT_MS;
    sy_init(G.seed);
    {   /* the first screen, whole, before the sound */
        t_pass = time_ticks() - 80000u * lsb;
        do { ui_step(); } while (tile_i >= 0 || lcd_tile_busy());
    }
    master = 0; master_tgt = 0;
    int a = audio_start(abuf, HALF_FRAMES * 2u);
    if (a != AUDIO_OK) { lcd_text(1, 12, a == AUDIO_ERR_PLL ? "SOUND: UNKNOWN PLL" : "SOUND: ERROR", RGB(255, 59, 48), 0); for (;;) ; }
    uint32_t t_pot = time_ticks(), ld_sum = 0, ld_n = 0, r_ticks = 0;
    int32_t *fill_h = 0; uint32_t fill_b = 0, t_half = 0;
    const uint32_t half_ticks = (uint32_t)((uint64_t)HALF_FRAMES * AUDIO_RATE_DEN * lsb / (AUDIO_RATE_NUM / 1000000u));
    for (;;) {
        if (!fill_h && audio_half_free()) { fill_h = audio_free_half(); fill_b = 0; r_ticks = 0; t_half = time_ticks(); }
        if (fill_h) {                                        /* one block, then the panel and the screen get their turn */
            panel_dark();
            uint32_t t0 = time_ticks();
            render_block(fill_h, fill_b);
            r_ticks += time_ticks() - t0;
            if (++fill_b < HALF_FRAMES / SY_BLOCK) goto housekeeping;
            audio_half_done(); fill_h = 0;
            if (G.run) {
                uint32_t pct = r_ticks * 100u / half_ticks;
                ld_sum += pct; if (++ld_n >= 64u) { G.load_pct = (float)(ld_sum / ld_n); ld_sum = 0; ld_n = 0; }
                if ((float)pct > G.load_peak && ms_acc > 5000u) G.load_peak = (float)pct;
                /* the sound never falls behind: over 85 % of its time, it lightens itself (fewer grains, a plainer
                 * hum); under 55 % for a while, it takes them back */
                static int eco, calm;
                if (pct > 85u) { if (eco < 2) sy_economy(++eco); calm = 0; }
                else if (pct < 55u) { if (++calm > 400 && eco > 0) { sy_economy(--eco); calm = 0; } }
                else calm = 0;
            }
            continue;
        }
    housekeeping:
        if (!panel_step()) { if (!fill_h) time_wait_us(40); continue; }
        panel_frame();
        if (G.save_req) { G.save_req = 0; save_begin(); }
        save_step();
        if ((uint32_t)(time_ticks() - t_pot) > 20000u * lsb) {
            t_pot = time_ticks();
            int pot = adc_read(ADC_CH_MASTER);
            if (pot >= 0) { float x = (float)pot * (1.0f / 1023.0f); master_tgt = 0.7f * x * x; }
        }
        /* the screen only while the sound is ahead: a half being filled late gets the time first */
        if (!fill_h || (uint32_t)(time_ticks() - t_half) < half_ticks / 3) { panel_dark(); ui_step(); }
    }
}
