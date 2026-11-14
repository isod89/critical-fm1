/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4 on the PC: every kind of every part of the plant, of every gesture and of the fuel, alone, each to its own
 * WAV (to choose by ear: tools/kindspage.py makes the page). The rest is off; the room is HOT STANDBY's.
 *   kinds3 OUTDIR [VARIANT 0..255] */
#include "../src/inst.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static const int WHITE[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 }, BLACK[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
static inst G; static FILE *f; static long ntot; static double t_now; static float pk;
static void run(double secs)
{
    long nb = (long)(secs * SY_SR / SY_BLOCK);
    for (long b = 0; b < nb; b++) {
        float l[SY_BLOCK], r[SY_BLOCK]; static sy_ctl c; inst_tick(&G, (uint32_t)(t_now * 1000)); inst_ctl(&G, &c); sy_block(&c, l, r); inst_meters(&G);
        for (int n = 0; n < SY_BLOCK; n++) { if (!isfinite(l[n]) || !isfinite(r[n])) { fprintf(stderr, "not a number\n"); exit(1); } short s[2] = { (short)(l[n] * 32000), (short)(r[n] * 32000) }; fwrite(s, 2, 2, f); if (fabsf(l[n]) > pk) pk = fabsf(l[n]); if (fabsf(r[n]) > pk) pk = fabsf(r[n]); }
        ntot += SY_BLOCK; t_now += (double)SY_BLOCK / SY_SR;
    }
}
static void key(int k, int d) { inst_key(&G, k, d); }
static void tap(int b) { key(BLACK[b], 1); run(0.1); key(BLACK[b], 0); }
static void heatrun(double total)             /* four notes held and quick ones over them: the plant heats; then all let go */
{
    static const int Q[8] = { 7, 8, 7, 9, 8, 11, 10, 8 };
    run(1); for (int w = 0; w < 4; w++) key(WHITE[w * 2], 1);
    for (int i = 0; i < 24; i++) { key(WHITE[Q[i % 8]], 1); run(0.24); key(WHITE[Q[i % 8]], 0); run(0.09); }
    for (int w = 0; w < 4; w++) key(WHITE[w * 2], 0);
    run(total - 1 - 24 * 0.33);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "."; int gene = argc > 2 ? atoi(argv[2]) : 128;       /* (every sound's variant: 0..255) */
    for (int fam = 0; fam <= F_FUEL; fam++) for (int k = 0; k < sy_nkind[fam]; k++) {
        char p[512], nm[40]; int j = 0; for (const char *s = sy_kind_name[fam][k]; *s; s++) nm[j++] = *s == ' ' ? '-' : *s; nm[j] = 0;
        snprintf(p, sizeof p, "%s/%02d-%s-%d-%s.wav", dir, fam, sy_fam_name[fam], k, nm);
        f = fopen(p, "wb"); char h[44] = { 0 }; fwrite(h, 1, 44, f); ntot = 0; t_now = 0; pk = 0;
        inst_new(&G, 1986); sy_init(1986);
        for (int q = 0; q < SY_NFAM; q++) { G.lock[q] = 1; G.P.kind[q] = 0; G.P.gene[q] = (uint8_t)gene; }
        G.P.kind[fam] = (uint8_t)k;
        inst_button(&G, 12, 1); inst_button(&G, 12, 0);
        for (int l = 0; l < SY_NSRC; l++) G.P.on[l] = 0;
        for (int q = 0; q < SY_NPLANT; q++) G.P.plant[q] = 0;
        G.P.horror = 0.4f;
        switch (fam) {
        case F_HUM: G.P.plant[P_HUM] = 0.8f; run(3); heatrun(11); break;
        case F_TURBINE: G.P.plant[P_TURBINE] = 0.9f; run(3); heatrun(15); break;
        case F_PUMP: G.P.plant[P_PUMP] = 1.0f; heatrun(15); break;
        case F_GEIGER: G.P.plant[P_GEIGER] = 0.8f; G.P.plant[P_PUMP] = 0.0f; run(4); heatrun(13); break;
        case F_RELAY: G.P.plant[P_RELAY] = 0.8f; run(0.5); tap(SIG_RELAYS); run(5); tap(SIG_RELAYS); run(5); break;
        case F_BREAKER: G.P.plant[P_RELAY] = 0.8f; G.P.plant[P_HUM] = 0.8f; run(5); tap(SIG_BLACKOUT); run(8); break;
        case F_GROAN: G.P.plant[P_GROAN] = 1.0f; run(0.3); tap(SIG_GROAN); run(k == 3 ? 13 : 10); break;
        case F_STEPS: run(0.3); tap(SIG_STEPS); run(17); break;
        case F_DOOR: run(0.3); tap(SIG_DOOR); run(gene > 200 ? 11 : 8); break;
        case F_RISER: run(0.3); key(BLACK[SIG_RISER], 1); run(k == 3 ? 13 : 10); key(BLACK[SIG_RISER], 0); run(4); break;
        case F_SWELL: run(0.3); key(BLACK[SIG_SWELL], 1); run(6.6); key(BLACK[SIG_SWELL], 0); run(5); break;
        case F_DROP: run(0.3); tap(SIG_DROP); run((k == 1 ? 6 : k == 2 ? 11 : 9) * (0.6 + 0.8 * gene / 255.0) + 3); break;
        case F_BRAAM: run(0.3); tap(SIG_BRAAM); run(k == 3 ? 6 : 5); tap(SIG_BRAAM); run(k == 3 ? 9 : 7); break;
        case F_FUEL: G.P.on[SRC_FUEL] = 1; run(4); heatrun(16); break;
        default: run(0.8); tap(SIG_STINGER); run(6); break;
        }
        unsigned sr = 44118, n = (unsigned)ntot * 4, v; fseek(f, 0, SEEK_SET); fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
        short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f); fwrite(&sr, 4, 1, f); v = sr * 4; fwrite(&v, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fclose(f);
        printf("%-8s %-11s %5.1f s  peak %.3f\n", sy_fam_name[fam], sy_kind_name[fam][k], t_now, pk);
    }
    return 0;
}
