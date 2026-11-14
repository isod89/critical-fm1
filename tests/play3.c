/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4 on the PC: a piece played through inst.c as hands would (the keys move rods; the knobs, the buttons),
 * written as a WAV. 3 min 20:
 *   0:00  hot standby: the core counts, a bed of a few channels, the xenon moving it
 *   0:20  one rod out: the prompt jump, the rise on its period; the neighbours a fourth away warm with it
 *   0:40  a second, a third: the regulator takes power from the others; let go: the delayed neutrons' tail
 *   1:05  a slow line, rod after rod; HOLD, two rods left out
 *   1:30  the hand that plays by itself (its figure, drawn), a black key or two
 *   2:05  the void coefficient raised: the channels ring and boil
 *   2:35  half the rods out (the margin low), then AZ-5: the graphite tips first
 *   then  the shutdown: the power falls on its last period, the turbine coasts down; it comes back
 *   play3 OUT.wav [SEED]      (a seed: on the plant drawn from it; none: on the first kind of each) */
#include "../src/inst.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static const int WHITE[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 }, BLACK[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
typedef struct { float t; int kind, a, b; } ev;          /* kind 0 white key (a, b: on), 1 black key, 2 button (a, b: down), 3 knob (a: encoder, b: turn) */
static ev E[600]; static int ne;
static void N(float t0, float t1, int w) { E[ne++] = (ev){ t0, 0, w, 1 }; E[ne++] = (ev){ t1, 0, w, 0 }; }
static void B(float t, int b) { E[ne++] = (ev){ t, 1, b, 1 }; E[ne++] = (ev){ t + 0.2f, 1, b, 0 }; }
static void BT(float t, int b, float hold) { E[ne++] = (ev){ t, 2, b, 1 }; E[ne++] = (ev){ t + hold, 2, b, 0 }; }
static void K(float t0, float t1, int e, int total) { int n = total < 0 ? -total : total; for (int i = 0; i < n; i++) E[ne++] = (ev){ t0 + (t1 - t0) * (float)i / (float)n, 3, e, total < 0 ? -1 : 1 }; }
static int cmp(const void *a, const void *b) { float d = ((const ev *)a)->t - ((const ev *)b)->t; return d < 0 ? -1 : d > 0; }
int main(int argc, char **argv)
{
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0; if (f) { char h[44] = { 0 }; fwrite(h, 1, 44, f); }
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 10) : 0;
    static inst G; inst_new(&G, seed ? seed : 1986); sy_init(1986);
    if (!seed) for (int q = 0; q < SY_NFAM; q++) { G.lock[q] = 1; G.P.kind[q] = 0; G.P.gene[q] = 110; }
    G.lock[F_GEN] = 1; G.P.kind[F_GEN] = seed ? (uint8_t)(seed % 7u) : 1;
    inst_button(&G, 12, 1); inst_button(&G, 12, 0);
    if (seed) { printf("the plant drawn:"); for (int q = 0; q < SY_NFAM; q++) printf(" %s=%s", sy_fam_name[q], sy_kind_name[q][G.P.kind[q]]); printf("\n"); }
    N(20, 38, 0);                                              /* one rod */
    N(40, 62, 4); N(46, 62, 9); N(52, 58, 2);                  /* two, three */
    { static const int L[8] = { 7, 8, 7, 5, 4, 5, 3, 2 }; for (int i = 0; i < 8; i++) N(65.0f + 3.0f * (float)i, 67.6f + 3.0f * (float)i, L[i]); }
    BT(88.5f, 10, 0.05f); N(89, 89.4f, 0); N(89.5f, 89.9f, 11); BT(91, 10, 0.05f);   /* HOLD: two rods left out, then HOLD off */
    BT(92, 11, 0.05f);                                         /* SEQ: the hand that plays by itself */
    E[ne++] = (ev){ 92.5f, 2, 8, 1 }; E[ne++] = (ev){ 92.55f, 2, 8, 0 }; K(93, 94, 2, 4); K(94, 95, 5, 30);   /* HOME: the board, GEN's row, its amount up */
    B(104, SIG_BRAAM); B(118, SIG_DOOR);
    BT(124, 11, 0.05f);                                        /* the hand off */
    BT(125, 2, 0.05f); K(126, 135, 3, 40);                     /* FX: the reactor; the void up */
    N(126, 150, 7); N(130, 150, 12);
    for (int w = 0; w < 16; w += 2) N(152, 158.5f, w);         /* half the rods out */
    BT(157, 13, 1.3f);                                         /* AZ-5 */
    B(166, SIG_TRIP);
    qsort(E, (size_t)ne, sizeof E[0], cmp);
    double t = 0, next = 0; int ie = 0; long n_tot = 0, nan_n = 0, nlim = 0, nblk = 0; float peak = 0;
    printf("power (heat) every 10 s:");
    while (t < 200.0) {
        while (ie < ne && E[ie].t <= t) {
            ev *e = &E[ie++];
            if (e->kind == 0) inst_key(&G, WHITE[e->a], e->b); else if (e->kind == 1) inst_key(&G, BLACK[e->a], e->b);
            else if (e->kind == 2) inst_button(&G, e->a, e->b); else inst_enc(&G, e->a, e->b);
        }
        float l[SY_BLOCK], r[SY_BLOCK]; static sy_ctl c; inst_tick(&G, (uint32_t)(t * 1000)); inst_ctl(&G, &c); sy_block(&c, l, r); inst_meters(&G);
        nblk++; if (G.limit > 0.02f) nlim++;
        { static int az_was; if (G.az != az_was) { printf(G.az == 2 ? "\n  %.1f s: AZ-5 drove the core prompt critical; the cut\n" : G.az == 1 ? "\n  %.1f s: AZ-5, the rods going in\n" : "\n  %.1f s: AZ-5 over\n", t); az_was = G.az; } }
        if (t >= next) { printf(" %.0e(%.2f)", G.rx.power, G.heat); next += 10; }
        for (int n = 0; n < SY_BLOCK; n++) { if (!isfinite(l[n]) || !isfinite(r[n])) { nan_n++; l[n] = r[n] = 0; } float a = fabsf(l[n]) > fabsf(r[n]) ? fabsf(l[n]) : fabsf(r[n]); if (a > peak) peak = a;
            short s[2] = { (short)(fmaxf(-1, fminf(1, l[n])) * 32000), (short)(fmaxf(-1, fminf(1, r[n])) * 32000) }; if (f) fwrite(s, 2, 2, f); }
        n_tot += SY_BLOCK; t += (double)SY_BLOCK / SY_SR;
    }
    printf("\nplay3: %.0f s, peak %.3f, the limiter at work in %.1f %% of the blocks, %ld samples that are not numbers\n", t, peak, 100.0 * (double)nlim / (double)nblk, nan_n);
    if (f) { unsigned sr = 44118, n = (unsigned)n_tot * 4, v; fseek(f, 0, SEEK_SET); fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
        short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f); fwrite(&sr, 4, 1, f); v = sr * 4; fwrite(&v, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fclose(f); }
    return nan_n != 0 || peak > 0.95f;
}
