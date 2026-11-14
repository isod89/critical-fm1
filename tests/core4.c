/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4 on the PC: the reactor alone (src/synth.c), driven as a ctl, no panel: the start from the source range,
 * the regulator bringing it to power, keys held (rods out) and let go, a long hold (the xenon after it), the void
 * coefficient raised, AZ-5. Prints what the panel would read, once a second; writes the sound.
 *   core4 OUT.wav [VOID 0..1] [AZ_AT_SECONDS] */
#include "../src/synth.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static sy_ctl C;
int main(int argc, char **argv)
{
    FILE *f = argc > 1 ? fopen(argv[1], "wb") : 0; if (f) { char h[44] = { 0 }; fwrite(h, 1, 44, f); }
    float voidk = argc > 2 ? (float)atof(argv[2]) : 0.35f, az_at = argc > 3 ? (float)atof(argv[3]) : 1e9f;
    sy_init(1986);
    static const int SC[7] = { 0, 1, 3, 5, 7, 8, 10 };
    for (int j = 0; j < SY_NZ; j++) C.zone_hz[j] = 73.42f * powf(2.0f, (SC[j % 7] + 12 * (j / 7)) / 12.0f);
    C.rx[0] = 0.75f; C.rx[1] = voidk; C.rx[2] = 0.35f; C.rx[3] = 0.6f;
    C.rx2[0] = 0.35f; C.rx2[1] = 0.45f; C.rx2[2] = 0.5f; C.rx2[3] = 0.3f;
    C.fu[0] = 0.55f; C.fu[1] = 0.6f; C.fu[2] = 0.4f; C.fu[3] = 0.3f;
    C.md[0] = 0.35f; C.md[1] = 0.5f; C.md[2] = 0.75f; C.md[3] = 0.4f;
    C.hl[0] = 0.78f; C.hl[1] = 0.5f; C.hl[2] = 0.5f; C.hl[3] = 0.4f;
    for (int s = 0; s < SY_NSRC; s++) { C.on[s] = 1; C.lvl[s] = 0.8f; C.space[s] = 0.7f; C.hall[s] = 0.6f; }
    C.hall[SRC_HALL] = 0;
    C.room_size = 1.2f; C.room_t60 = 6.0f; C.room_damp = 0.5f; C.room_mix = 0.6f;
    for (int p = 0; p < SY_NPLANT; p++) C.plant[p] = 0.7f;
    C.horror = 0.6f; C.root_hz = 73.42f;
    for (int q = 0; q < SY_NFAM; q++) { C.kind[q] = 0; C.gene[q] = 128; C.gest[q] = 0.7f; }
    if (getenv("ONLY")) { int o = atoi(getenv("ONLY")); for (int q = 0; q < SY_NSRC; q++) C.on[q] = (o >> q) & 1; if (!(o & 8)) for (int p = 0; p < SY_NPLANT; p++) C.plant[p] = 0; if (!(o & 16)) C.room_mix = 0; }
    C.gest[F_SHIFT] = 0; C.gest[F_GEN] = 0; C.kind[F_FUEL] = argc > 4 ? (uint8_t)atoi(argv[4]) : 0;
    double t = 0, next = 0; long nt = 0; float peak = 0; int nan = 0;
    while (t < 150.0) {
        for (int j = 0; j < SY_NZ; j++) C.rod[j] = 0;
        if (t > 25 && t < 60) C.rod[0] = 1;                      /* a key held, 35 s */
        if (t > 35 && t < 55) C.rod[4] = 1;
        if (t > 45 && t < 55) C.rod[9] = 1;
        if (t > 75 && t < 80) C.rod[0] = 1;                      /* the first again, after it was poisoned */
        if (t > 85 && t < 120) { C.rod[2] = 1; C.rod[6] = 1; }
        if (t > 95) C.rx[1] = voidk + (float)(t - 95) / 40.0f * (0.85f - voidk) > 0.85f ? 0.85f : voidk + (float)(t - 95) / 40.0f * (0.85f - voidk);   /* the void raised, slowly */
        if (az_at < 1e8f && t > az_at - 15 && t < az_at) for (int j = 0; j < SY_NZ; j += 2) C.rod[j] = 1;     /* (before the scram: half the rods out, the margin low) */
        if (t >= az_at && t < az_at + 0.01) sy_az5();
        float l[SY_BLOCK], r[SY_BLOCK]; sy_block(&C, l, r);
        for (int n = 0; n < SY_BLOCK; n++) { if (!isfinite(l[n]) || !isfinite(r[n])) { nan++; l[n] = r[n] = 0; } float a = fabsf(l[n]) > fabsf(r[n]) ? fabsf(l[n]) : fabsf(r[n]); if (a > peak) peak = a;
            if (f) { short s[2] = { (short)(l[n] * 32000), (short)(r[n] * 32000) }; fwrite(s, 2, 2, f); } }
        nt += SY_BLOCK; t += SY_BLOCK / SY_SR;
        if (t >= next) {
            const sy_rx *x = sy_reactor(); next += 3;
            printf("%5.0f P %8.2e th %.2f rho %+.3f bank %+.2f per %6.1f sur %+5.2f void %.2f orm %4.1f trip %d pr %d | ", t, x->power, x->thermal, x->rho, x->bank, x->period, x->sur, x->voids, x->orm, x->trip, x->prompt);
            for (int j = 0; j < SY_NZ; j++) printf("%c", x->n[j] < 1e-5f ? '.' : x->n[j] < 1e-3f ? ':' : x->n[j] < 0.05f ? 'o' : x->n[j] < 0.3f ? 'O' : x->n[j] < 1.0f ? '#' : '@');
            printf(" | xe0 %.2f |", x->xe[0]); for (int j = 0; j < SY_NZ; j++) printf(" %.0f", x->n[j] > 1e-9f ? 10 * log10f(x->n[j]) : -90); printf("\n");
        }
    }
    printf("peak %.3f, %d not numbers\n", peak, nan);
    if (f) { unsigned sr = 44118, n = (unsigned)nt * 4, v; fseek(f, 0, SEEK_SET); fwrite("RIFF", 1, 4, f); v = 36 + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
        short s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f); fwrite(&sr, 4, 1, f); v = sr * 4; fwrite(&v, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f); fclose(f); }
    return 0;
}
