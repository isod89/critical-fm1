/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: the screens as the FM-1 draws them (PPM), while the instrument is played (the real sound behind, so
 * the meters, the windows and the recorder show what they would).   shots3 OUTDIR */
#include "../src/inst.h"
#include "../src/screen.h"
#include "../app/gfx.h"
#include <stdio.h>
#include <string.h>
static inst G; static uint16_t px[GFX_W * GFX_H]; static const char *dir; static int n; static double t_now;
static const int WHITE[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 }, BLACK[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
static void run(double secs)
{
    long nb = (long)(secs * SY_SR / SY_BLOCK);
    for (long b = 0; b < nb; b++) {
        float l[SY_BLOCK], r[SY_BLOCK]; static sy_ctl c; inst_tick(&G, (uint32_t)(t_now * 1000));
        if (G.run) { inst_ctl(&G, &c); sy_block(&c, l, r); inst_meters(&G); }
        t_now += (double)SY_BLOCK / SY_SR;
    }
}
static void shot(const char *name)
{
    static uint16_t t[120 * 16];
    scr_begin(&G);
    for (int ty = 0; ty < GFX_H; ty += 16) for (int tx = 0; tx < GFX_W; tx += 120) {
        gfx_target(t, tx, ty, 120, 16); scr_draw(&G);
        for (int y = 0; y < 16; y++) for (int x = 0; x < 120; x++) px[(ty + y) * GFX_W + tx + x] = t[y * 120 + x];
    }
    char fn[256]; snprintf(fn, sizeof fn, "%s/%02d-%s.ppm", dir, n++, name);
    FILE *f = fopen(fn, "wb"); fprintf(f, "P6 %d %d 255\n", GFX_W, GFX_H);
    for (int i = 0; i < GFX_W * GFX_H; i++) { uint16_t c = px[i]; unsigned char r[3] = { (unsigned char)((c >> 11) * 255 / 31), (unsigned char)(((c >> 5) & 63) * 255 / 63), (unsigned char)((c & 31) * 255 / 31) }; fwrite(r, 1, 3, f); }
    fclose(f);
}
static void btn(int b) { inst_button(&G, b, 1); run(0.05); inst_button(&G, b, 0); }
int main(int argc, char **argv)
{
    dir = argc > 1 ? argv[1] : ".";
    inst_new(&G, 1986); sy_init(1986); G.load_pct = 31; G.load_peak = 44;
    t_now = 3; inst_tick(&G, 3000); G.splash_until = 3000 + SCR_BOOT_MS;              /* power-on: the terminal's login */
    run(1.9); shot("boot-check"); run(1.4); shot("boot-login"); run(1.6); shot("boot-password"); run(1.1); shot("boot-granted");
    run(1.0); shot("card");
    btn(12); run(8); shot("desk-standby");                                 /* PLAY: the core at a little power, its bed */
    inst_key(&G, WHITE[0], 1); inst_key(&G, WHITE[4], 1); run(6); inst_enc(&G, 3, 4); run(0.3); shot("desk-two-rods");   /* two rods out */
    inst_key(&G, BLACK[SIG_DOOR], 1); run(0.2); inst_key(&G, BLACK[SIG_DOOR], 0); run(0.8); shot("desk-key-card");
    btn(3); run(1.5); shot("desk-channels");                              /* SEL: the channels' page */
    inst_button(&G, 6, 1); btn(2); run(1.5); shot("desk-regulator"); inst_button(&G, 6, 0);   /* EDIT on the reactor: the regulator */
    btn(8); if (G.cur >= NROW_NOTES) { btn(8); btn(8); } run(1.0); shot("board-notes");     /* HOME */
    for (int i = 0; i < 4; i++) { inst_enc(&G, 2, 1); } btn(11); inst_enc(&G, 3, 1); inst_enc(&G, 3, 1); inst_enc(&G, 5, 30); run(6.0); shot("board-gen");
    btn(8); run(1.0); shot("board-plant");
    btn(8); run(1.0); shot("rods");
    inst_key(&G, WHITE[0], 0); inst_key(&G, WHITE[4], 0);
    btn(2); for (int i = 0; i < 25; i++) inst_enc(&G, 3, 1); run(1.0);    /* the void up */
    for (int i = 0; i < 40; i++) { int w = 7 + i % 6; inst_key(&G, WHITE[w], 1); run(0.25); inst_key(&G, WHITE[w], 0); run(0.1); }
    shot("desk-boiling");
    for (int w = 0; w < 16; w += 2) inst_key(&G, WHITE[w], 1);
    run(4); shot("desk-margin-low");
    inst_button(&G, 13, 1); run(1.3); shot("desk-az5"); inst_button(&G, 13, 0); run(0.6); shot("desk-az5-after");
    btn(8); btn(8); if (!G.page) btn(8); run(2); shot("rods-after");
    printf("shots3: %d screens\n", n);
    return 0;
}
