/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: a restless musician for MINUTES (keys, knobs, buttons, signals, scenes, AZ-5, PLAY, at random), then
 * everything let go and turned down. Every sample a number; nothing over the limiter's ceiling; no block of sound
 * with a denormal in it once things are quiet (the FPU's flags, x86).   soak3 MINUTES SEED */
#include "../src/inst.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(__x86_64__)
#include <xmmintrin.h>
#define DEN_CLEAR() _mm_setcsr(_mm_getcsr() & ~0x3Fu)
#define DEN_SEEN() (_mm_getcsr() & 0x02u)
#else
#define DEN_CLEAR()
#define DEN_SEEN() 0
#endif
static inst G; static uint32_t rs; static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static double t_now; static long nan_n, den_n, den_run, den_worst, blocks; static float peak, hmax;
static void run(double secs)
{
    long nb = (long)(secs * SY_SR / SY_BLOCK);
    for (long b = 0; b < nb; b++) {
        float l[SY_BLOCK], r[SY_BLOCK]; static sy_ctl c; inst_tick(&G, (uint32_t)(t_now * 1000));
        t_now += (double)SY_BLOCK / SY_SR; blocks++;
        if (!G.run) continue;
        inst_ctl(&G, &c);
        DEN_CLEAR(); sy_block(&c, l, r);
        if (DEN_SEEN()) { den_n++; if (++den_run > den_worst) den_worst = den_run; } else den_run = 0;
        if (DEN_SEEN() && getenv("DEN") && den_n % 2000 == 1) fprintf(stderr, "denormal at %.1f s: heat %.3f tension %.3f blackout %d riser %d levels %.3f %.3f %.3f power %.2e plant %.3f %.0f %.3f %.3f %.3f %.3f\n", t_now, G.heat, G.tension, G.P.blackout, G.P.riser, G.level[0], G.level[1], G.level[2], G.rx.power, G.plant[0], G.plant[1], G.plant[2], G.plant[3], G.plant[4], G.plant[5]);
        inst_meters(&G); if (G.heat > hmax) hmax = G.heat;
        for (int n = 0; n < SY_BLOCK; n++) { if (!isfinite(l[n]) || !isfinite(r[n])) nan_n++; float a = fabsf(l[n]) > fabsf(r[n]) ? fabsf(l[n]) : fabsf(r[n]); if (a > peak) peak = a; }
    }
}
int main(int argc, char **argv)
{
    double mins = argc > 1 ? atof(argv[1]) : 5; rs = argc > 2 ? (uint32_t)atoi(argv[2]) * 2654435761u + 1 : 12345u;
    inst_new(&G, rs); sy_init(rs); inst_button(&G, 12, 1); inst_button(&G, 12, 0);
    long acts = 0, saves = 0;
    while (t_now < mins * 60) {
        uint32_t a = rnd() % 100;
        if (a < 40) { int k = (int)(rnd() % 27); inst_key(&G, k, 1); run(0.05 + (rnd() % 4000) * 0.001); if (rnd() % 4) inst_key(&G, k, 0); }
        else if (a < 62) inst_enc(&G, (int)(rnd() % 7), (int)(rnd() % 41) - 20);
        else if (a < 80) { int b = (int)(rnd() % 14); if (b == 12 && rnd() % 6) b = 2 + (int)(rnd() % 4); inst_button(&G, b, 1); run(b == 13 && rnd() % 3 == 0 ? 1.3 : b == 6 ? 0.0 : 0.05); if (b == 6) { inst_enc(&G, 2 + (int)(rnd() % 4), (int)(rnd() % 61) - 30); run(0.05); } inst_button(&G, b, 0); }
        else if (a < 84) { for (int k = 0; k < 27; k++) inst_key(&G, k, 0); }
        else if (a < 86) { inst_button(&G, 12, 1); run(1.0); inst_button(&G, 12, 0); }                      /* PLAY held: a new plant */
        else if (a < 87) { inst_button(&G, 9, 1); run(1.1); inst_button(&G, 9, 0); if (G.save_req) { static uint8_t sv[1024]; static inst H; G.save_req = 0; int n = inst_save(&G, sv); inst_new(&H, 5); if (n > 1000 || !inst_load(&H, sv, n) || memcmp(H.lock, G.lock, sizeof G.lock) || H.root != G.root || H.scene != G.scene) { printf("soak3: what was saved did not come back\n"); return 1; } for (int q = 0; q < SY_NFAM; q++) if (G.lock[q] && H.P.kind[q] != G.P.kind[q]) { printf("soak3: a locked kind was lost\n"); return 1; } saves++; } }
        else run((rnd() % 3000) * 0.001);
        run(0.02); acts++;
    }
    /* everything let go, every layer off, the plant turned down: silence must be cheap */
    for (int k = 0; k < 27; k++) inst_key(&G, k, 0);
    if (!G.run) { inst_button(&G, 12, 1); inst_button(&G, 12, 0); }
    G.hold = 0; for (int j = 0; j < SY_NZ; j++) { G.latch[j] = 0; G.gen_off[j] = 0; } G.P.gest[F_GEN] = 0; G.walking = 0; G.P.riser = 0; G.P.swell = 0; G.P.blackout = 0;
    for (int p = 0; p < SY_NPLANT; p++) G.P.plant[p] = 0;
    for (int l = 0; l < SY_NSRC; l++) G.P.on[l] = 0;
    G.P.hl[1] = 0.3f;
    run(150); long d0 = den_n; den_worst = 0; den_run = 0; run(30);
    if (saves) printf("soak3: %ld saves came back whole\n", saves);
    printf("soak3: %.0f min, %ld actions, peak %.3f, heat up to %.2f, %ld samples that are not numbers, %ld blocks with a denormal (%ld of them in the last 30 s of silence, %ld in a row at most)\n",
           mins, acts, peak, hmax, nan_n, den_n, den_n - d0, den_worst);
    return nan_n != 0 || peak > 0.95f || den_n - d0 > 20;
}
