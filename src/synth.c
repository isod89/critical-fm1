/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: the sound (synth.h). */
#include "synth.h"
#include "../core/cmath.h"
#define SR (SY_SR * 0.5f)
#define NB (SY_BLOCK / 2)
#define DT ((float)NB / SR)
#define SRQ (SR * 0.5f)
#define NQ (NB / 2)

static uint32_t rs = 2463534242u;
static float rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return (float)(rs >> 8) * (1.0f / 16777216.0f); }
static float flush(float x) { return x < 1e-15f && x > -1e-15f ? 0.0f : x; }
static float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float rate(float secs) { return 1.0f - c_exp(-DT / secs); }      /* a block's step toward a target reached in about `secs` */

/* ---- a band-limited saw, built at start (the gestures' strings, brass, scream): five widths by the note's height */
static int16_t SAWT[1280];
static const uint16_t MOFF[5] = { 0, 512, 768, 1024, 1152 };
static const uint8_t MSH[5] = { 23, 24, 24, 25, 25 };
static int wt_mip(float hz) { return hz <= 164.0f ? 0 : hz <= 328.0f ? 1 : hz <= 656.0f ? 2 : hz <= 1312.0f ? 3 : 4; }
static inline float wt_at(const int16_t *t, int sh, uint32_t ph)
{
    uint32_t i = ph >> sh, m = (1u << (32 - sh)) - 1u; float f = (float)(ph & ((1u << sh) - 1u)) * (1.0f / (float)(1u << sh));
    float a = (float)t[i], b = (float)t[(i + 1) & m]; return a + (b - a) * f;
}
static uint32_t inc_of(float hz) { return (uint32_t)(hz * (4294967296.0f / SR)); }
/* a sine by table (FM needs its phase bent) */
static float SIN[1025];
static inline float sin_ph(uint32_t ph) { uint32_t i = ph >> 22; float f = (float)(ph & 0x3FFFFFu) * (1.0f / 4194304.0f); return SIN[i] + (SIN[i + 1] - SIN[i]) * f; }
/* a low-pass that can be swept (state variable, trapezoidal): two poles */
typedef struct { float a1, a2, a3, k, s1, s2; } svf;
static void svf_set(svf *f, float hz, float q)
{
    if (hz > SR * 0.42f) hz = SR * 0.42f;
    float w = C_PI * hz / SR, g = c_sin(w) / c_cos(w); f->k = 1.0f / q;
    f->a1 = 1.0f / (1.0f + g * (g + f->k)); f->a2 = g * f->a1; f->a3 = g * f->a2;
}
static inline float svf_lp(svf *f, float x)
{
    float v3 = x - f->s2, v1 = f->a1 * f->s1 + f->a2 * v3, v2 = f->s2 + f->a2 * f->s1 + f->a3 * v3;
    f->s1 = 2 * v1 - f->s1; f->s2 = 2 * v2 - f->s2; return v2;
}

static inline float svf_bp(svf *f, float x)
{
    float v3 = x - f->s2, v1 = f->a1 * f->s1 + f->a2 * v3, v2 = f->s2 + f->a2 * f->s1 + f->a3 * v3;
    f->s1 = 2 * v1 - f->s1; f->s2 = 2 * v2 - f->s2; return v1;
}
/* ================================================================ the buses */
static float tension, heat, bend = 1.0f, wow1, wow2, lev[SY_NSRC], lim_show;
static float mixL[NB], mixR[NB], rin[NB], cin[NB], eL[NB], eR[NB];
static void meter(int l, const float *a, float g)
{
    float pk = 0; for (int n = 0; n < NB; n++) { float v = c_absf(a[n]) * g; if (v > pk) pk = v; }
    pk = clamp01(pk * 4.0f); lev[l] += (pk - lev[l]) * (pk > lev[l] ? 0.5f : 0.06f); if (lev[l] < 1e-6f) lev[l] = 0;
}
static void give(const sy_ctl *c, int s)      /* a source's block to the mix, the plate, the hall */
{
    float g = c->lvl[s], sp = c->space[s] * g * 0.5f, cl = c->hall[s] * g * 0.5f;
    for (int n = 0; n < NB; n++) { float a = eL[n], b = eR[n]; mixL[n] += a * g; mixR[n] += b * g; rin[n] += (a + b) * sp; cin[n] += (a + b) * cl; }
    meter(s, eL, g);
}
/* ================================================================ THE LOOP: what the reactor's heat drives */
#define TJ 7
static struct {
    uint32_t hph[2]; float hsag, hfl, hfl_t, ha;                         /* HUM: its two units; the sag; the lights' dip */
    float tu, ta[TJ], tlev; uint32_t tph[TJ], ttw[TJ];                   /* (tlev: it comes up slowly after a cold start) */
    float pad0;                   /* TURBINE: where in its octave; each octave's level */
    float pt, pe, pf; uint32_t pph; int pn;                              /* PUMP */
    float krate, pper, tide, gst, bk2; uint8_t gkind;
    int ln; float lt, lgap, la, lp, l60, lth;                               /* a blow to come (the second knock, the heart's second beat) */
    struct { float re, im, cr, ci; } gm[6]; int gi, kacc; float kph, klp[2]; uint8_t ksync;      /* GEIGER: its six small voices; its pulse's phase */
    struct { float re, im, cr, ci; } rl[4]; float rt, rgap, rshow, bk_t; int rburst; uint8_t bk_was, dark;   /* RELAY: a clack's modes; the run; the breaker */
    float gd[2048]; int gw; float gt, ge, ghz, gb0, gb1, gu, gdur, glp, gnz, gn2; int gon; uint32_t gph;   /* GROAN */
    uint32_t qph; float qa, qw, qt, qhz; int gforce; float alive;        /* GHOST; (a groan asked for; the grid: 1 alive, 0 lost) */
    struct { float re, im, cr, ci; } dm[10]; float dthr, dthi, dtc, dts; /* a door, a knock: modes struck */
} PT;
/* ---- 3.3: the kinds. Every part of the plant and every gesture has several; a plant is a draw of one of each (and
 * of a gene for each: a pitch, a pace), the first of each being 3.2's */
const uint8_t sy_nkind[SY_NFAM] = { 3, 3, 3, 2, 2, 2, 4, 3, 4, 4, 4, 3, 4, 4, 4, 3, 7 };
const char *const sy_fam_name[SY_NFAM] = { "HUM", "TURBINE", "PUMPS", "GEIGER", "RELAYS", "BREAKER", "GROAN", "STEPS", "DOOR", "RISER", "SWELL", "DROP", "BRAAM", "STINGER", "FUEL", "SHIFT", "GEN" };
const char *const sy_kind_name[SY_NFAM][7] = {
    { "MAINS", "SICK", "DEEP" }, { "FALLING", "RISING", "TIDAL" }, { "HEART", "TRIPLE", "SLOW" }, { "DRIPS", "PULSE" },
    { "SMALL", "HEAVY" }, { "SINGLE", "DOUBLE" }, { "BEND", "WHALE", "CREAK", "DEEP" }, { "STEEL", "PIPES", "HEART" },
    { "GREAT", "HATCH", "KNOCKS", "PLATE" }, { "STRINGS", "FALL", "TREMOLO", "ENDLESS" }, { "BREATH", "BELL", "VOID", "SHUDDER" },
    { "FALL", "BOMB", "FROM BELOW" }, { "BRASS", "TRITONE", "GRIND", "HORN" }, { "SHOCK", "CLUSTER", "SCREECH", "ANVIL" },
    { "DETECTOR", "RODS", "GLASS", "MAINS" }, { "SINK", "RISE", "TEAR" },
    { "STALKER", "DESCENT", "LULLABY", "DIES IRAE", "TRITONE", "CLUSTER", "PULSE" } };
/* each kind's trim: set by measure (tests/loud3.py), so that the kinds of one sound are as loud as its first (3.2's).
 * (the fuels: their own, measured apart: tests/fuel4) */
static const float TRIM[SY_NFAM][4] = { { 1.00f, 1.26f, 1.30f, 1.00f }, { 1.00f, 1.04f, 1.08f, 1.00f }, { 1.00f, 0.77f, 1.19f, 1.00f }, { 1.00f, 1.53f, 1.00f, 1.00f }, { 1.00f, 0.76f, 1.00f, 1.00f }, { 1.00f, 1.05f, 1.00f, 1.00f }, { 1.00f, 0.48f, 2.29f, 0.90f }, { 1.00f, 0.36f, 0.93f, 1.00f }, { 1.00f, 1.01f, 2.69f, 0.68f }, { 1.00f, 1.23f, 1.48f, 1.04f }, { 1.00f, 1.17f, 1.06f, 1.00f }, { 1.00f, 0.54f, 1.01f, 1.00f }, { 1.00f, 1.04f, 1.04f, 1.00f }, { 1.00f, 1.26f, 1.30f, 1.00f }, { 1.00f, 0.79f, 0.97f, 0.96f }, { 1.00f, 1.00f, 1.00f, 1.00f }, { 1.00f, 1.00f, 1.00f, 1.00f } };
static uint8_t KD[SY_NFAM], GN[SY_NFAM]; static float FG[SY_NFAM];
static float gene(int f) { return (float)GN[f] * (1.0f / 255.0f); }
static float kg(int f) { return FG[f] * TRIM[f][KD[f] & 3]; }          /* a gesture's gain: its level (0.7: as built) and its kind's trim */
void sy_kinds(const sy_ctl *c) { for (int f = 0; f < SY_NFAM; f++) { KD[f] = c->kind[f] < sy_nkind[f] ? c->kind[f] : 0; GN[f] = c->gene[f]; float g = c->gest[f] * (1.0f / 0.7f); FG[f] = (f >= F_STEPS && f <= F_STINGER) || f == F_BREAKER ? g * g : 1.0f; } }
/* a transformer: its mains, and the weights of 1x .. 6x (MAINS: 100 Hz and its family over 50; SICK: one that buzzes; DEEP: an octave under).
 * Its VARIANT: the mains' pitch, 46 .. 54 Hz */
static const float HK[3][7] = { { 50, 0.45f, 1.0f, 0.22f, 0.42f, 0.10f, 0.20f }, { 50, 0.30f, 0.80f, 0.55f, 0.30f, 0.35f, 0.12f }, { 25, 0.90f, 0.50f, 0.35f, 1.0f, 0.15f, 0.30f } };
/* struck modes: the shock's, a low piano cluster's, an anvil's; a steel plate's (a cracked gong) */
static const float BF[4][8] = { { 38, 55, 83, 147, 262, 415, 690, 1100 }, { 41.2f, 43.7f, 55, 58.3f, 82.4f, 87.3f, 116.5f, 174.6f }, { 212, 367, 513, 698, 911, 1240, 1710, 2230 }, { 110, 174, 246, 321, 426, 506, 649, 781 } },
                   BT[4][8] = { { 4, 3.5f, 3, 2.5f, 2, 1.5f, 1, 0.7f }, { 5, 5, 4.5f, 4.5f, 4, 3, 2.5f, 2 }, { 3, 2.6f, 2.2f, 1.8f, 1.5f, 1.2f, 0.9f, 0.7f }, { 6, 5.5f, 5, 4.2f, 3.5f, 3, 2.4f, 2 } },
                   BA[4][8] = { { 1, 0.9f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.25f }, { 1, 0.9f, 0.9f, 0.8f, 0.8f, 0.6f, 0.4f, 0.3f }, { 1, 0.8f, 0.7f, 0.5f, 0.4f, 0.3f, 0.2f, 0.15f }, { 1, 0.9f, 0.8f, 0.7f, 0.55f, 0.45f, 0.35f, 0.25f } };
static int eco, az_ph; static float az_t, az_z, cut = 1.0f, trip, out_show;
void sy_economy(int level) { eco = level < 0 ? 0 : level > 2 ? 2 : level; }
void sy_az5(void) { if (!az_ph) { az_ph = 1; az_t = 0; az_z = 0; } }
int sy_az5_phase(void) { return az_ph; }
float sy_out_level(void) { return out_show; }
static void strike(float amp, float pitch, float t60s, float thud)
{
    for (int j = 0; j < 10; j++) {
        float f = (52.0f + 760.0f * rnd() * rnd()) * pitch, t60 = t60s * c_pow(f / 60.0f, -0.45f), w = C_TWO_PI * f / SR, dec = c_exp(-6.91f / (t60 * SR));
        PT.dm[j].cr = c_cos(w) * dec; PT.dm[j].ci = c_sin(w) * dec; PT.dm[j].re += amp * (0.4f + 0.6f * rnd()) * 0.16f * c_pow(60.0f / f, 0.25f);
    }
    { float w = C_TWO_PI * 44.0f * pitch / SR, dec = c_exp(-6.91f / (0.5f * SR)); PT.dtc = c_cos(w) * dec; PT.dts = c_sin(w) * dec; PT.dthr += amp * thud; }
}
static void later(float t, int n, float amp, float pitch, float t60, float thud) { PT.ln = n; PT.lt = t; PT.lgap = t; PT.la = amp; PT.lp = pitch; PT.l60 = t60; PT.lth = thud; }
void sy_step(float near)                      /* STEEL a steel floor; PIPES two knocks in a pipe; HEART a body's double beat. Their VARIANT: their weight */
{
    float w = gene(F_STEPS), n2 = near * near, p = 1.225f - 0.45f * w, tl = 0.6f + 0.8f * w, th = 0.7f + 0.6f * w, g = kg(F_STEPS);
    switch (KD[F_STEPS]) {
    default: strike((0.10f + 0.50f * n2) * g, (0.75f + 0.1f * rnd()) * p, 0.35f * tl, 0.9f * th); break;
    case 1: strike((0.20f + 1.0f * n2) * g, (1.6f + 0.5f * rnd()) * p, 0.9f * tl, 0.0f); later(0.11f + 0.05f * rnd(), 1, (0.15f + 0.75f * n2) * g, 1.7f * p, 0.9f * tl, 0.0f); break;
    case 2: strike(0.25f * (0.2f + n2) * g, 0.9f * p, 0.12f, 2.7f * th); later(0.22f, 1, 0.17f * (0.2f + n2) * g, 0.8f * p, 0.12f, 2.7f * th); break;
    }
}
static void boom(const float *f, const float *t60, const float *a, int n, float g, float fs, float ts)      /* given modes struck (the breaker, a dull blow, the shock) */
{
    for (int j = 0; j < n && j < 10; j++) { float w = C_TWO_PI * f[j] * fs / SR, dec = c_exp(-6.91f / (t60[j] * ts * SR)); PT.dm[j].cr = c_cos(w) * dec; PT.dm[j].ci = c_sin(w) * dec; PT.dm[j].re += a[j] * g; }
}
static void clack(float amp)                   /* a relay: three short high modes and a thump (RELAYS; CONTACTORS: heavy). Their VARIANT: their pitch */
{
    static const float RK[2][5] = { { 500, 900, 1.0f, 0.8f, 120 }, { 220, 300, 2.2f, 1.2f, 80 } };   /* its pitch from, over; how long; the thump, its pitch */
    static const float R[4] = { 1.0f, 2.3f, 3.7f, 0.0f }, T[4] = { 0.030f, 0.020f, 0.012f, 0.060f }, A[4] = { 1.0f, 0.7f, 0.5f, 1.0f };
    const float *k = RK[KD[F_RELAY]];
    float f0 = (k[0] + k[1] * rnd()) * (1.25f - 0.5f * gene(F_RELAY)), g = amp * (0.25f + 0.15f * rnd()) * TRIM[F_RELAY][KD[F_RELAY]];
    for (int j = 0; j < 4; j++) { float f = j == 3 ? k[4] : f0 * R[j]; if (f > 9000.0f) f = 9000.0f; float w = C_TWO_PI * f / SR, dec = c_exp(-6.91f / (T[j] * k[2] * SR)); PT.rl[j].cr = c_cos(w) * dec; PT.rl[j].ci = c_sin(w) * dec; PT.rl[j].re += g * A[j] * (j == 3 ? k[3] : 1.0f); }
    PT.rshow = 1.0f;
}
static void gtick(float f, float t60, float a)        /* the dosimeter: one of its six small voices struck */
{
    int m = PT.gi; PT.gi = (PT.gi + 1) % 6;
    float w = C_TWO_PI * f / SR, dec = c_exp(-6.91f / (t60 * SR)); PT.gm[m].cr = c_cos(w) * dec; PT.gm[m].ci = c_sin(w) * dec; PT.gm[m].re += a;
}
/* ---- what chills: the gestures (their state; their block is after the plant's) */
#define RV 12
static struct {
    int r_on, r_kind; float r_t, r_dur, r_tr, r_base, r_st[RV], r_up[RV], r_ex[RV], r_g; uint32_t r_ph[RV]; svf r_f[2]; uint8_t r_was;
    int b_on, b_step, b_kind; float b_t, b_hz, b_last, b_l1, b_l2, b_auto, b_e, b_a0; uint32_t b_ph[7]; uint8_t hot_was;
    int s_on, s_kind; float s_t, s_tau, s_tr, s_g, s_hz, s_hp, s_lp, s_l0; uint32_t s_ph[4]; uint8_t s_was;
    int d_on, d_kind; float d_t, d_g; uint32_t d_ph, d_p2;
    int k_on, k_kind; float k_t, k_hp, k_lp, k_gain; uint32_t k_ph[5]; svf k_f;
    float clock;
} GS;
static void braam_now(float root_hz)
{
    float f = root_hz > 20 ? root_hz : 73.42f; while (f >= 65.4f) f *= 0.5f; while (f < 32.7f) f *= 2;
    GS.b_step = GS.b_on || GS.clock - GS.b_last < 10.0f ? (GS.b_step + 1) % 4 : 0;       /* (again soon: a semitone higher) */
    if (!GS.b_on) {                                 /* (the three close ones meet a second in, the two an octave up a little before: it blooms, then thins as it dies) */
        static const float PH[7] = { 0.0f, -0.176f, 0.176f, 0.31f, 0.075f, 0.62f, 0.45f };
        for (int j = 0; j < 7; j++) GS.b_ph[j] = (uint32_t)(int32_t)(PH[j] * 4294967296.0f);
        GS.b_e = 0; GS.b_l1 = 0; GS.b_l2 = 0; GS.b_kind = KD[F_BRAAM];
    }
    GS.b_a0 = GS.b_e;                               /* (struck again while it sounds: from where it is, no gap) */
    GS.b_hz = f * c_exp2((float)GS.b_step / 12.0f); GS.b_on = 1; GS.b_t = 0; GS.b_last = GS.clock;
}
/* the stinger: its blow's modes and weight, its burst, its scream (how loud, from where, sliding by how many semitones, dying in) */
typedef struct { uint8_t tab; float boom, noise, scr, hz, gl, dec; } stk;
static const stk SK[4] = { { 0, 0.40f, 0.7f, 0.30f, 1175, 0.34f, 0.35f }, { 1, 0.28f, 0.3f, 0, 0, 0, 0 }, { 0, 0.22f, 0.5f, 0.42f, 1400, -7.0f, 0.5f }, { 2, 0.30f, 0.5f, 0, 0, 0, 0 } };
static void stinger_now(void)
{
    int k = KD[F_STINGER]; const stk *s = &SK[k];
    GS.k_gain = kg(F_STINGER);
    boom(BF[s->tab], BT[s->tab], BA[s->tab], 8, s->boom * GS.k_gain, 1.125f - 0.25f * gene(F_STINGER), 1.0f);
    GS.k_on = 1; GS.k_kind = k; GS.k_t = 0;
}
static float ev_root = 73.42f;
void sy_event(int ev)
{
    float p, g;
    switch (ev) {
    case EV_GROAN: PT.gt = 0; PT.gforce = 1; break;
    case EV_RELAYS: PT.rburst = 10; PT.rgap = 0.5f; PT.rt = 0; break;
    case EV_BRAAM: braam_now(ev_root); break;
    case EV_DROP: GS.d_on = 1; GS.d_kind = KD[F_DROP]; GS.d_t = 0; GS.d_ph = 0; GS.d_p2 = 0; break;
    case EV_STINGER: stinger_now(); break;
    case EV_TRIP: trip = 1.0f; strike(0.7f, 0.8f, 3.0f, 0.6f); break;
    case EV_STEP: sy_step(0.5f); break;
    case EV_KNOCK: strike(0.5f, 1.7f + rnd(), 1.2f, 0.5f); break;
    case EV_DOOR: p = gene(F_DOOR); g = kg(F_DOOR);      /* the GREAT DOOR; a HATCH slammed; three KNOCKS; a steel PLATE. Its VARIANT: its size (the great door at its largest: a vault's) */
        switch (KD[F_DOOR]) {
        default: strike(1.0f * g, 1.55f - 1.1f * p, 2.5f + 4.0f * p, 0.5f); break;
        case 1: strike(0.9f * g, 1.5f * (1.25f - 0.5f * p), 0.8f, 1.2f); break;
        case 2: strike(0.5f * g, 1.3f * (1.25f - 0.5f * p), 0.5f, 0.6f); later(0.30f, 2, 0.5f * g, 1.3f * (1.25f - 0.5f * p), 0.5f, 0.6f); break;
        case 3: boom(BF[3], BT[3], BA[3], 8, 0.20f * g, 1.25f - 0.5f * p, 1.0f); break;
        }
        break;
    default: break;
    }
}

/* ================================================================ THE CORE: sixteen channels, each a point reactor
 * Six groups of delayed neutrons (U-235, Keepin): their shares of beta and their decay constants. In dollars, with the
 * prompt jump approximation (the prompt neutrons settle at once), each channel j:
 *   n_j = (sum_i a_i y_ij + K (n_left + n_right) + q) / (1 - rho_j + 2K),   y_ij' = s lambda_i (n_j - y_ij)
 * (y: its precursors, as the power they would hold; K: what leaks to and from each neighbour; q: the source; s: TEMPO,
 * how much faster than a real core time runs). Past one dollar the approximation fails: the channel is prompt
 * critical, and its power is let run on the prompt neutrons alone. */
static const float BI[6] = { 0.033f, 0.219f, 0.196f, 0.395f, 0.115f, 0.042f }, LI[6] = { 0.0124f, 0.0305f, 0.111f, 0.301f, 1.14f, 3.01f };
static struct {
    float n[SY_NZ], y[SY_NZ][6], T[SY_NZ], vd[SY_NZ], io[SY_NZ], xe[SY_NZ], rp[SY_NZ], rho[SY_NZ], enr[SY_NZ];
    float bank, P, Pth, lnP, dln, trip_t, az_a, rho_m, vd_m; uint8_t trip, prompt, gene_was, enr_ok;
} RX;
static sy_rx RXO;
const sy_rx *sy_reactor(void) { return &RXO; }
static float log10f_(float x) { return c_log2(x > 1e-12f ? x : 1e-12f) * 0.30103f; }
static uint32_t hash32(uint32_t a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return a; }
/* the ring is laid out in fourths: channel j (the j-th white key) sits between j+3 and j-3 */
#define LEFT(j) (((j) + 3) & (SY_NZ - 1))
#define RIGHT(j) (((j) + SY_NZ - 3) & (SY_NZ - 1))
static void core_reset(float n0, float bank0)
{
    for (int j = 0; j < SY_NZ; j++) { RX.n[j] = n0; for (int i = 0; i < 6; i++) RX.y[j][i] = n0; RX.T[j] = 0; RX.vd[j] = 0; RX.io[j] = 0; RX.xe[j] = 0; RX.rp[j] = 0; RX.rho[j] = 0; }
    RX.bank = bank0; RX.P = n0; RX.Pth = 0; RX.lnP = c_log2(n0) * 0.693147f; RX.dln = 0; RX.trip = 0; RX.trip_t = 0; RX.az_a = 0; RX.prompt = 0;
}
static void reactor_block(const sy_ctl *c)
{
    float s = 3.0f + 37.0f * c->rx[3] * c->rx[3];                                   /* TEMPO: 3 .. 40 times a real core */
    float K = 0.004f + 0.30f * c->rx2[0] * c->rx2[0];                                /* COUPLING */
    float aT = 1.0f, aV = -0.30f + 2.0f * c->rx[1], wX = 1.4f * c->rx[2];       /* fuel heat (always against), steam (VOID), xenon */
    float q = 1e-7f * c_exp2(10.0f * c->rx2[3]);                                    /* SOURCE: 1e-7 .. 1e-4 */
    float tT = 4.0f / c_sqrt(s), tV = 0.45f * tT, tX = 9.0f + 30.0f * (1.0f - c->rx[2]);   /* (the xenon's time: its own, 9 .. 39 s) */
    float a[6]; for (int i = 0; i < 6; i++) a[i] = 1.0f - c_exp(-LI[i] * s * DT);
    if (!RX.enr_ok || RX.gene_was != GN[F_FUEL]) {                                  /* the core's loading: each channel's enrichment, from the draw */
        RX.enr_ok = 1; RX.gene_was = GN[F_FUEL];
        for (int j = 0; j < SY_NZ; j++) RX.enr[j] = 0.40f * ((float)(hash32((uint32_t)GN[F_FUEL] * 977u + (uint32_t)j * 7919u + 1u) & 1023u) * (1.0f / 1023.0f) - 0.5f);
    }
    /* the rods: each its key's; AZ-5 (and a trip) drives them all in, in two seconds, graphite tips first */
    int az = az_ph == 1 || RX.trip_t > 0;
    if (az_ph == 1) RX.az_a += DT / 2.0f; else if (RX.trip_t <= 0) RX.az_a = 0;
    float out = 0;
    for (int j = 0; j < SY_NZ; j++) {
        float tgt = az ? 0.0f : c->rod[j] ? 1.0f : 0.0f, sp = az ? DT / 2.0f : DT / 0.25f, d = tgt - RX.rp[j];
        RX.rp[j] += d > sp ? sp : d < -sp ? -sp : d; out += RX.rp[j];
    }
    /* the bank: by hand (BANK), or the regulator's (AR), which may pull it out no further than BANK allows */
    float ar = c->rx2[2], lim = -1.0f + 1.8f * c->rx[0];
    if (az) ;                                                                       /* (the scram: the bank stays where it was; the rods do the work) */
    else if (ar < 0.01f) RX.bank = lim;
    else {
        float set = c_exp2(-10.0f + 10.0f * c->rx2[1]), e = log10f_(set) - log10f_(RX.P); if (e > 1) e = 1; if (e < -1) e = -1;
        RX.bank += ((0.04f + 1.2f * ar * ar) * e - 0.35f * RX.dln) * DT * (0.5f + 0.06f * s);      /* (on the power, and against a short period) */
        if (RX.bank > lim) RX.bank = lim;
        if (RX.bank < -2.0f) RX.bank = -2.0f;
    }
    float tip = 0, absb = 0;
    if (az_ph == 1) {      /* the graphite tips: the more steam counts, and the fewer rods were left in, the worse */
        float u = RX.az_a, w = 0.3f + 1.6f * clamp01((aV - 0.3f) / 1.4f) * clamp01(1.3f - RXO.orm / 16.0f);
        tip = w * (u < 0.3f ? c_sin(C_PI * u / 0.3f) : 0.0f); absb = 4.0f * clamp01((u - 0.2f) / 0.8f);
    }
    else if (RX.trip_t > 0) absb = 4.0f;
    /* each channel */
    float pm = 0, rm = 0, vm = 0; RX.prompt = 0;
    for (int j = 0; j < SY_NZ; j++) {
        float r = RX.bank + (-0.25f + 0.80f * RX.rp[j]) + RX.enr[j] - aT * RX.T[j] + aV * RX.vd[j] - wX * RX.xe[j] + tip - absb;
        RX.rho[j] = r; rm += r;
        float src = q + K * (RX.n[LEFT(j)] + RX.n[RIGHT(j)]);
        for (int i = 0; i < 6; i++) src += BI[i] * RX.y[j][i];
        float n;
        if (r > 0.98f + 2.0f * K) { RX.prompt = 1; n = RX.n[j] * (1.0f + (r - 0.98f) * DT * s * 40.0f); }    /* (prompt critical: no delayed neutron is waited for) */
        else n = src / (1.0f - r + 2.0f * K);
        if (n > 50.0f) n = 50.0f;
        RX.n[j] = n;
        for (int i = 0; i < 6; i++) RX.y[j][i] += (n - RX.y[j][i]) * a[i];
        RX.T[j] += (n - RX.T[j]) * rate(tT);
        RX.vd[j] += (n / (1.0f + 0.4f * n) - RX.vd[j]) * rate(tV);
        {   float nx = n * 90.0f;                                                      /* (the xenon as a core at power has it: its burn-out, its pit after) */
            RX.io[j] += (nx - RX.io[j]) * rate(tX);
            RX.xe[j] += (RX.io[j] + 0.05f * nx - RX.xe[j] * (1.0f + 2.0f * nx)) * rate(tX); if (RX.xe[j] > 0.9f) RX.xe[j] = 0.9f; }
        pm += n; vm += RX.vd[j];
    }
    pm *= 1.0f / SY_NZ; RX.rho_m = rm * (1.0f / SY_NZ); RX.vd_m = vm * (1.0f / SY_NZ);
    float ln = c_log2(pm > 1e-9f ? pm : 1e-9f) * 0.693147f, dln = (ln - RX.lnP) / DT; RX.lnP = ln; RX.P = pm;
    RX.dln += (dln - RX.dln) * rate(0.25f);
    RX.Pth += (pm - RX.Pth) * rate(tT * 1.5f);
    /* the protection: too much power, or too short a period: AZ-5 by itself */
    float pk = 0; for (int j = 0; j < SY_NZ; j++) if (RX.n[j] > pk) pk = RX.n[j];
    if (!az_ph && RX.trip_t <= 0 && (pk > 3.0f || (RX.dln > 12.0f && pm > 0.2f))) { RX.trip = pk > 3.0f ? 1 : 2; sy_az5(); }
    if (az_ph == 1 && RX.az_a >= 1.0f) { az_ph = 0; RX.trip_t = 4.0f; if (!RX.trip) RX.trip = 3; }      /* (the rods are in: held there four seconds) */
    if (az_ph == 1 && pk > 6.0f) { az_ph = 2; az_t = -0.45f; RX.trip_t = 0; sy_event(EV_STINGER); braam_now(ev_root); }     /* (prompt critical under the scram: the blow, then the cut) */
    if (RX.trip_t > 0) { RX.trip_t -= DT; if (RX.trip_t <= 0) { RX.trip_t = 0; RX.trip = 0; RX.bank = -1.2f; } }
    /* what the panel reads */
    for (int j = 0; j < SY_NZ; j++) { RXO.n[j] = RX.n[j]; RXO.rod[j] = RX.rp[j]; RXO.xe[j] = RX.xe[j]; RXO.temp[j] = RX.T[j]; }
    RXO.power = pm; RXO.thermal = RX.Pth; RXO.rho = RX.rho_m; RXO.bank = RX.bank; RXO.voids = RX.vd_m;
    RXO.period = c_absf(RX.dln) < 0.005f ? 0.0f : 1.0f / RX.dln; RXO.sur = RX.dln * (60.0f / 2.302585f);
    RXO.orm = (float)SY_NZ - out + 10.0f * clamp01((0.5f - RX.bank) / 2.5f);
    RXO.trip = RX.trip; RXO.prompt = RX.prompt; RXO.ar = ar >= 0.01f && !az;
}
/* the hottest channel (its pitch, its power) */
static int hottest(void) { int b = 0; for (int j = 1; j < SY_NZ; j++) if (RX.n[j] > RX.n[b]) b = j; return b; }

/* ================================================================ THE CHANNELS: the neutrons as a detector hears them
 * A unit of power is a million counts a second. Each count strikes the channel's four resonances; few, they are
 * clicks; many, a grain; more, a tone. The tone brightens with the power (Cherenkov light: the more of it, the bluer).
 * The flux trembles (fuel that vibrates; with steam, boiling at about half a hertz). */
#define CM 4
static const float FR[4][CM] = { { 1.0f, 2.003f, 3.007f, 4.02f }, { 1.0f, 1.414f, 1.93f, 2.58f }, { 1.0f, 2.756f, 5.404f, 8.933f }, { 1.0f, 2.0f, 3.0f, 4.0f } },
                   FW[4][CM] = { { 1.0f, 0.50f, 0.33f, 0.22f }, { 1.0f, 0.80f, 0.70f, 0.55f }, { 1.0f, 0.45f, 0.25f, 0.12f }, { 1.0f, 0.60f, 0.40f, 0.30f } };
static struct { struct { float re, im; } m[SY_NZ][CM]; float vib[SY_NZ], vib2[SY_NZ], boil[SY_NZ], hz[SY_NZ]; float det[SY_NZ][CM]; uint8_t det_ok; } CHN;
static float zone_pan(int j) { int p = (11 * j) & (SY_NZ - 1); return 0.5f + 0.45f * c_sin(C_TWO_PI * (float)p / SY_NZ); }      /* (round the ring: round the ears) */
static void channels_block(const sy_ctl *c)
{
    float ring = c->fu[0], cher = c->fu[1], noise = c->fu[2], spread = c->fu[3];
    int fk = KD[F_FUEL] & 3, cm = eco ? (eco > 1 ? 2 : 3) : CM;           /* (short of time: the highest partials let go) */
    static float ex[NB];
    for (int n = 0; n < NB; n++) { eL[n] = 0; eR[n] = 0; }
    if (!CHN.det_ok) { CHN.det_ok = 1; for (int j = 0; j < SY_NZ; j++) for (int k = 0; k < CM; k++) CHN.det[j][k] = (float)(hash32((uint32_t)(j * 31 + k + 7)) & 1023u) * (1.0f / 1023.0f) - 0.5f; }
    float t60b = 0.05f + 6.0f * ring * ring, depth = noise * (0.04f + 0.25f * clamp01(RX.vd_m));
    for (int j = 0; j < SY_NZ; j++) {
        float n = RX.n[j];
        /* the flux trembles: a few hertz of vibration, and with steam, boiling (half a hertz) */
        CHN.vib[j] += ((rnd() - 0.5f) * 2.0f - CHN.vib[j]) * 0.25f; CHN.vib2[j] += (CHN.vib[j] - CHN.vib2[j]) * 0.25f;
        CHN.boil[j] += C_TWO_PI * DT * (0.45f + 0.1f * (float)(j & 3)); if (CHN.boil[j] > C_TWO_PI) CHN.boil[j] -= C_TWO_PI;
        float ne = n * (1.0f + depth * (2.5f * CHN.vib2[j] + 0.6f * clamp01(RX.vd[j] * 1.5f) * c_sin(CHN.boil[j])));
        if (ne < 0) ne = 0;
        float L = clamp01((log10f_(ne) + 7.0f) * (1.0f / 7.2f)), A = ne > 1e-3f ? 2.0f * c_pow(ne, 0.6f) : 2.0f * 0.0158f * c_pow(ne * 1000.0f, 0.25f);     /* (at power, 12 dB a decade: a held channel stands out; under, the counts, softly) */
        float rt = ne * 1.0e6f, E = rt * DT;
        float live = 0; for (int k = 0; k < CM; k++) live += c_absf(CHN.m[j][k].re) + c_absf(CHN.m[j][k].im);
        if (E < 0.002f && live < 1e-6f) { for (int k = 0; k < CM; k++) { CHN.m[j][k].re = 0; CHN.m[j][k].im = 0; } continue; }
        float hz = c->zone_hz[j] * bend; CHN.hz[j] = hz;
        float t60 = t60b * (0.30f + 0.70f * L), ev = A * TRIM[F_FUEL][fk] / c_sqrt(rt * t60 * 0.5f > 1.0f ? rt * t60 * 0.5f : 1.0f);
        /* the counts of this block: one by one (a few), or as the noise they become (many) */
        for (int i = 0; i < NB; i++) ex[i] = 0;
        if (E < 6.0f) {
            int k = 0; float p = c_exp(-E), sm = p, u = rnd();
            while (u > sm && k < 16) { k++; p *= E / (float)k; sm += p; }
            for (int i = 0; i < k; i++) ex[(int)(rnd() * (NB - 0.01f))] += ev * (0.6f + 0.8f * rnd());
        } else { float g = ev * 3.4641f * c_sqrt(E / (float)NB); for (int i = 0; i < NB; i++) ex[i] = (rnd() - 0.5f) * g; }
        float cr[CM], ci[CM], gk[CM], tilt = 1.5f - 1.9f * cher * L; if (tilt < 0.15f) tilt = 0.15f;
        for (int k = 0; k < CM; k++) {
            if (k >= cm) { cr[k] = 0; ci[k] = 0; gk[k] = 0; CHN.m[j][k].re = 0; CHN.m[j][k].im = 0; continue; }
            float f = hz * FR[fk][k] * (1.0f + spread * 0.02f * CHN.det[j][k] * (float)(k + 1));
            if (fk == 3) { float h = (float)(int)(f / 50.0f + 0.5f); if (h < 1) h = 1; f = 50.0f * h * (1.0f + spread * 0.004f * CHN.det[j][k]); }   /* (MAINS: on the grid's harmonics) */
            if (f > SR * 0.42f) { cr[k] = 0; ci[k] = 0; gk[k] = 0; continue; }
            float w = C_TWO_PI * f / SR, dec = c_exp(-6.91f / (t60 / (1.0f + 0.35f * (float)k) * SR));
            cr[k] = c_cos(w) * dec; ci[k] = c_sin(w) * dec; gk[k] = c_pow(FW[fk][k], tilt);
        }
        float pr = zone_pan(j), pl = 1.0f - pr;
        for (int i = 0; i < NB; i++) {
            float x = ex[i], o = 0;
            for (int k = 0; k < cm; k++) {
                float re = CHN.m[j][k].re * cr[k] - CHN.m[j][k].im * ci[k] + x * gk[k]; CHN.m[j][k].im = CHN.m[j][k].re * ci[k] + CHN.m[j][k].im * cr[k]; CHN.m[j][k].re = re;
                o += CHN.m[j][k].im;
            }
            eL[i] += o * pl; eR[i] += o * pr;
        }
        for (int k = 0; k < CM; k++) { CHN.m[j][k].re = flush(CHN.m[j][k].re); CHN.m[j][k].im = flush(CHN.m[j][k].im); }
    }
    give(c, SRC_FUEL);
}

/* ================================================================ THE MODERATOR: one deep voice under the hottest channel
 * (UNDERTOW, 3.5: it keeps breaking down into what is under it: its half, its third, its fifth, its seventh, locked to it,
 * coming and going like a fault; more with the steam). Its level follows the heat. */
static struct { uint32_t ph; float hz, a[4], t[4], tim, e; } MO;
static void moderator_block(const sy_ctl *c)
{
    static const uint32_t UM[5] = { 420u, 210u, 140u, 84u, 60u };
    const float *k = c->md;
    for (int n = 0; n < NB; n++) { eL[n] = 0; eR[n] = 0; }
    int h = hottest(); float f = c->zone_hz[h] * bend; while (f >= 82.0f) f *= 0.5f; while (f < 41.0f) f *= 2;
    if (MO.hz < 1) MO.hz = f;
    MO.hz += (f - MO.hz) * rate(0.6f);
    float tg = clamp01(c_sqrt(RX.Pth * 30.0f)), e1 = MO.e + (tg - MO.e) * rate(0.3f + 5.0f * k[3] * k[3]), e = MO.e, de = (e1 - e) / NB; MO.e = e1 < 1e-5f ? 0 : e1;
    if (e1 < 1e-5f && e < 1e-5f) { meter(SRC_MOD, eL, 0); return; }
    MO.tim -= DT * (0.3f + 2.4f * k[1]) * (0.5f + RX.vd_m + tension);
    if (MO.tim <= 0) {
        MO.tim = 0.35f + 1.6f * rnd();
        if (rnd() < 0.3f + 0.5f * k[1]) { for (int j = 0; j < 4; j++) MO.t[j] = 0.12f * rnd(); MO.t[(int)(rnd() * 3.99f)] = 1.0f; } else for (int j = 0; j < 4; j++) MO.t[j] = 0.06f * rnd();
    }
    float a[4], da[4], dep = 1.3f * k[2], drv = 0.5f + 1.4f * k[0];
    for (int j = 0; j < 4; j++) { a[j] = MO.a[j]; float t = a[j] + (MO.t[j] * dep - a[j]) * rate(0.12f); da[j] = (t - a[j]) / NB; MO.a[j] = t; }
    uint32_t inc = (uint32_t)(MO.hz * (1.0f / 420.0f) * (4294967296.0f / SR)), ph = MO.ph;
    for (int n = 0; n < NB; n++) {
        ph += inc; e += de;
        float x = sin_ph(ph * UM[0]) + 0.3f * sin_ph(ph * UM[0] * 2u);
        for (int j = 0; j < 4; j++) { a[j] += da[j]; uint32_t qq = ph * UM[j + 1]; x += a[j] * (sin_ph(qq) + 0.45f * sin_ph(qq * 2u) + 0.2f * sin_ph(qq * 3u)); }
        float o = c_tanh(x * drv) * e * 0.30f; eL[n] = o; eR[n] = o;
    }
    MO.ph = ph;
    give(c, SRC_MOD);
}

static void plant_block(const sy_ctl *c)
{
    for (int n = 0; n < NB; n++) { eL[n] = 0; eR[n] = 0; }
    static float pw[NB];                                                 /* what of it goes to the plate; the cloud takes the groan, the ghost, the door */
    for (int n = 0; n < NB; n++) pw[n] = 0;
    static const float BKF[6] = { 44, 61, 137, 290, 505, 880 }, BKT[6] = { 3.5f, 3.0f, 2.2f, 1.5f, 1.0f, 0.6f }, BKA[6] = { 1.0f, 0.8f, 0.5f, 0.4f, 0.3f, 0.2f };
    if (PT.ln > 0) { PT.lt -= DT; if (PT.lt <= 0) { strike(PT.la, PT.lp * (0.97f + 0.06f * rnd()), PT.l60, PT.lth); PT.ln--; PT.lt = PT.lgap; } }
    if (PT.bk2 > 0) { PT.bk2 -= DT; if (PT.bk2 <= 0) { float w = gene(F_BREAKER); PT.bk2 = 0; boom(BKF, BKT, BKA, 6, 0.45f * kg(F_BREAKER), 0.84f * (1.225f - 0.45f * w), 0.725f + 0.55f * w); } }
    /* ---- the grid let go: a run of relays, then the main breaker falls (and only then the hum goes) */
    if (c->blackout && !PT.bk_was) { PT.rburst = 7; PT.rgap = 0.28f; PT.rt = 0; PT.bk_t = 1.3f; }
    if (!c->blackout && PT.bk_was) { PT.rburst = 3; PT.rgap = 0.22f; PT.rt = 0; PT.bk_t = 0; }
    PT.bk_was = c->blackout; PT.dark = c->blackout;
    if (PT.bk_t > 0) {
        PT.dark = 0; PT.bk_t -= DT;
        if (PT.bk_t <= 0) { float w = gene(F_BREAKER); boom(BKF, BKT, BKA, 6, (KD[F_BREAKER] ? 0.40f : 0.50f) * kg(F_BREAKER), 1.225f - 0.45f * w, 0.725f + 0.55f * w); if (KD[F_BREAKER]) PT.bk2 = 0.19f; PT.dark = 1; }      /* (BREAKER; DOUBLE: it falls in two. Its VARIANT: its size) */
    }
    PT.alive += ((PT.dark ? 0.0f : 1.0f) - PT.alive) * rate(PT.dark ? 0.9f : 1.2f);          /* (the breaker fallen: the hum sinks and dies; back: it climbs again) */
    if (PT.alive < 1e-3f && PT.dark) PT.alive = 0;
    if (PT.alive > 0.9999f) PT.alive = 1;
    /* ---- HUM */
    if (c->plant[P_HUM] > 0.001f) {
        float tg = heat * heat * 0.05f; PT.hsag += (tg - PT.hsag) * rate(6.0f); if (PT.hsag < 1e-7f) PT.hsag = 0;
        PT.hfl_t -= DT;
        if (PT.hfl_t <= 0) { PT.hfl_t = heat > 0.45f ? (0.06f + 0.25f * rnd()) + (rnd() < 0.5f ? 0.0f : (1.5f + 5.0f * rnd()) * (1.2f - heat)) : 1.0f; PT.hfl = heat > 0.45f && PT.hfl > 0.9f && rnd() < 0.5f ? 0.25f + 0.3f * rnd() : 1.0f; }
        float lost = 1.0f - PT.alive, a1 = PT.ha + (PT.hfl * PT.alive * PT.alive - PT.ha) * 0.35f, da = (a1 - PT.ha) / NB, a = PT.ha, hot = 1.0f + 1.0f * heat, g = c->plant[P_HUM] * 0.11f * TRIM[F_HUM][KD[F_HUM]] * (0.2f + 0.8f * clamp01(heat * 1.4f));      /* (the generator's load: the reactor's heat) */
        const float *hk = HK[KD[F_HUM]];
        float hz = hk[0] * (0.92f + 0.16f * gene(F_HUM));
        uint32_t i0 = inc_of(hz * c_exp2(-1.6f * lost * lost)), i1 = inc_of(hz * 1.0014f * (1.0f - PT.hsag) * (0.985f + 0.015f * a1) * c_exp2(-2.4f * lost * lost));
        int nh = eco >= 2 ? 4 : 6;
        float w[6]; for (int h = 0; h < 6; h++) w[h] = hk[1 + h] * (h == 1 ? 1.0f : h & 1 ? 0.6f + 0.4f * hot : hot * 0.5f);
        if (a1 < 1e-6f && a < 1e-6f) { a = 0; da = 0; g = 0; }
        for (int n = 0; n < NB && g > 0; n++) {
            a += da; PT.hph[0] += i0; PT.hph[1] += i1;
            float x = 0, y = 0; for (int h = 0; h < nh; h++) { x += w[h] * sin_ph(PT.hph[0] * (uint32_t)(h + 1)); y += w[h] * sin_ph(PT.hph[1] * (uint32_t)(h + 1)); }
            x *= g * a; y *= g * a; eL[n] += 0.8f * x + 0.35f * y; eR[n] += 0.35f * x + 0.8f * y; pw[n] += (x + y) * 0.25f;
        }
        PT.ha = a1 < 1e-6f ? 0 : a1;
    }
    /* ---- TURBINE: every octave of the shaft at once, falling (cooling) or rising (played hard) */
    if (c->plant[P_TURBINE] > 0.001f) {
        float r = 0.42f * (tension - heat) - 0.028f; if (r > 0.25f) r = 0.25f; if (r < -0.30f) r = -0.30f;
        if (KD[F_TURBINE] == 1) r = -r;                               /* (RISING: a climb without end, and it falls when played hard) */
        else if (KD[F_TURBINE] == 2) { PT.tide += DT / (24.0f + 16.0f * gene(F_TURBINE)); if (PT.tide >= 1) PT.tide -= 1; r = 0.07f * sin_ph((uint32_t)(PT.tide * 4294967040.0f)) + 0.25f * (tension - heat); }   /* (TIDAL: up and down, slowly) */
        PT.tu += r * DT; while (PT.tu < 0) PT.tu += 1; while (PT.tu >= 1) PT.tu -= 1;
        PT.tlev += (1.0f - PT.tlev) * rate(3.0f);
        float cen = 2.3f + 1.2f * heat + (gene(F_TURBINE) - 0.5f) * 0.8f, lv = c->plant[P_TURBINE] * 0.12f * TRIM[F_TURBINE][KD[F_TURBINE]] * (0.15f + 0.85f * clamp01(heat * 1.4f)) * PT.tlev, da[TJ]; uint32_t ia[TJ], ib[TJ];
        for (int j = 0; j < TJ; j++) {
            float pos = (float)j + PT.tu, f = 27.5f * c_exp2(pos), dd = (pos - cen) / 1.25f, a = c_exp(-dd * dd) * c_sin(C_PI * pos / TJ) * lv;
            if (f > SR * 0.2f) a = 0;
            ia[j] = inc_of(f); ib[j] = inc_of(f + 0.4f + 0.0009f * f); da[j] = (a - PT.ta[j]) / NB;
        }
        for (int n = 0; n < NB; n++) {
            float x = 0, y = 0;
            for (int j = 0; j < TJ; j++) { PT.ta[j] += da[j]; PT.tph[j] += ia[j]; PT.ttw[j] += ib[j]; float s0 = sin_ph(PT.tph[j]); x += PT.ta[j] * (s0 + 0.30f * sin_ph(PT.tph[j] * 2u)); y += PT.ta[j] * (sin_ph(PT.ttw[j]) + 0.30f * s0); }
            eL[n] += x; eR[n] += y; pw[n] += (x + y) * 0.35f;
        }
        for (int j = 0; j < TJ; j++) if (PT.ta[j] < 1e-9f) PT.ta[j] = 0;
    }
    /* ---- PUMP: HEART a double beat; TRIPLE three; SLOW one, heavy, lower. Their VARIANT: their pitch */
    if (c->plant[P_PUMP] > 0.001f) {
        static const struct { uint8_t nb; float gap, per, a[3], f0[3], f1, dec; } PK[3] = {
            { 2, 0.24f, 1.0f, { 1.0f, 0.7f, 0 }, { 50, 43, 0 }, 36, 0.085f }, { 3, 0.19f, 1.1f, { 1.0f, 0.55f, 0.8f }, { 55, 47, 50 }, 38, 0.075f },
            { 1, 0, 1.5f, { 1.25f, 0, 0 }, { 42, 0, 0 }, 27, 0.17f } };
        int k = KD[F_PUMP], nb = PK[k].nb; float pg = 0.88f + 0.24f * gene(F_PUMP);
        PT.pt -= DT;
        if (PT.pt <= 0) {
            PT.pn = (PT.pn + 1) % nb;
            if (PT.pn == 0) { PT.ksync = 1; PT.pper = (1.55f - 0.85f * heat) * PK[k].per; }
            PT.pt = PT.pn < nb - 1 ? PK[k].gap : PT.pper - (float)(nb - 1) * PK[k].gap; if (PT.pt < 0.05f) PT.pt = 0.05f;
            PT.pe = PK[k].a[PT.pn] * TRIM[F_PUMP][k] * c->plant[P_PUMP] * 0.65f * (0.25f + 0.85f * clamp01(heat)) * PT.alive; PT.pf = PK[k].f0[PT.pn] * pg; PT.pph = 0;
        }
        if (PT.pe > 1e-5f) {
            float f1 = PK[k].f1 * pg, dec = c_exp(-1.0f / (PK[k].dec * SR)), fd = c_exp(-1.0f / (0.25f * SR));
            for (int n = 0; n < NB; n++) { PT.pph += inc_of(PT.pf); PT.pf = f1 + (PT.pf - f1) * fd; float x = sin_ph(PT.pph) * PT.pe; PT.pe *= dec; eL[n] += x; eR[n] += x; pw[n] += x * 0.3f; }
        } else PT.pe = 0;
    }
    /* ---- GEIGER: the dosimeter (3.3: the dry click is gone; the two kept by ear). All but silent cold; it follows the
     * plant: the heat and, at once, the tension of the hands. On its own battery: it goes on in a blackout.
     *   DRIPS  soft low drops, far in the room, at random
     *   PULSE  a wooden tock that beats with the pumps: one a beat cold, then two, four, eight */
    if (c->plant[P_GEIGER] > 0.001f) {
        float lv = c->plant[P_GEIGER], d = clamp01(heat + 0.35f * tension * (1.0f - heat)), sur = 1.0f, pg = 0.8f + 0.45f * gene(F_GEIGER);
        int pulse = KD[F_GEIGER] == 1, fire = 0, acc = 0, pumps = c->plant[P_PUMP] > 0.001f;
        if (pulse) {
            int nb = d < 0.2f ? 1 : d < 0.45f ? 2 : d < 0.7f ? 4 : 8; float per = pumps && PT.pper > 0.3f ? PT.pper : 1.55f - 0.85f * heat;
            PT.kph += DT * (float)nb / per * sur;
            if (PT.ksync && pumps) { if (PT.kph >= 0.15f) { fire = 1; acc = 1; } PT.kph = 0; PT.kacc = 0; }      /* (the pumps' strong beat: its own first) */
            else if (PT.kph >= 1.0f) { PT.kph -= 1.0f; if (PT.kph > 1.0f) PT.kph = 0; fire = 1; PT.kacc = (PT.kacc + 1) % nb; acc = PT.kacc == 0; }
        } else fire = rnd() < (0.3f + 0.7f * lv) * (0.25f + 11.0f * c_pow(d, 1.7f)) * sur * DT;
        PT.krate = 40.0f * d;
        if (fire) {
            if (pulse) { float a = (acc ? 1.0f : 0.55f) * lv * TRIM[F_GEIGER][1]; gtick(380.0f * pg, 0.06f, 0.42f * a); gtick(1140.0f * pg, 0.03f, 0.17f * a); }
            else { float q = rnd(); gtick(520.0f * pg * c_exp2(0.9f * rnd()), 0.07f, (0.3f + 0.7f * q * q) * 0.40f * lv * TRIM[F_GEIGER][0]); }
        }
        float e = 0; for (int j = 0; j < 6; j++) e += c_absf(PT.gm[j].re) + c_absf(PT.gm[j].im);
        if (e > 1e-6f) {
            float dry = pulse ? 0.60f : 0.30f, wet = pulse ? 0.7f : 1.2f, l0 = PT.klp[0], l1 = PT.klp[1];
            for (int j = 0; j < 6; j++) if (c_absf(PT.gm[j].re) + c_absf(PT.gm[j].im) < 1e-12f) { PT.gm[j].re = 0; PT.gm[j].im = 0; }
            for (int n = 0; n < NB; n++) {
                float x = 0, y = 0;
                for (int j = 0; j < 6; j++) { float re = PT.gm[j].re * PT.gm[j].cr - PT.gm[j].im * PT.gm[j].ci; PT.gm[j].im = PT.gm[j].re * PT.gm[j].ci + PT.gm[j].im * PT.gm[j].cr; PT.gm[j].re = re; if (j & 1) y += PT.gm[j].im; else x += PT.gm[j].im; }
                l0 += 0.5f * (x - l0); l1 += 0.5f * (y - l1);                           /* (rounded: 2.4 kHz) */
                eL[n] += (l0 + 0.4f * l1) * dry; eR[n] += (l1 + 0.4f * l0) * dry; pw[n] += (l0 + l1) * wet;
            }
            PT.klp[0] = flush(l0); PT.klp[1] = flush(l1);
        } else { for (int j = 0; j < 6; j++) { PT.gm[j].re = 0; PT.gm[j].im = 0; } PT.klp[0] = 0; PT.klp[1] = 0; }
    } else PT.krate = 0;
    PT.ksync = 0;
    /* ---- RELAY: one now and then; hot, in runs that quicken */
    {   float lv = c->plant[P_RELAY];
        PT.rt -= DT; PT.rshow -= DT / 0.3f; if (PT.rshow < 0) PT.rshow = 0;
        if (PT.rburst > 0) {
            if (PT.rt <= 0) { clack(lv > 0.6f ? lv : 0.6f); PT.rburst--; PT.rgap *= 0.8f; if (PT.rgap < 0.06f) PT.rgap = 0.06f; PT.rt = PT.rburst ? PT.rgap * (0.6f + 0.8f * rnd()) : (3.0f + 12.0f * rnd()) / (0.75f + 3.0f * heat); }
        } else if (PT.rt <= 0 && lv > 0.001f && PT.alive > 0.5f) {
            if (heat > 0.4f && rnd() < 0.6f) { PT.rburst = 3 + (int)(rnd() * 4.99f); PT.rgap = 0.35f; PT.rt = 0; }
            else { clack(lv); PT.rt = (3.0f + 12.0f * rnd()) / (0.75f + 3.0f * heat); }
        }
        float e = 0; for (int j = 0; j < 4; j++) e += c_absf(PT.rl[j].re) + c_absf(PT.rl[j].im);
        if (e > 1e-5f) {
            for (int j = 0; j < 4; j++) if (c_absf(PT.rl[j].re) + c_absf(PT.rl[j].im) < 1e-12f) { PT.rl[j].re = 0; PT.rl[j].im = 0; }
            for (int n = 0; n < NB; n++) {
                float x = 0;
                for (int j = 0; j < 4; j++) { float re = PT.rl[j].re * PT.rl[j].cr - PT.rl[j].im * PT.rl[j].ci; PT.rl[j].im = PT.rl[j].re * PT.rl[j].ci + PT.rl[j].im * PT.rl[j].cr; PT.rl[j].re = re; x += PT.rl[j].im; }
                eL[n] += x * 0.9f; eR[n] += x * 0.6f; pw[n] += x * 0.8f; cin[n] += x * 0.3f;
            }
        } else for (int j = 0; j < 4; j++) { PT.rl[j].re = 0; PT.rl[j].im = 0; }
    }
    /* ---- GROAN: a length of steel that moans and bends (a tuned line, fed by a slow rumble, its length sliding) */
    if (c->plant[P_GROAN] > 0.001f) {
        PT.gt -= DT;
        if (!PT.gon && PT.gt <= 0 && (heat > 0.04f || PT.gforce)) {
            PT.gforce = 0;
            static const float IV[5] = { 1.0f, 1.0595f, 1.4142f, 0.9439f, 1.5f };
            float low = 0; for (int j = 0; j < SY_NZ; j++) if (RX.rp[j] > 0.5f && (low == 0 || c->zone_hz[j] < low)) low = c->zone_hz[j];
            if (low < 1) low = 73.42f;
            int gk = KD[F_GROAN]; float lo = gk == 1 ? 92.0f : gk == 3 ? 23.0f : 46.0f;      /* BEND; WHALE an octave up, it rises and comes back; CREAK short, in jerks; DEEP an octave under, long */
            while (low >= 2.0f * lo) low *= 0.5f;
            while (low < lo) low *= 2;
            PT.gkind = (uint8_t)gk; PT.gst = 0;
            PT.ghz = low * IV[(int)(rnd() * 4.99f)] * c_exp2((gene(F_GROAN) - 0.5f) * 0.5f); PT.gb0 = 0; PT.gb1 = gk == 1 ? 2.0f + 2.0f * rnd() : (rnd() < 0.75f ? -1.0f : 1.0f) * (1.0f + 2.5f * rnd()); PT.gu = 0; PT.gdur = gk == 0 ? 3.0f + 3.5f * rnd() : gk == 1 ? 5.0f + 3.0f * rnd() : gk == 2 ? 2.0f + rnd() : 6.0f + 3.0f * rnd(); PT.gon = 1;
        }
        if (PT.gon || PT.ge > 1e-4f) {
            PT.gu += DT / PT.gdur;
            float et = PT.gon && PT.gu < 0.7f ? 1.0f : 0.0f; if (PT.gu >= 1 && PT.gon) { PT.gon = 0; PT.gt = (3.0f + 8.0f * rnd()) / (0.35f + heat); }
            float e1 = PT.ge + (et - PT.ge) * rate(et > PT.ge ? 0.9f : 1.3f), e = PT.ge, de = (e1 - e) / NB;
            float uu = PT.gu > 1 ? 1 : PT.gu, semis = PT.gkind == 1 ? PT.gb1 * c_sin(C_PI * uu) : PT.gb0 + (PT.gb1 - PT.gb0) * uu * uu, fb = PT.gkind == 2 ? 0.975f : PT.gkind == 1 ? 0.992f : 0.988f, jerk = 1.0f, lpc = PT.gkind == 1 ? 0.8f : 0.45f, gkg = TRIM[F_GROAN][PT.gkind], f = PT.ghz * c_exp2(semis / 12.0f), D = SR / f - 2.2f, g = c->plant[P_GROAN] * 0.8f;
            if (D > 2040) D = 2040;
            int di = (int)D; float df = D - (float)di; uint32_t inc = inc_of(f * 0.5f);
            if (PT.gkind == 2) { PT.gst += (9.0f - 5.0f * uu) * DT; jerk = PT.gst - (float)(int)PT.gst < 0.5f ? 1.0f : 0.0f; }
            for (int n = 0; n < NB; n++) {
                e += de; PT.gph += inc;
                PT.gnz += 0.02f * ((rnd() - 0.5f) - PT.gnz); PT.gn2 += 0.02f * (PT.gnz - PT.gn2);
                float x = e * jerk * ((PT.gkind == 1 ? (PT.gnz - PT.gn2) * 1.6f : PT.gn2 * 2.2f) + 0.012f * sin_ph(PT.gph));
                float y0 = PT.gd[(PT.gw - di) & 2047], y1 = PT.gd[(PT.gw - di - 1) & 2047], y = y0 + (y1 - y0) * df;
                PT.glp += lpc * (y - PT.glp);
                float o = x + fb * PT.glp; PT.gd[PT.gw & 2047] = o; PT.gw++;
                o *= g * gkg; eL[n] += o * 0.4f; eR[n] += o * 0.7f; pw[n] += o * 0.9f; cin[n] += o * 0.5f;
            }
            PT.ge = e1; PT.glp = flush(PT.glp);
        }
    }
    /* ---- GHOST (3.1: no longer a thin high tone: it whistled). A low shadow under the note held longest: a semitone
     * beneath it, down in the second octave, sliding flatter as the heat rises, trembling: the note's wrong double */
    if (c->plant[P_GHOST] > 0.001f) {
        int hj = hottest(); float top = RX.n[hj] > 0.05f ? c->zone_hz[hj] * bend : 0.0f, pr = clamp01(RX.n[hj]);
        if (top > 0) { float f = top * 0.9439f; while (f < 98.0f) f *= 2; while (f >= 196.0f) f *= 0.5f; if (PT.qhz < 1) PT.qhz = f; PT.qhz += (f - PT.qhz) * rate(1.5f); }
        float tg = top > 0 ? c->plant[P_GHOST] * 0.17f * clamp01(pr * 1.6f - 0.25f) * (0.5f + 0.5f * tension) : 0.0f;
        float a1 = PT.qa + (tg - PT.qa) * rate(tg > PT.qa ? 3.5f : 2.5f);
        if (a1 > 1e-5f) {
            PT.qw += ((rnd() - 0.5f) * 2.0f - PT.qw) * 0.02f; PT.qt += ((rnd() - 0.5f) * 2.0f - PT.qt) * 0.05f;
            uint32_t inc = inc_of(PT.qhz * c_exp2((PT.qw * 1.2f - c->horror * heat * 0.6f) / 12.0f));
            float a = PT.qa * (1.0f + 0.8f * PT.qt), b = a1 * (1.0f + 0.8f * PT.qt), da = (b - a) / NB;
            for (int n = 0; n < NB; n++) {
                a += da; PT.qph += inc;
                float x = (sin_ph(PT.qph) + 0.35f * sin_ph(PT.qph * 2u) + 0.14f * sin_ph(PT.qph * 3u)) * a;
                eL[n] += x * 0.45f; eR[n] += x * 0.6f; pw[n] += x * 1.0f; cin[n] += x * 0.4f;
            }
        }
        PT.qa = a1 < 1e-5f ? 0 : a1;
    }
    /* ---- the door, the knock */
    {   float e = c_absf(PT.dthr) + c_absf(PT.dthi); for (int j = 0; j < 10; j++) e += c_absf(PT.dm[j].re) + c_absf(PT.dm[j].im);
        if (e > 1e-5f) {
            if (c_absf(PT.dthr) + c_absf(PT.dthi) < 1e-12f) { PT.dthr = 0; PT.dthi = 0; }      /* (what has died is dead: the thud is gone long before a door's low modes) */
            for (int j = 0; j < 10; j++) if (c_absf(PT.dm[j].re) + c_absf(PT.dm[j].im) < 1e-12f) { PT.dm[j].re = 0; PT.dm[j].im = 0; }
            for (int n = 0; n < NB; n++) {
                float x = 0, y = 0;
                for (int j = 0; j < 10; j++) { float re = PT.dm[j].re * PT.dm[j].cr - PT.dm[j].im * PT.dm[j].ci; PT.dm[j].im = PT.dm[j].re * PT.dm[j].ci + PT.dm[j].im * PT.dm[j].cr; PT.dm[j].re = re; if (j & 1) y += PT.dm[j].im; else x += PT.dm[j].im; }
                float re = PT.dthr * PT.dtc - PT.dthi * PT.dts; PT.dthi = PT.dthr * PT.dts + PT.dthi * PT.dtc; PT.dthr = re;
                eL[n] += x + 0.3f * y + PT.dthi; eR[n] += y + 0.3f * x + PT.dthi; pw[n] += (x + y) * 0.9f; cin[n] += (x + y) * 0.4f;
            }
        } else { for (int j = 0; j < 10; j++) { PT.dm[j].re = 0; PT.dm[j].im = 0; } PT.dthr = 0; PT.dthi = 0; }
    }
    for (int n = 0; n < NB; n++) { mixL[n] += eL[n]; mixR[n] += eR[n]; rin[n] += pw[n]; }
}

/* ================================================================ what chills: the gestures (3.2; 3.3: their kinds) */
static void gesture_block(const sy_ctl *c)
{
    static float gl[NB], gr[NB]; int any = 0;
    GS.clock += DT; if (c->root_hz > 20) ev_root = c->root_hz;
    for (int n = 0; n < NB; n++) { gl[n] = 0; gr[n] = 0; }
    const int16_t *saw = SAWT;
    /* hot for the first time in a while: a blow by itself (HORROR past its middle) */
    if (heat > 0.72f && !GS.hot_was) { GS.hot_was = 1; if (c->horror > 0.5f && GS.clock - GS.b_auto > 30.0f) { GS.b_auto = GS.clock; braam_now(ev_root); } }
    else if (heat < 0.55f) GS.hot_was = 0;
    /* ---- RISER (held): twelve strings, until let go: it stops dead.
     *   STRINGS  from a tight cluster, each sliding up its own way      FALL     from high and wide, down onto one note
     *   TREMOLO  a tight cluster that shivers, faster and faster        ENDLESS  a climb that never arrives, as long as it is held */
    if (c->riser && !GS.r_was && !GS.r_on) {
        int k = KD[F_RISER]; float f = 0; for (int j = 0; j < SY_NZ; j++) if (RX.rp[j] > 0.5f && (f == 0 || c->zone_hz[j] < f)) f = c->zone_hz[j];
        if (f < 1) f = ev_root;
        while (f >= 220.0f) f *= 0.5f;
        while (f < 110.0f) f *= 2;
        GS.r_kind = k; GS.r_dur = 7.5f + 4.0f * gene(F_RISER); GS.r_base = f; GS.r_t = 0; GS.r_g = 0; GS.r_tr = 0; GS.r_on = 1;
        for (int v = 0; v < RV; v++) {
            float a = rnd(), b = rnd(), e = rnd();
            GS.r_st[v] = k == 0 ? -2.0f + 4.0f * a : k == 1 ? 8.0f + 16.0f * a : k == 2 ? -1.5f + 3.0f * a : 4.0f * (float)v;
            GS.r_up[v] = k == 0 ? 6.0f + 20.0f * b : k == 1 ? -GS.r_st[v] - 0.3f + 0.6f * b : k == 2 ? 0.5f + 2.0f * b : 0.0f;
            GS.r_ex[v] = k == 1 ? 0.7f + 0.6f * e : 1.5f + e; GS.r_ph[v] = (uint32_t)(rnd() * 4294967040.0f);
        }
        for (int ch = 0; ch < 2; ch++) { GS.r_f[ch].s1 = 0; GS.r_f[ch].s2 = 0; svf_set(&GS.r_f[ch], 3000.0f, 0.75f); }
    }
    GS.r_was = c->riser;
    if (GS.r_on) {
        int k = GS.r_kind; float u = GS.r_t / GS.r_dur, tr = 1.0f; int stop = !c->riser || (k != 3 && u >= 1.0f), nv = eco ? (eco > 1 ? 6 : 8) : RV; if (u > 1) u = 1;      /* (short of time: fewer strings, each louder) */
        if (k == 2) { GS.r_tr += (5.0f + 9.0f * u) * DT; if (GS.r_tr >= 1) GS.r_tr -= 1; float q = 0.5f + 0.5f * sin_ph((uint32_t)(GS.r_tr * 4294967040.0f)); tr = 0.2f + 0.8f * q * q; }
        float g1 = stop ? 0.0f : (0.3f + 0.7f * u * c_sqrt(u)) * clamp01(GS.r_t / 1.5f) * (1.0f / 32000.0f) * 0.15f * kg(F_RISER) * c_sqrt((float)RV / (float)nv) * tr, g = GS.r_g, dg = (g1 - g) / NB;
        uint32_t inc[RV]; const int16_t *t[RV]; int sh[RV]; float gv[RV];
        for (int v = 0; v < nv; v++) {
            float semis; gv[v] = 1.0f;
            if (k == 3) { float pos = (float)v / RV + GS.r_t / (GS.r_dur * 1.6f); pos -= (float)(int)pos; semis = pos * 48.0f - 12.0f; float w = c_sin(C_PI * pos); gv[v] = w * w * 1.6f; }
            else semis = GS.r_st[v] + GS.r_up[v] * c_pow(u, GS.r_ex[v]);
            float f = GS.r_base * c_exp2(semis / 12.0f); int mip = wt_mip(f); inc[v] = inc_of(f); t[v] = saw + MOFF[mip]; sh[v] = MSH[mip];
        }
        for (int n = 0; n < NB; n++) {
            float a = 0, b = 0; g += dg;
            for (int v = 0; v < nv; v += 2) { GS.r_ph[v] += inc[v]; GS.r_ph[v + 1] += inc[v + 1]; a += wt_at(t[v], sh[v], GS.r_ph[v]) * gv[v]; b += wt_at(t[v + 1], sh[v + 1], GS.r_ph[v + 1]) * gv[v + 1]; }
            gl[n] += svf_lp(&GS.r_f[0], (a + 0.3f * b) * g); gr[n] += svf_lp(&GS.r_f[1], (b + 0.3f * a) * g);
        }
        GS.r_g = g1; GS.r_t += DT; any = 1;
        if (stop) GS.r_on = 0;
    }
    /* ---- BRAAM: seven saws on the low root, through a low-pass that opens and closes, driven hard.
     *   BRASS  root, octave, fifth      TRITONE  the fifth is a tritone      GRIND  semitones against each other
     *   HORN   it swells in half a second and opens slowly: a horn from the deep */
    if (GS.b_on) {
        static const float BR[4][7] = { { 1.0f, 1.004f, 0.996f, 2.0f, 2.008f, 1.498f, 0.5f }, { 1.0f, 1.004f, 0.996f, 1.4142f, 2.008f, 2.8284f, 0.5f },
                                        { 1.0f, 1.0595f, 0.5f, 0.5297f, 2.0f, 2.119f, 1.004f }, { 1.0f, 1.004f, 0.996f, 2.0f, 2.008f, 1.498f, 0.5f } };
        int horn = GS.b_kind == 3; const float *RT = BR[GS.b_kind];
        float t0 = GS.b_t, t1 = t0 + DT, br = 0.55f + 0.9f * gene(F_BRAAM), cut = horn ? 120.0f + 1300.0f * br * (1.0f - c_exp(-t0 / 0.7f)) * c_exp(-t0 / 2.6f) : 120.0f + 1500.0f * br * c_exp(-t0 / 0.9f) * clamp01(t0 / 0.12f), k = 1.0f - c_exp(-C_TWO_PI * cut / SR);
        float e1 = c_exp(-t1 / (horn ? 3.0f : 2.2f)) * (GS.b_a0 + (1.0f - GS.b_a0) * clamp01(t1 / (horn ? 0.5f : 0.03f))), e = GS.b_e, de = (e1 - e) / NB;
        uint32_t inc[7]; const int16_t *t[7]; int sh[7];
        for (int j = 0; j < 7; j++) { float f = GS.b_hz * RT[j]; int mip = wt_mip(f); inc[j] = inc_of(f); t[j] = saw + MOFF[mip]; sh[j] = MSH[mip]; }
        float l1 = GS.b_l1, l2 = GS.b_l2, bgn = 0.50f * FG[F_BRAAM] * TRIM[F_BRAAM][GS.b_kind];
        for (int n = 0; n < NB; n++) {
            float x = 0; e += de;
            for (int j = 0; j < 7; j++) { GS.b_ph[j] += inc[j]; x += wt_at(t[j], sh[j], GS.b_ph[j]); }
            l1 += k * (x * (1.0f / 32000.0f) - l1); l2 += k * (l1 - l2);
            float o = c_tanh(l2 * 0.75f) * e * bgn; gl[n] += o; gr[n] += o;
        }
        GS.b_l1 = flush(l1); GS.b_l2 = flush(l2); GS.b_e = e1; GS.b_t += DT; any = 1;
        if (GS.b_t > (horn ? 11.0f : 9.0f)) { GS.b_on = 0; GS.b_l1 = 0; GS.b_l2 = 0; GS.b_e = 0; }
    }
    /* ---- SWELL (held): it comes out of nothing and grows; let go (or full): it is cut.
     *   BREATH   a breath and two close low tones, cut on a dull blow      BELL  a bell backwards, cut on the bell itself
     *   VOID     two close tones climbing a fourth, cut on nothing        SHUDDER  the breath, shaking, slower and slower */
    if (c->swell && !GS.s_was && !GS.s_on) {
        int k = KD[F_SWELL]; float f = ev_root, lo = k == 1 ? 110.0f : 82.0f;
        while (f >= 2.0f * lo) f *= 0.5f;
        while (f < lo) f *= 2;
        GS.s_kind = k; GS.s_tau = 1.0f + 0.6f * gene(F_SWELL); GS.s_hz = f; GS.s_t = 0; GS.s_g = 0; GS.s_tr = 0; GS.s_on = 1; GS.s_hp = 0; GS.s_lp = 0; GS.s_l0 = 0;
    }
    GS.s_was = c->swell;
    if (GS.s_on) {
        static const float SWR[4][4] = { { 1, 1.0595f, 0, 0 }, { 1, 1.593f, 2.136f, 2.65f }, { 1, 1.0595f, 0, 0 }, { 1, 1.0595f, 0, 0 } },
                           SWA[4][4] = { { 0.5f, 0.4f, 0, 0 }, { 0.5f, 0.3f, 0.2f, 0.15f }, { 0.6f, 0.5f, 0, 0 }, { 0.5f, 0.4f, 0, 0 } };
        int k = GS.s_kind; float T = 4.6f * GS.s_tau, u = clamp01(GS.s_t / T), tr = 1.0f; int stop = !c->swell || GS.s_t >= T + 0.3f;
        float lvl = c_exp((GS.s_t - T) / GS.s_tau); if (lvl > 1) lvl = 1;
        if (k == 3) { GS.s_tr += (9.0f - 5.0f * u) * DT; if (GS.s_tr >= 1) GS.s_tr -= 1; tr = 0.35f + 0.65f * (0.5f + 0.5f * sin_ph((uint32_t)(GS.s_tr * 4294967040.0f))); }
        float sg = FG[F_SWELL] * TRIM[F_SWELL][k], g1 = stop ? 0.0f : lvl * 0.42f * tr * sg, g = GS.s_g, dg = (g1 - g) / NB, bend = k == 2 ? c_exp2(5.0f * u / 12.0f) : 1.0f, nzg = k == 0 || k == 3 ? 1.3f : 0.0f;
        uint32_t inc[4]; for (int j = 0; j < 4; j++) inc[j] = inc_of(GS.s_hz * SWR[k][j] * bend);
        for (int n = 0; n < NB; n++) {
            float o = 0; g += dg;
            for (int j = 0; j < 4; j++) { GS.s_ph[j] += inc[j]; o += SWA[k][j] * sin_ph(GS.s_ph[j]); }
            float w = rnd() - 0.5f; GS.s_l0 += 0.63f * (w - GS.s_l0); GS.s_lp += 0.63f * (GS.s_l0 - GS.s_lp); GS.s_hp += 0.082f * (GS.s_lp - GS.s_hp);      /* (300 Hz .. 3.5 kHz, twice over at the top) */
            float nz = (GS.s_lp - GS.s_hp) * nzg; o *= g;
            gl[n] += o + nz * g; gr[n] += o + nz * g * 0.8f;
        }
        GS.s_g = g1; GS.s_t += DT; any = 1;
        if (stop) {
            static const float F[3] = { 41, 58, 95 }, T3[3] = { 1.8f, 1.4f, 1.0f }, A[3] = { 1.0f, 0.7f, 0.4f }, T4[4] = { 4.0f, 3.0f, 2.5f, 2.0f }, A4[4] = { 1.0f, 0.6f, 0.4f, 0.3f };
            if (k == 1) boom(SWR[1], T4, A4, 4, 0.35f * sg * clamp01(lvl * 3.0f), GS.s_hz, 1.0f);
            else if (k != 2) boom(F, T3, A, 3, 0.62f * sg * clamp01(lvl * 3.0f), 1.0f, 1.0f);
            GS.s_on = 0;
        }
    }
    /* ---- DROP: a low tone and its close double.
     *   FALL  95 Hz down to 26      BOMB  from high, fast, onto a blow      FROM BELOW  the other way: it comes up, and grows
     * Its VARIANT: how long it takes (FALL: four to ten seconds) */
    if (GS.d_on) {
        static const float DK[3][4] = { { 95, 26, 7, 1.06f }, { 380, 30, 4, 1.03f }, { 24, 88, 9, 1.06f } };
        int kd = GS.d_kind; const float *k = DK[kd]; float T = k[2] * (0.6f + 0.8f * gene(F_DROP)), u = clamp01(GS.d_t / T), f = k[0] * c_exp2(c_log2(k[1] / k[0]) * u), tail = kd == 1 ? 0.3f : 1.0f, dgn = FG[F_DROP] * TRIM[F_DROP][kd];
        float g1 = clamp01(GS.d_t / 0.4f) * clamp01((T + tail - GS.d_t) / tail) * 0.20f * dgn * (kd == 2 ? 0.3f + 0.7f * u : 1.0f), g = GS.d_g, dg = (g1 - g) / NB; uint32_t i0 = inc_of(f), i1 = inc_of(f * k[3]);
        if (kd == 1 && GS.d_t < T && GS.d_t + DT >= T) boom(BF[0], BT[0], BA[0], 8, 0.30f * dgn, 1.0f, 1.0f);
        for (int n = 0; n < NB; n++) {
            g += dg; GS.d_ph += i0; GS.d_p2 += i1;
            float o = (sin_ph(GS.d_ph) + 0.5f * sin_ph(GS.d_ph * 2u) + 0.25f * sin_ph(GS.d_ph * 3u) + 0.8f * sin_ph(GS.d_p2)) * g; gl[n] += o; gr[n] += o;
        }
        GS.d_g = g1; GS.d_t += DT; any = 1;
        if (GS.d_t >= T + tail) { GS.d_on = 0; GS.d_g = 0; }
    }
    /* ---- STINGER: a blow (on the door's modes), a burst, and for some a high cluster that screams and dies.
     *   SHOCK  the three      CLUSTER  a fist low on a piano      SCREECH  the scream, falling      ANVIL  struck iron */
    if (GS.k_on) {
        const stk *s = &SK[GS.k_kind];
        float t0 = GS.k_t, en = c_exp(-t0 / 0.05f) * s->noise * GS.k_gain, dn = c_exp(-1.0f / (0.05f * SR)), es = 0, ds = 1;
        uint32_t inc[5] = { 0, 0, 0, 0, 0 }; int mip = 3;
        if (s->scr > 0) {
            float hz = s->hz * (1.125f - 0.25f * gene(F_STINGER)) * c_exp2(s->gl * clamp01(t0 / 0.9f) / 12.0f);
            es = c_exp(-t0 / s->dec) * (s->scr * GS.k_gain / 5.0f / 32000.0f); ds = c_exp(-1.0f / (s->dec * SR)); mip = wt_mip(hz * 1.13f);
            for (int j = 0; j < 5; j++) inc[j] = inc_of(hz * c_exp2((float)(j - 2) / 12.0f));
        }
        const int16_t *t = saw + MOFF[mip];
        if (t0 == 0) { svf_set(&GS.k_f, 4500.0f, 0.75f); GS.k_f.s1 = 0; GS.k_f.s2 = 0; GS.k_lp = 0; GS.k_hp = 0; }
        for (int n = 0; n < NB; n++) {
            float w = rnd() - 0.5f; GS.k_lp += 0.82f * (w - GS.k_lp); GS.k_hp += 0.055f * (GS.k_lp - GS.k_hp);
            float o = (GS.k_lp - GS.k_hp) * 2.0f * en; en *= dn;
            if (s->scr > 0) { float x = 0; for (int j = 0; j < 5; j++) { GS.k_ph[j] += inc[j]; x += wt_at(t, MSH[mip], GS.k_ph[j]); } o += svf_lp(&GS.k_f, x * es); es *= ds; }
            gl[n] += o; gr[n] += o * 0.9f;
        }
        GS.k_t += DT; any = 1;
        if (GS.k_t > (s->scr > 0 ? 5.5f * s->dec : 0.5f)) GS.k_on = 0;
    }
    if (any) for (int n = 0; n < NB; n++) { mixL[n] += gl[n]; mixR[n] += gr[n]; rin[n] += (gl[n] + gr[n]) * 0.65f; cin[n] += (gl[n] + gr[n]) * 0.25f; }
}

/* ---- SHIFT (3.5): every frequency moved by `hz` (single sideband): two all-pass chains a quarter turn apart (O. Niemitalo's), then a turn */
static struct { float hx[3][2][4][2], hy[3][2][4][2], hd[3]; uint32_t sph[3]; } LB;
static void shift(float *x, int ch, float hz)
{
    static const float A[2][4] = { { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f }, { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f } };
    uint32_t inc = (uint32_t)(int32_t)(hz * (4294967296.0f / SR));
    for (int n = 0; n < NB; n++) {
        float q[2];
        for (int pth = 0; pth < 2; pth++) {
            float s = x[n];
            for (int k = 0; k < 4; k++) {
                float a2 = A[pth][k] * A[pth][k], *X = LB.hx[ch][pth][k], *Y = LB.hy[ch][pth][k], y = a2 * (s + Y[1]) - X[1];
                X[1] = X[0]; X[0] = s; Y[1] = flush(Y[0]); Y[0] = y; s = y;
            }
            q[pth] = s;
        }
        float i = LB.hd[ch]; LB.hd[ch] = q[0];                              /* (the first path, a sample late) */
        LB.sph[ch] += inc;
        x[n] = i * sin_ph(LB.sph[ch] + 0x40000000u) + q[1] * sin_ph(LB.sph[ch]);      /* (hz under 0: down) */
    }
}

/* ================================================================ THE HALL: the memory, the echo, the grains (at a quarter of the rate)
 * 4.0: what comes back is played into a room that keeps only its own resonances, a little more each time round
 * (GENERATIONS): what was played dissolves, generation after generation, into the hall's drone */
#define EN 65536
#define EGN 6
#if defined(CRITICAL_FW)
static int16_t EM[EN] __attribute__((section(".lowbss")));
#else
static int16_t EM[EN];
#endif
typedef struct { float pos, inc, gl, gr, u, du; int n; } grain;
static struct { uint32_t w; float d, lp, hp, env, agc; grain g[EGN]; float eh[NB + 16], ilp1, ilp2, ol[2], or_[2]; int wipe; } EC;
static struct { svf b[4]; float f, q; } HG;
static float qL[NQ], qR[NQ], qS[NQ];            /* the cloud's block at its rate; what it gives the plate */
static inline float em_at(float d)
{
    int di = (int)d; float fr = d - (float)di; uint32_t i = EC.w - (uint32_t)di;
    return ((float)EM[i & (EN - 1)] * (1 - fr) + (float)EM[(i - 1) & (EN - 1)] * fr) * (1.0f / 16384.0f);
}
static void grain_new(grain *q, const sy_ctl *c, float scatter)
{
    float r = 1.0f, u = rnd();
    if (u < 0.30f * scatter) r *= 0.5f;                                              /* (3.1: now and then an octave under; never over: it shimmered high) */
    if (r > 2) r = 2;
    if (r < 0.25f) r = 0.25f;
    if (rnd() < 0.30f * scatter) r = -r;
    float len = (0.9f - 0.70f * scatter) * (0.6f + 0.8f * rnd()) * SRQ;
    float off = EC.d + (rnd() * (float)(EN - 64) - EC.d) * scatter * scatter + (rnd() - 0.5f) * 0.06f * SRQ;
    float drift = 1.0f - r, lo = 16.0f + (drift < 0 ? -drift * len : 0), hi = (float)(EN - 32) - (drift > 0 ? drift * len : 0);
    if (hi < lo) { len *= 0.25f; lo = 16.0f + (drift < 0 ? -drift * len : 0); hi = (float)(EN - 32) - (drift > 0 ? drift * len : 0); }
    if (off < lo) off = lo;
    if (off > hi) off = hi;
    float p = 0.5f + (rnd() - 0.5f) * (0.3f + 0.7f * scatter);
    q->pos = (float)((EC.w - (uint32_t)off) & (EN - 1)); q->inc = r; q->n = (int)len; if (q->n < 64) q->n = 64;
    q->u = 0; q->du = 1.0f / (float)q->n; q->gl = c_cos(p * C_PI * 0.5f); q->gr = c_sin(p * C_PI * 0.5f);
}
static void cloud_block(const sy_ctl *c)
{
    const float *k = c->hl;
    float dt = 0.25f * c_exp2(4.56f * k[0]) * SRQ, regen = 0.55f + 0.47f * c_sqrt(k[1]), gm = k[2], scatter = clamp01(k[3] + 0.3f * tension), lvl = c->on[SRC_HALL] ? c->lvl[SRC_HALL] : 0, sp = c->space[SRC_HALL], pk = 0, gen = k[1];
    {   /* the room's own resonances: the root's octave, its fifth, the octave above (at this rate, a filter's frequency is set twice over) */
        float f = ev_root; while (f >= 110.0f) f *= 0.5f; while (f < 55.0f) f *= 2; float qq = 2.0f + 14.0f * gen;
        if (c_absf(f - HG.f) > 0.05f || c_absf(qq - HG.q) > 0.05f) { HG.f = f; HG.q = qq; svf_set(&HG.b[0], 2.0f * f, qq); svf_set(&HG.b[1], 3.0f * f, qq); svf_set(&HG.b[2], 4.0f * f, qq); svf_set(&HG.b[3], 6.0f * f, qq); }
    }
    if (dt > (float)(EN - 64)) dt = (float)(EN - 64);
    int ng = gm < 0.01f ? 0 : eco >= 2 ? 2 : eco == 1 ? 4 : EGN; float ga = c_sqrt(1 - gm), gb = c_sqrt(gm);
    if (EC.wipe > 0) {                                      /* AZ-5's cut: the memory is emptied, a part a block */
        int n = EC.wipe > 8192 ? 8192 : EC.wipe;
        for (int i = 0; i < n; i++) EM[(EN - EC.wipe + i) & (EN - 1)] = 0;
        EC.wipe -= n;
        for (int i = 0; i < EGN; i++) EC.g[i].n = 0;
        for (int i = 0; i < NB + 16; i++) EC.eh[i] = 0;
        EC.lp = 0; EC.hp = 0; EC.env = 0; EC.ilp1 = 0; EC.ilp2 = 0; EC.ol[0] = 0; EC.ol[1] = 0; EC.or_[0] = 0; EC.or_[1] = 0;
        for (int m = 0; m < NQ; m++) { qL[m] = 0; qR[m] = 0; qS[m] = 0; }
        lev[SRC_HALL] = 0;
        return;
    }
    for (int n = 0; n < 16; n++) EC.eh[n] = EC.eh[NB + n];
    for (int n = 0; n < NB; n++) EC.eh[16 + n] = cin[n];
    for (int m = 0; m < NQ; m++) {
        const float *h = EC.eh + 8 + 2 * m;
        float x = 0.5f * h[0] + 0.32129f * (h[-1] + h[1]) - 0.09250f * (h[-3] + h[3]) + 0.040605f * (h[-5] + h[5]) - 0.019395f * (h[-7] + h[7]);
        EC.ilp1 += 0.80f * (x - EC.ilp1); EC.ilp2 += 0.80f * (EC.ilp1 - EC.ilp2); x = EC.ilp2;      /* (what the grains read an octave up must not fold) */
        float dd = dt - EC.d; EC.d += dd > 0.5f ? 0.5f : dd < -0.5f ? -0.5f : dd;
        float tl = em_at(EC.d), tr = em_at(EC.d * 0.75f), gl = 0, gr = 0;
        for (int i = 0; i < ng; i++) {
            grain *q = &EC.g[i];
            if (q->n <= 0) grain_new(q, c, scatter);
            int pi = (int)q->pos; float fr = q->pos - (float)pi;
            float sm = (float)EM[pi & (EN - 1)] * (1 - fr) + (float)EM[(pi + 1) & (EN - 1)] * fr;
            float wn = 4.0f * q->u * (1.0f - q->u); wn *= wn * (1.0f / 16384.0f);
            gl += sm * wn * q->gl; gr += sm * wn * q->gr;
            q->u += q->du; q->n--;
            q->pos += q->inc; if (q->pos >= (float)EN) q->pos -= (float)EN; else if (q->pos < 0) q->pos += (float)EN;
        }
        float back = (ga * (0.6f * tl + 0.4f * tr) + gb * 0.70f * (gl + gr)) * regen * EC.agc;
        if (gen > 0.01f) { float rr = 0; for (int i = 0; i < 4; i++) rr += svf_bp(&HG.b[i], back); back += (rr * (1.6f / HG.q) - back) * gen; }      /* (played back into the room) */
        EC.lp += 0.72f * (back - EC.lp); EC.hp += 0.03f * (EC.lp - EC.hp);
        float wr = x + EC.lp - EC.hp, e = c_absf(wr);
        EC.env += (e > EC.env ? 0.05f : 0.0004f) * (e - EC.env);
        EC.agc += 0.01f * ((EC.env > 0.20f ? 0.20f / EC.env : 1.0f) - EC.agc);
        float v = wr * 16384.0f; v = v > 32767.0f ? 32767.0f : v < -32767.0f ? -32767.0f : v;
        EM[EC.w & (EN - 1)] = (int16_t)(v >= 0 ? v + 0.5f : v - 0.5f); EC.w++;
        float ol = (ga * 1.1f * tl + gb * 1.4f * gl) * lvl, orr = (ga * 1.1f * tr + gb * 1.4f * gr) * lvl;
        EC.ol[0] += 0.75f * (ol - EC.ol[0]); EC.ol[1] += 0.75f * (EC.ol[0] - EC.ol[1]); EC.or_[0] += 0.75f * (orr - EC.or_[0]); EC.or_[1] += 0.75f * (EC.or_[0] - EC.or_[1]);
        qL[m] = EC.ol[1]; qR[m] = EC.or_[1]; qS[m] = (qL[m] + qR[m]) * sp * 0.5f;
        if (c_absf(qL[m]) > pk) pk = c_absf(qL[m]);
    }
    pk = clamp01(pk * 4.0f); lev[SRC_HALL] += (pk - lev[SRC_HALL]) * (pk > lev[SRC_HALL] ? 0.5f : 0.06f); if (lev[SRC_HALL] < 1e-6f) lev[SRC_HALL] = 0;
    for (int i = 0; i < 4; i++) { HG.b[i].s1 = flush(HG.b[i].s1); HG.b[i].s2 = flush(HG.b[i].s2); }
    EC.lp = flush(EC.lp); EC.hp = flush(EC.hp); EC.env = flush(EC.env); EC.ilp1 = flush(EC.ilp1); EC.ilp2 = flush(EC.ilp2);
    for (int i = 0; i < 2; i++) { EC.ol[i] = flush(EC.ol[i]); EC.or_[i] = flush(EC.or_[i]); }
}

/* ================================================================ the plate (after Dattorro's, 1997), at a quarter of the rate
 * four all-passes smear what enters; then two halves feed each other (an all-pass whose length turns slowly, a
 * line, a damping, an all-pass, a line): dense at once, smooth for as long as it lasts. */
static struct {
    float id[4][256], a1[512], d1[2048], b1[1024], e1[2048], a2[512], d2[2048], b2[2048], e2[2048];
    int w; float lpA, lpB, bw, ph, rh[NB + 16], set_size; int L[12];
} PL;
static const float PLEN[12] = { 142, 107, 379, 277, 672, 4453, 1800, 3720, 908, 4217, 2656, 3163 };
#define TAP(buf, mask, n) buf[(PL.w - (n)) & (mask)]
static inline float apf(float *buf, int mask, int len, float g, float x) { float d = buf[(PL.w - len) & mask], y = d - g * x; buf[PL.w & mask] = x + g * y; return y; }
static void plate_block(const sy_ctl *c)
{
    float size = c->room_size < 0.3f ? 0.3f : c->room_size > 1.24f ? 1.24f : c->room_size;
    if (size != PL.set_size) { PL.set_size = size; for (int i = 0; i < 12; i++) { PL.L[i] = (int)(PLEN[i] * (SRQ / 29761.0f) * size); if (PL.L[i] < 4) PL.L[i] = 4; } }
    float g = c_pow(10.0f, -0.544f * size / c->room_t60),          /* (four times round each loop of 0.725 s) */
           dd2 = g + 0.15f; if (dd2 > 0.5f) dd2 = 0.5f; if (dd2 < 0.25f) dd2 = 0.25f;
    float damp = 1.0f - c->room_damp * 0.85f, mix = c->room_mix;
    PL.ph += C_TWO_PI * (float)NQ / SRQ * 0.7f; if (PL.ph > C_TWO_PI) PL.ph -= C_TWO_PI;
    float mo = 5.0f + 4.0f * c_sin(PL.ph), mo2 = 5.0f + 4.0f * c_cos(PL.ph);
    int m1 = (int)mo, m2 = (int)mo2; float f1 = mo - (float)m1, f2 = mo2 - (float)m2;
    const int *L = PL.L;
    for (int n = 0; n < 16; n++) PL.rh[n] = PL.rh[NB + n];
    for (int n = 0; n < NB; n++) PL.rh[16 + n] = rin[n];
    for (int m = 0; m < NQ; m++) {
        const float *h = PL.rh + 8 + 2 * m;
        float x = 0.5f * h[0] + 0.32129f * (h[-1] + h[1]) - 0.09250f * (h[-3] + h[3]) + 0.040605f * (h[-5] + h[5]) - 0.019395f * (h[-7] + h[7]) + qS[m] + 1e-18f;
        PL.bw += 0.92f * (x - PL.bw); x = PL.bw;
        x = apf(PL.id[0], 255, L[0], 0.75f, x); x = apf(PL.id[1], 255, L[1], 0.75f, x); x = apf(PL.id[2], 255, L[2], 0.625f, x); x = apf(PL.id[3], 255, L[3], 0.625f, x);
        /* the two halves */
        float fbA = TAP(PL.e2, 2047, L[11]) * g, fbB = TAP(PL.e1, 2047, L[7]) * g;
        {   float in = x + fbA, d = PL.a1[(PL.w - L[4] - m1) & 511]; d += (PL.a1[(PL.w - L[4] - m1 - 1) & 511] - d) * f1;
            float y = d + 0.70f * in; PL.a1[PL.w & 511] = in - 0.70f * y;
            PL.d1[PL.w & 2047] = y; float t = TAP(PL.d1, 2047, L[5]);
            PL.lpA += damp * (t - PL.lpA); t = PL.lpA * g;
            t = apf(PL.b1, 1023, L[6], dd2, t); PL.e1[PL.w & 2047] = t; }
        {   float in = x + fbB, d = PL.a2[(PL.w - L[8] - m2) & 511]; d += (PL.a2[(PL.w - L[8] - m2 - 1) & 511] - d) * f2;
            float y = d + 0.70f * in; PL.a2[PL.w & 511] = in - 0.70f * y;
            PL.d2[PL.w & 2047] = y; float t = TAP(PL.d2, 2047, L[9]);
            PL.lpB += damp * (t - PL.lpB); t = PL.lpB * g;
            t = apf(PL.b2, 2047, L[10], dd2, t); PL.e2[PL.w & 2047] = t; }
        /* what is heard: taps along both halves, left and right from different places */
        float yl = TAP(PL.d2, 2047, L[9] * 266 / 4217) + TAP(PL.d2, 2047, L[9] * 2974 / 4217) - TAP(PL.b2, 2047, L[10] * 1913 / 2656) + TAP(PL.e2, 2047, L[11] * 1996 / 3163)
                 - TAP(PL.d1, 2047, L[5] * 1990 / 4453) - TAP(PL.b1, 1023, L[6] * 187 / 1800) - TAP(PL.e1, 2047, L[7] * 1066 / 3720);
        float yr = TAP(PL.d1, 2047, L[5] * 353 / 4453) + TAP(PL.d1, 2047, L[5] * 3627 / 4453) - TAP(PL.b1, 1023, L[6] * 1228 / 1800) + TAP(PL.e1, 2047, L[7] * 2673 / 3720)
                 - TAP(PL.d2, 2047, L[9] * 2111 / 4217) - TAP(PL.b2, 2047, L[10] * 335 / 2656) - TAP(PL.e2, 2047, L[11] * 121 / 3163);
        PL.w++;
        qL[m] += yl * mix * 0.6f; qR[m] += yr * mix * 0.6f;
    }
    PL.lpA = flush(PL.lpA); PL.lpB = flush(PL.lpB); PL.bw = flush(PL.bw);
}

/* ================================================================ the whole */
static float airL[NQ + 8], airR[NQ + 8], tpL[NB + 8], tpR[NB + 8], upL[NB + 8], upR[NB + 8], pendL[NB], pendR[NB], pend_pk, lim_g = 1, hpl, hpr;
static void cold(void);
void sy_init(uint32_t seed)
{
    rs = seed ? seed : 1u;
    for (int i = 0; i <= 1024; i++) SIN[i] = c_sin(C_TWO_PI * (float)i / 1024.0f);
    {   /* the saw: each width summed from its harmonics (as many as its highest note leaves under the half rate) */
        static const uint16_t LEN[5] = { 512, 256, 256, 128, 128 }, NH[5] = { 64, 32, 16, 8, 4 };
        for (int w = 0; w < 5; w++) for (int i = 0; i < LEN[w]; i++) {
            float x = 0; uint32_t ph = (uint32_t)((uint64_t)i * 4294967296ull / LEN[w]);
            for (int h = 1; h <= NH[w]; h++) x += sin_ph(ph * (uint32_t)h) / (float)h;
            SAWT[MOFF[w] + i] = (int16_t)(x * 0.55f * 32000.0f / 1.0f > 32767.0f ? 32767 : x * 0.55f * 32000.0f < -32767.0f ? -32767 : x * 0.55f * 32000.0f);
        }
    }
    for (int n = 0; n < EN; n++) EM[n] = 0;
    EC.w = 0; cold(); EC.wipe = 0; core_reset(1e-3f, 0.2f);      /* (hot standby: critical, at a thousandth of its power) */
    az_ph = 0; az_t = 0; az_z = 0; cut = 1; trip = 0; out_show = 0; eco = 0;
    for (int n = 0; n < NQ + 8; n++) { airL[n] = 0; airR[n] = 0; }
    for (int n = 0; n < NB + 8; n++) { tpL[n] = 0; tpR[n] = 0; upL[n] = 0; upR[n] = 0; }
    for (int n = 0; n < NB; n++) { pendL[n] = 0; pendR[n] = 0; }
    pend_pk = 0; lim_g = 1; hpl = 0; hpr = 0;
}
static void cold(void)                        /* everything that sounds, silent and cold (the start; AZ-5's cut). The cloud's memory is emptied apart */
{
    uint32_t w = EC.w;
    #define ZERO(x) do { uint32_t *z_ = (uint32_t *)(void *)&(x); for (unsigned i_ = 0; i_ < sizeof(x) / 4; i_++) z_[i_] = 0; } while (0)
    { float ck = GS.clock, bl = GS.b_last, ba = GS.b_auto; ZERO(GS); GS.clock = ck; GS.b_last = bl; GS.b_auto = ba; }
    ZERO(CHN); ZERO(MO); ZERO(EC); ZERO(PL); ZERO(PT); ZERO(LB); ZERO(HG);
    EC.w = w; EC.wipe = EN;
    EC.d = 2.0f * SRQ; EC.agc = 1; PL.set_size = -1;
    PT.hfl = 1; PT.gt = 6.0f; PT.tu = 0.5f; PT.rt = 3.0f;               /* (the hum and the shaft come up by themselves, in a few seconds) */
    tension = 0; heat = 0; bend = 1; wow1 = 0; wow2 = 0;
    for (int l = 0; l < SY_NSRC; l++) lev[l] = 0;
}
void sy_block(const sy_ctl *c, float *l, float *r)
{
    sy_kinds(c);
    /* AZ-5: the rods go in (the reactor: reactor_block); prompt critical under them: the cut, 2.5 s of nothing, all cold */
    if (az_ph == 2) { az_t += DT; if (az_t > 0.06f && az_t < 0.06f + DT * 1.5f) { cold(); core_reset(2e-6f, -0.8f); RX.trip_t = 2.5f; RX.trip = 3; }      /* (after it: cold, from the source's counts) */ if (az_t > 2.5f) { az_ph = 0; az_z = 0; } }
    reactor_block(c);
    /* the heat is the reactor's; the tension, how fast its power moves. Hot, the pitch of everything sags and wavers */
    { float tm = 0; for (int j = 0; j < SY_NZ; j++) if (RX.T[j] > tm) tm = RX.T[j]; heat = clamp01(0.5f * c_sqrt(RX.Pth * 12.0f) + 0.6f * tm); }      /* (the core's heat, and its hottest channel's) */
    { float tg = clamp01(c_absf(RX.dln) * 0.6f); tension += (tg - tension) * rate(tg > tension ? 0.3f : 2.0f); if (tension < 1e-5f) tension = 0; }
    if (trip > 0) trip -= DT / 3.0f;
    wow1 += C_TWO_PI * DT * 0.17f; if (wow1 > C_TWO_PI) wow1 -= C_TWO_PI;
    wow2 += C_TWO_PI * DT * 0.41f; if (wow2 > C_TWO_PI) wow2 -= C_TWO_PI;
    bend = c_exp2(c->horror * heat * (-0.45f * heat + 0.12f * c_sin(wow1) + 0.06f * c_sin(wow2)) / 12.0f);
    for (int n = 0; n < NB; n++) { mixL[n] = 0; mixR[n] = 0; rin[n] = 0; cin[n] = 0; }
    if (c->on[SRC_FUEL]) channels_block(c); else lev[SRC_FUEL] = 0;
    {   /* SHIFT: the channels (and what they give the plate), before the rest joins them */
        float a = c->gest[F_SHIFT];
        if (a > 0.005f) { float d = a * (10.0f + 60.0f * heat * c->horror + 16.0f * tension), dl = KD[F_SHIFT] == 1 ? d : -d, dr = KD[F_SHIFT] == 0 ? -d : d; shift(mixL, 0, dl); shift(mixR, 1, dr * 1.31f); shift(rin, 2, dl * 1.15f); }
    }
    if (c->on[SRC_MOD]) moderator_block(c); else lev[SRC_MOD] = 0;
    plant_block(c);
    gesture_block(c);
    cloud_block(c);
    plate_block(c);
    /* the cloud and the plate, up to this rate */
    for (int n = 0; n < 8; n++) { airL[n] = airL[NQ + n]; airR[n] = airR[NQ + n]; }
    for (int n = 0; n < NQ; n++) { airL[8 + n] = qL[n]; airR[8 + n] = qR[n]; }
    float pk = 0;
    static float outL[NB], outR[NB];
    for (int n = 0; n < NQ; n++) {
        const float *x = airL + n + 4, *y = airR + n + 4;
        float a1 = 0.64258f * (x[0] + x[1]) - 0.18500f * (x[-1] + x[2]) + 0.08121f * (x[-2] + x[3]) - 0.03879f * (x[-3] + x[4]);
        float b1 = 0.64258f * (y[0] + y[1]) - 0.18500f * (y[-1] + y[2]) + 0.08121f * (y[-2] + y[3]) - 0.03879f * (y[-3] + y[4]);
        mixL[2 * n] += x[0]; mixR[2 * n] += y[0]; mixL[2 * n + 1] += a1; mixR[2 * n + 1] += b1;
    }
    float cut_t = az_ph == 2 && az_t > 0 ? 0.0f : 1.0f, dcut = (cut_t - cut) * (cut_t < cut ? 1.0f / (0.035f * SR) * NB : 0.05f) / NB, cutg = cut, sg = 0.62f * (az_ph == 1 ? 1.0f + 0.5f * az_z : 1.0f);
    if (cut_t < cut && cut + dcut * NB < 0) dcut = -cut / NB;
    if (cut_t > cut && cut_t - cut < 1e-4f) { cutg = cut_t; dcut = 0; }
    cut = cutg + dcut * NB;
    for (int n = 0; n < NB; n++) {
        cutg += dcut;
        float a = mixL[n] * sg * cutg, b = mixR[n] * sg * cutg;      /* (room for the channels, the moderator, the loop, a plate and the hall under the ceiling) */
        hpl += 0.0062f * (a - hpl); hpr += 0.0062f * (b - hpr); a -= hpl; b -= hpr;
        outL[n] = a; outR[n] = b; a = c_absf(a); b = c_absf(b); if (a > pk) pk = a; if (b > pk) pk = b;
    }
    hpl = flush(hpl); hpr = flush(hpr);
    /* the limiter, one block ahead, on the true peak; then twice the rate (as CRITICAL 2.2) */
    for (int n = 0; n < 8; n++) { tpL[n] = tpL[NB + n]; tpR[n] = tpR[NB + n]; }
    for (int n = 0; n < NB; n++) { tpL[8 + n] = outL[n]; tpR[8 + n] = outR[n]; }
    for (int n = 0; n < NB; n++) {
        const float *x = tpL + n + 4, *y = tpR + n + 4;
        float a = c_absf(0.64258f * (x[0] + x[1]) - 0.18500f * (x[-1] + x[2]) + 0.08121f * (x[-2] + x[3]) - 0.03879f * (x[-3] + x[4]));
        float b = c_absf(0.64258f * (y[0] + y[1]) - 0.18500f * (y[-1] + y[2]) + 0.08121f * (y[-2] + y[3]) - 0.03879f * (y[-3] + y[4]));
        if (a > pk) pk = a;
        if (b > pk) pk = b;
    }
    float need = pk > 0.89f ? 0.89f / pk : 1.0f, lim_old = pend_pk > 0.89f ? 0.89f / pend_pk : 1.0f, g1;
    if (need < lim_g) g1 = need; else { g1 = lim_g + (need - lim_g) * 0.02f; if (g1 > lim_old) g1 = lim_old; if (g1 < lim_g) g1 = lim_g; }
    float dg = (g1 - lim_g) / (NB - 4);
    for (int n = 0; n < 8; n++) { upL[n] = upL[NB + n]; upR[n] = upR[NB + n]; }
    for (int n = 0; n < NB; n++) { if (n < NB - 4) lim_g += dg; else lim_g = g1; upL[8 + n] = pendL[n] * lim_g; upR[8 + n] = pendR[n] * lim_g; pendL[n] = outL[n]; pendR[n] = outR[n]; }
    for (int n = 0; n < NB; n++) {
        const float *x = upL + n + 4, *y = upR + n + 4;
        l[2 * n] = x[0]; r[2 * n] = y[0];
        l[2 * n + 1] = 0.64258f * (x[0] + x[1]) - 0.18500f * (x[-1] + x[2]) + 0.08121f * (x[-2] + x[3]) - 0.03879f * (x[-3] + x[4]);
        r[2 * n + 1] = 0.64258f * (y[0] + y[1]) - 0.18500f * (y[-1] + y[2]) + 0.08121f * (y[-2] + y[3]) - 0.03879f * (y[-3] + y[4]);
    }
    pend_pk = pk; lim_show = 1 - lim_g;
    { float opk = 0; for (int n = 0; n < NB; n++) { float a = c_absf(upL[8 + n]); if (a > opk) opk = a; } out_show += (opk - out_show) * (opk > out_show ? 0.5f : 0.05f); if (out_show < 1e-6f) out_show = 0; }
}
float sy_plant(int w)
{
    switch (w) {
    case P_HUM: return PT.alive * PT.ha;
    case P_TURBINE: return 1500.0f * c_exp2(PT.tu);
    case P_PUMP: return clamp01(PT.pe * 6.0f);
    case P_GEIGER: return clamp01(PT.krate / 40.0f);
    case P_RELAY: return PT.rshow;
    case P_GROAN: return PT.ge;
    case P_GHOST: return clamp01(PT.qa * 9.0f);
    default: return 0;
    }
}
float sy_level(int s) { return s >= 0 && s < SY_NSRC ? lev[s] : 0; }
float sy_heat(void) { return heat; }
float sy_tension(void) { return tension; }
float sy_limit(void) { return lim_show; }
float sy_hum_hz(void) { float lost = 1.0f - PT.alive; return PT.alive < 0.03f ? 0.0f : 50.0f * (0.92f + 0.16f * gene(F_HUM)) * (1.0f - 0.5f * PT.hsag) * c_exp2(-2.0f * lost * lost); }
