/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4 filmed: the instrument played from a script of timed gestures, its sound to a WAV and its screen,
 * thirty times a second, to a file of 240 x 240 RGB565 pictures (for the tape: lore/vhs, tape 8).
 *   film4 SCRIPT OUT.wav OUT.rgb565 SECONDS      (and OUT.rgb565.st: the console's state, a record a picture)
 * A script line: TIME COMMAND ARGS.
 *   boot                       power-on: the terminal's login (inst.splash_until)
 *   btn B | bd B | bu B        a button tapped, down, up
 *   wk W 0|1 | bk B 0|1        a white key (0..15), a black key (0..10)
 *   enc E D                    an encoder turned D steps
 *   ramp ARRAY I VALUE SECS    one of the sound's values turned smoothly: rx rx2 fu md hl lvl (its I-th), gen (GEN's amount)
 *   kind F K                   a family's kind, locked (set off camera)
 * The plant is the first kind of each, and the seed fixed: the film is the same each time it is made. */
#include "../src/inst.h"
#include "../src/screen.h"
#include "../app/gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static const int WHITE[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 }, BLACK[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
typedef struct { double t; char cmd[8], arr[8]; double a[4]; } ev;
static ev E[4096]; static int ne;
static inst G; static float lim_max; static long nblk, nlim;
static struct { int on; float *p; float v0, v1; double t0, t1; } ramp[16];
static int cmp(const void *a, const void *b) { double d = ((const ev *)a)->t - ((const ev *)b)->t; return d < 0 ? -1 : d > 0; }
static float *target(const char *a, int i)
{
    sy_ctl *p = &G.P; i &= 3;
    if (!strcmp(a, "rx")) return &p->rx[i];
    if (!strcmp(a, "rx2")) return &p->rx2[i];
    if (!strcmp(a, "fu")) return &p->fu[i];
    if (!strcmp(a, "md")) return &p->md[i];
    if (!strcmp(a, "hl")) return &p->hl[i];
    if (!strcmp(a, "lvl")) return &p->lvl[i % SY_NSRC];
    if (!strcmp(a, "gen")) return &p->gest[F_GEN];
    return 0;
}
int main(int argc, char **argv)
{
    if (argc < 5) return 1;
    FILE *f = fopen(argv[1], "r"); char line[256];
    while (fgets(line, sizeof line, f) && ne < 4096) {
        ev *e = &E[ne]; memset(e, 0, sizeof *e);
        if (sscanf(line, "%lf %7s", &e->t, e->cmd) < 2) continue;
        if (!strcmp(e->cmd, "ramp")) sscanf(line, "%*f %*s %7s %lf %lf %lf", e->arr, &e->a[0], &e->a[1], &e->a[2]);
        else sscanf(line, "%*f %*s %lf %lf %lf %lf", &e->a[0], &e->a[1], &e->a[2], &e->a[3]);
        ne++;
    }
    fclose(f); qsort(E, (size_t)ne, sizeof E[0], cmp);
    char stn[512]; snprintf(stn, sizeof stn, "%s.st", argv[3]);
    FILE *wav = fopen(argv[2], "wb"), *pic = fopen(argv[3], "wb"), *st = fopen(stn, "wb"); double total = atof(argv[4]);
    { char h[44] = { 0 }; fwrite(h, 1, 44, wav); }
    inst_new(&G, 1986); sy_init(1986);
    for (int q = 0; q < SY_NFAM; q++) { G.lock[q] = 1; G.P.kind[q] = 0; G.P.gene[q] = 128; }
    static uint16_t px[GFX_W * GFX_H], tile[120 * 16];
    double t = 0, next_frame = 0; int ie = 0; long nfr = 0, nsm = 0; int tapb[14]; double tapt[14]; for (int i = 0; i < 14; i++) tapb[i] = 0;
    int az_seen = 0; double az_cut = -1;
    while (t < total) {
        while (ie < ne && E[ie].t <= t) {
            ev *e = &E[ie++]; int a = (int)e->a[0], b = (int)e->a[1];
            if (!strcmp(e->cmd, "boot")) { inst_tick(&G, (uint32_t)(t * 1000)); G.splash_until = G.now_ms + SCR_BOOT_MS; }
            else if (!strcmp(e->cmd, "btn")) { inst_button(&G, a, 1); tapb[a] = 1; tapt[a] = t + 0.07; }
            else if (!strcmp(e->cmd, "bd")) inst_button(&G, a, 1);
            else if (!strcmp(e->cmd, "bu")) inst_button(&G, a, 0);
            else if (!strcmp(e->cmd, "wk")) inst_key(&G, WHITE[a], b);
            else if (!strcmp(e->cmd, "bk")) inst_key(&G, BLACK[a], b);
            else if (!strcmp(e->cmd, "enc")) inst_enc(&G, a, b);
            else if (!strcmp(e->cmd, "kind")) { G.P.kind[a] = (uint8_t)b; G.lock[a] = 1; }
            else if (!strcmp(e->cmd, "ramp")) {
                float *p = target(e->arr, a); if (!p) { fprintf(stderr, "film4: no such value %s\n", e->arr); return 1; }
                for (int r = 0; r < 16; r++) if (!ramp[r].on) { ramp[r].on = 1; ramp[r].p = p; ramp[r].v0 = *p; ramp[r].v1 = (float)e->a[1]; ramp[r].t0 = t; ramp[r].t1 = t + (e->a[2] > 0.01 ? e->a[2] : 0.01); break; }
            }
        }
        for (int i = 0; i < 14; i++) if (tapb[i] && t >= tapt[i]) { inst_button(&G, i, 0); tapb[i] = 0; }
        for (int r = 0; r < 16; r++) if (ramp[r].on) {
            double u = (t - ramp[r].t0) / (ramp[r].t1 - ramp[r].t0); if (u >= 1) { u = 1; ramp[r].on = 0; }
            *ramp[r].p = ramp[r].v0 + (ramp[r].v1 - ramp[r].v0) * (float)u;
            for (int i = 0; i < 4; i++) if (G.sel != PG_BOARD && fabsf(inst_knob_value(&G, i) - *ramp[r].p) < 1e-6f) { G.knob_i = i; G.knob_ms = (uint32_t)(t * 1000) + 1; }
        }
        float l[SY_BLOCK], r[SY_BLOCK]; static sy_ctl c; inst_tick(&G, (uint32_t)(t * 1000));
        if (G.run) { inst_ctl(&G, &c); sy_block(&c, l, r); inst_meters(&G); } else { memset(l, 0, sizeof l); memset(r, 0, sizeof r); }
        if (G.az == 2 && !az_seen) { az_seen = 1; az_cut = t; }
        nblk++; if (G.limit > 0.02f) nlim++; if (G.limit > lim_max) lim_max = G.limit;
        for (int n = 0; n < SY_BLOCK; n++) { short s[2] = { (short)(fmaxf(-1, fminf(1, l[n])) * 32000), (short)(fmaxf(-1, fminf(1, r[n])) * 32000) }; fwrite(s, 2, 2, wav); }
        nsm += SY_BLOCK; t += (double)SY_BLOCK / SY_SR;
        while (next_frame <= t) {
            scr_begin(&G);
            for (int ty = 0; ty < GFX_H; ty += 16) for (int tx = 0; tx < GFX_W; tx += 120) { gfx_target(tile, tx, ty, 120, 16); scr_draw(&G); for (int y = 0; y < 16; y++) memcpy(px + (ty + y) * GFX_W + tx, tile + y * 120, 240); }
            fwrite(px, 2, GFX_W * GFX_H, pic); nfr++; next_frame += 1.0 / 30.0;
            {   /* the lamps, the four knobs as the panel has them, the page, AZ-5, run; the core: power, heat, ORM, rods out */
                struct { uint64_t leds; float k[4]; int32_t sel, page, mixer, az, run; float power, heat, orm, voids; uint32_t rods; } q;
                memset(&q, 0, sizeof q);
                q.leds = inst_leds(&G); for (int i = 0; i < 4; i++) q.k[i] = inst_knob_value(&G, i);
                q.sel = G.sel; q.page = G.page; q.mixer = G.mixer; q.az = G.az | (G.rec_held ? 4 : 0) | (G.rx.trip ? 8 : 0); q.run = G.run;
                q.power = G.rx.power; q.heat = G.heat; q.orm = G.rx.orm; q.voids = G.rx.voids;
                for (int j = 0; j < SY_NZ; j++) if (inst_rod_out(&G, j)) q.rods |= 1u << j;
                fwrite(&q, sizeof q, 1, st);
            }
        }
    }
    { unsigned sr = 44118, n = (unsigned)nsm * 4, v; fseek(wav, 0, SEEK_SET); fwrite("RIFF", 1, 4, wav); v = 36 + n; fwrite(&v, 4, 1, wav); fwrite("WAVEfmt ", 1, 8, wav); v = 16; fwrite(&v, 4, 1, wav);
      short s = 1; fwrite(&s, 2, 1, wav); s = 2; fwrite(&s, 2, 1, wav); fwrite(&sr, 4, 1, wav); v = sr * 4; fwrite(&v, 4, 1, wav); s = 4; fwrite(&s, 2, 1, wav); s = 16; fwrite(&s, 2, 1, wav); fwrite("data", 1, 4, wav); fwrite(&n, 4, 1, wav); }
    fclose(wav); fclose(pic); fclose(st);
    printf("film4: %.1f s, %ld pictures, %d gestures; the limiter worked in %.1f %% of the blocks (at most %.0f %% taken off); %s\n", t, nfr, ne,
           100.0 * (double)nlim / (double)(nblk ? nblk : 1), 100.0 * lim_max, az_seen ? "the core went prompt critical" : "no excursion");
    if (az_seen) printf("film4: the cut at %.2f s\n", az_cut);
    return 0;
}
