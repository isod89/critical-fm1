/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: the instrument (inst.h). */
#include "inst.h"
#include "../core/cmath.h"

enum { B_OCTD = 0, B_OCTU, B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC };
enum { E_SELECT = 0, E_ALGORITHM, E_K1, E_K2, E_K3, E_K4, E_PRESETS };
static const uint8_t WHITE[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 };
static const uint8_t BLACK[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
#define LED_KEY(k) (14 + (k))

const char *const inst_scale_name[NSCALE] = { "PHRYGIAN", "AEOLIAN", "LOCRIAN", "HARMONIC MINOR", "HUNGARIAN MINOR", "DORIAN" };
static const uint8_t SCALE[NSCALE][7] = { { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 6, 8, 10 },
                                          { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 6, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 10 } };
const char *const inst_note_name[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char *const inst_page_name[NPAGE] = { "REACTOR", "CHANNELS", "MODERATOR", "LOOP", "HALL", "BOARD" };
const char *const inst_sig_name[NSIG] = { "STEPS", "DOOR", "GROAN", "RELAYS", "BLACKOUT", "RISER", "SWELL", "DROP", "BRAAM", "STINGER", "TURBINE TRIP" };

/* ---- the scenes: every knob of the five pages, and the room (the plant drawn, its levels, stay) */
typedef struct { const char *name; float room[4], rx[4], rx2[4], fu[4], md[4], hl[4], lvl[SY_NSRC], space[SY_NSRC], hall[SY_NSRC]; } scene;
static const scene SCENE[] = {
    /* a core at a little power, held there by its regulator: the counts, a bed of its channels, what the hands raise */
    { "HOT STANDBY", { 0.96f, 0.83f, 0.58f, 0.58f }, { 0.75f, 0.35f, 0.35f, 0.60f }, { 0.25f, 0.45f, 0.50f, 0.30f },
      { 0.55f, 0.60f, 0.40f, 0.30f }, { 0.35f, 0.50f, 0.75f, 0.40f }, { 0.78f, 0.45f, 0.50f, 0.40f }, { 0.80f, 0.45f, 0.70f }, { 0.70f, 0.40f, 0.80f }, { 0.60f, 0.40f, 0.0f } },
    /* much xenon, slow, the channels hardly coupled: what is played poisons itself, and the power wanders round the ring */
    { "XENON PIT", { 1.0f, 0.90f, 0.50f, 0.62f }, { 0.80f, 0.25f, 0.85f, 0.45f }, { 0.12f, 0.40f, 0.35f, 0.35f },
      { 0.70f, 0.45f, 0.30f, 0.45f }, { 0.30f, 0.70f, 0.90f, 0.70f }, { 0.85f, 0.70f, 0.60f, 0.50f }, { 0.80f, 0.55f, 0.75f }, { 0.75f, 0.40f, 0.85f }, { 0.70f, 0.50f, 0.0f } },
    /* the steam counts against it: the channels ring and boil, the margin is thin */
    { "BOILING", { 0.85f, 0.75f, 0.45f, 0.50f }, { 0.85f, 0.62f, 0.30f, 0.70f }, { 0.30f, 0.50f, 0.65f, 0.30f },
      { 0.40f, 0.80f, 0.85f, 0.50f }, { 0.55f, 0.80f, 0.70f, 0.30f }, { 0.60f, 0.35f, 0.70f, 0.70f }, { 0.75f, 0.50f, 0.60f }, { 0.60f, 0.35f, 0.70f }, { 0.50f, 0.40f, 0.0f } },
    /* a pool: long rings, the light at its bluest, the hall keeping only its own resonance */
    { "BLUE POOL", { 1.0f, 0.95f, 0.30f, 0.70f }, { 0.70f, 0.20f, 0.40f, 0.50f }, { 0.35f, 0.50f, 0.40f, 0.40f },
      { 0.90f, 0.95f, 0.25f, 0.20f }, { 0.25f, 0.30f, 0.50f, 0.60f }, { 0.90f, 0.85f, 0.40f, 0.30f }, { 0.80f, 0.35f, 0.80f }, { 0.85f, 0.40f, 0.90f }, { 0.80f, 0.40f, 0.0f } },
};
#define NSCENE ((int)(sizeof SCENE / sizeof SCENE[0]))
const char *const inst_scene_name[] = { "HOT STANDBY", "XENON PIT", "BOILING", "BLUE POOL" };
int inst_nscene(void) { return NSCENE; }

static int white_of(int k) { for (int w = 0; w < 16; w++) if (WHITE[w] == k) return w; return -1; }
static int black_of(int k) { for (int b = 0; b < 11; b++) if (BLACK[b] == k) return b; return -1; }
static void scopy(char *d, const char *s, int n) { int i = 0; for (; s[i] && i < n - 1; i++) d[i] = s[i]; d[i] = 0; }
static char *sput(char *p, const char *s) { while (*s) *p++ = *s++; return p; }
void inst_say(inst *g, const char *s) { scopy(g->toast, s, sizeof g->toast); g->toast_until = g->now_ms + 1800; }
static void say2(inst *g, const char *a, const char *b) { char t[32], *p = sput(t, a); if (p - t < 28 && *b) { p = sput(p, " "); } for (; *b && p - t < 31; b++) *p++ = *b; *p = 0; inst_say(g, t); }
static uint32_t mix32(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x ? x : 1u; }
static uint32_t irnd(inst *g) { g->seed ^= g->seed << 13; g->seed ^= g->seed >> 17; g->seed ^= g->seed << 5; return g->seed; }
float inst_key_hz(const inst *g, int w)
{
    int s = SCALE[g->scale][w % 7] + g->root + 12 * (w / 7) + 12 * g->koct;
    return 130.813f * c_exp2((float)s / 12.0f);
}
const char *inst_key_note(const inst *g, int w) { return inst_note_name[(SCALE[g->scale][w % 7] + g->root) % 12]; }
int inst_rod_out(const inst *g, int j) { return g->down[j] || g->latch[j] || g->gen_off[j]; }

static void scene_load(inst *g, int i)
{
    const scene *s = &SCENE[i]; sy_ctl *p = &g->P;
    g->scene = i;
    for (int k = 0; k < 4; k++) { g->room[k] = s->room[k]; p->rx[k] = s->rx[k]; p->rx2[k] = s->rx2[k]; p->fu[k] = s->fu[k]; p->md[k] = s->md[k]; p->hl[k] = s->hl[k]; }
    for (int k = 0; k < SY_NSRC; k++) { p->lvl[k] = s->lvl[k]; p->space[k] = s->space[k]; p->hall[k] = s->hall[k]; }
    if (p->gest[0] == 0) {                     /* (the very first: the plant's levels) */
        static const float PL0[SY_NPLANT] = { 0.8f, 0.8f, 1.0f, 0.6f, 0.7f, 0.9f, 0.6f };
        for (int f = 0; f < SY_NFAM; f++) p->gest[f] = 0.7f;
        p->gest[F_SHIFT] = 0.35f; p->gest[F_GEN] = 0;
        for (int q = 0; q < SY_NPLANT; q++) p->plant[q] = PL0[q];
        p->horror = 0.7f; for (int k = 0; k < SY_NSRC; k++) p->on[k] = 1;
    }
}
void inst_new_plant(inst *g)                  /* one kind of each sound, of the fuel, the shift, the figure of the hand that plays by itself, and its variant; what is locked stays */
{
    for (int f = 0; f < SY_NFAM; f++) if (!g->lock[f]) { g->P.kind[f] = (uint8_t)((irnd(g) >> 8) % sy_nkind[f]); g->P.gene[f] = (uint8_t)(irnd(g) >> 16); }
}
static const int8_t FAM_OF[NSIG] = { F_STEPS, F_DOOR, F_GROAN, F_RELAY, F_BREAKER, F_RISER, F_SWELL, F_DROP, F_BRAAM, F_STINGER, -1 };
/* the board: THE NOTES (five rows), THE PLANT (sixteen: left, what is always there; right, what the black keys strike) */
const int8_t inst_row_fam[NROW] = { F_FUEL, -4, -3, F_SHIFT, F_GEN,
    F_HUM, F_TURBINE, F_PUMP, F_GEIGER, F_RELAY, F_GROAN, -1, -2, F_STEPS, F_DOOR, F_BREAKER, F_RISER, F_SWELL, F_DROP, F_BRAAM, F_STINGER };
const char *inst_row_name(int r) { int f = inst_row_fam[r]; return f == -1 ? "HORROR" : f == -2 ? "GHOST" : f == -3 ? "HALL" : f == -4 ? "MODERATOR" : f == F_BREAKER ? "BLACKOUT" : sy_fam_name[f]; }
int inst_row_variant(int r) { int f = inst_row_fam[r]; return f >= 0 && f != F_SHIFT; }
static float *row_level(inst *g, int r)
{
    static const int8_t PL[SY_NFAM] = { P_HUM, P_TURBINE, P_PUMP, P_GEIGER, P_RELAY, -1, P_GROAN, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int f = inst_row_fam[r];
    if (f == -1) return &g->P.horror;
    if (f == -2) return &g->P.plant[P_GHOST];
    if (f == -3) return &g->P.lvl[SRC_HALL];
    if (f == -4) return &g->P.lvl[SRC_MOD];
    if (f == F_FUEL) return &g->P.lvl[SRC_FUEL];
    return PL[f] >= 0 ? &g->P.plant[PL[f]] : &g->P.gest[f];
}
float inst_row_level(const inst *g, int r) { return *row_level((inst *)g, r); }
int inst_sig_fam(int b) { return FAM_OF[b]; }
static int row_of(int f) { for (int r = 0; r < NROW; r++) if (inst_row_fam[r] == f) return r; return 0; }
static int same(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static void say_kind(inst *g, const char *what, int f, int free)
{
    char t[32], *p = sput(t, what); const char *k = free ? "FREE" : sy_kind_name[f][g->P.kind[f]];
    if (!same(what, k)) { p = sput(p, ": "); for (; *k && p - t < 31; k++) *p++ = *k; }
    *p = 0; inst_say(g, t);
}
static void event(inst *g, int ev) { if (g->ev_n < 6) g->ev_req[g->ev_n++] = (int8_t)ev; }
static void audition(inst *g, int f)          /* what was just chosen is heard (what is struck; what is held or always there is heard anyway) */
{
    if ((uint32_t)(g->now_ms - g->aud_ms) < 500u && g->aud_ms) return;
    g->aud_ms = g->now_ms ? g->now_ms : 1;
    if (f == F_DOOR) event(g, EV_DOOR); else if (f == F_GROAN) event(g, EV_GROAN); else if (f == F_RELAY) event(g, EV_RELAYS); else if (f == F_DROP) event(g, EV_DROP);
    else if (f == F_BRAAM) event(g, EV_BRAAM); else if (f == F_STINGER) event(g, EV_STINGER); else if (f == F_STEPS) { if (g->step_n < 4) g->step_req[g->step_n++] = 0.8f; }
}
/* a kind by hand. Free: a turn to the right takes the one drawn, as it is, and locks it. Locked: right, left: the next,
 * the one before; left of the first: FREE again (the draws take it back) */
static void pick_kind(inst *g, int f, int d)
{
    int n = sy_nkind[f], k = g->P.kind[f]; const char *nm = f == F_BREAKER ? "BLACKOUT" : sy_fam_name[f];
    if (!g->lock[f]) { if (d < 0) { say_kind(g, nm, f, 1); return; } g->lock[f] = 1; }
    else { k += d > 0 ? 1 : -1; if (k >= n) k = n - 1; if (k < 0) { g->lock[f] = 0; say_kind(g, nm, f, 1); return; } g->P.kind[f] = (uint8_t)k; g->aud_ms = 0; }
    say_kind(g, nm, f, 0); audition(g, f);
}
#define SAVE_VER 0x40
int inst_save(const inst *g, uint8_t *b)
{
    uint8_t *p = b; const uint8_t *q = (const uint8_t *)&g->P;
    *p++ = SAVE_VER; *p++ = (uint8_t)sizeof g->P; *p++ = (uint8_t)(sizeof g->P >> 8); *p++ = SY_NFAM;
    for (unsigned i = 0; i < sizeof g->P; i++) *p++ = q[i];
    q = (const uint8_t *)g->room; for (unsigned i = 0; i < sizeof g->room; i++) *p++ = q[i];
    *p++ = (uint8_t)g->scene; *p++ = (uint8_t)g->root; *p++ = (uint8_t)g->scale; *p++ = (uint8_t)(g->koct + 8); *p++ = (uint8_t)g->sel;
    for (int f = 0; f < SY_NFAM; f++) *p++ = g->lock[f];
    return (int)(p - b);
}
int inst_load(inst *g, const uint8_t *b, int n)
{
    if (n != (int)(4 + sizeof g->P + sizeof g->room + 5 + SY_NFAM) || b[0] != SAVE_VER || (b[1] | b[2] << 8) != (int)sizeof g->P || b[3] != SY_NFAM) return 0;
    const uint8_t *p = b + 4; uint8_t *q = (uint8_t *)&g->P;
    for (unsigned i = 0; i < sizeof g->P; i++) q[i] = *p++;
    q = (uint8_t *)g->room; for (unsigned i = 0; i < sizeof g->room; i++) q[i] = *p++;
    g->scene = p[0] < NSCENE ? p[0] : 0; g->root = p[1] % 12; g->scale = p[2] % NSCALE; g->koct = (int)p[3] - 8; g->sel = p[4] < NPAGE ? p[4] : PG_REACTOR; p += 5;
    if (g->koct < -2 || g->koct > 1) g->koct = -1;
    for (int f = 0; f < SY_NFAM; f++) { g->lock[f] = p[f] != 0; if (g->P.kind[f] >= sy_nkind[f]) g->P.kind[f] = 0; }
    for (int j = 0; j < SY_NZ; j++) g->P.rod[j] = 0;
    g->P.blackout = 0; g->P.riser = 0; g->P.swell = 0;
    inst_new_plant(g);
    return 1;
}
void inst_new(inst *g, uint32_t seed)
{
    uint8_t *z = (uint8_t *)g; for (unsigned i = 0; i < sizeof *g; i++) z[i] = 0;
    g->seed = mix32(seed);
    g->root = 2; g->scale = 0; g->koct = -1; g->sel = PG_REACTOR;
    g->bk_held = -1; g->gkind = 255;
    scene_load(g, 0);
    inst_new_plant(g);
}
/* ---- the knobs: what the four are, on this page, with EDIT or without */
static const char *const KN[NPAGE][4] = { { "BANK", "VOID", "XENON", "TEMPO" }, { "RING", "CHERENKOV", "NOISE", "SPREAD" }, { "TONE", "FAULT", "DEPTH", "DECAY" },
    { "HUM", "TURBINE", "PUMPS", "GEIGER" }, { "TIME", "GENERATIONS", "GRAINS", "SCATTER" }, { "SOUND", "KIND", "VARIANT", "LEVEL" } };
static const char *const KE[NPAGE][4] = { { "COUPLING", "AR TARGET", "AR SPEED", "SOURCE" }, { "LEVEL", "SPACE", "HALL", "" }, { "LEVEL", "SPACE", "HALL", "" },
    { "RELAYS", "GROANS", "GHOST", "HORROR" }, { "SIZE", "DECAY", "DAMP", "MIX" }, { "SIZE", "DECAY", "DAMP", "MIX" } };
static float *knob_ptr(inst *g, int i)        /* 0: none there (the board's first three: asked apart) */
{
    sy_ctl *p = &g->P; int s = g->sel, e = g->mixer;
    switch (s) {
    case PG_REACTOR: return e ? &p->rx2[i] : &p->rx[i];
    case PG_FUEL: return e ? (i == 0 ? &p->lvl[SRC_FUEL] : i == 1 ? &p->space[SRC_FUEL] : i == 2 ? &p->hall[SRC_FUEL] : 0) : &p->fu[i];
    case PG_MOD: return e ? (i == 0 ? &p->lvl[SRC_MOD] : i == 1 ? &p->space[SRC_MOD] : i == 2 ? &p->hall[SRC_MOD] : 0) : &p->md[i];
    case PG_LOOP: { static const int8_t PI[4] = { P_HUM, P_TURBINE, P_PUMP, P_GEIGER }, PE[3] = { P_RELAY, P_GROAN, P_GHOST }; return e ? (i == 3 ? &p->horror : &p->plant[PE[i]]) : &p->plant[PI[i]]; }
    case PG_HALL: return e ? &g->room[i] : &p->hl[i];
    default: return e ? &g->room[i] : i == 3 ? row_level(g, g->cur) : 0;
    }
}
const char *inst_knob_name(const inst *g, int i) { return g->mixer ? KE[g->sel][i] : KN[g->sel][i]; }
float inst_knob_value(const inst *g, int i) { const float *v = knob_ptr((inst *)g, i); return v ? *v : -1.0f; }

/* ---- the rods */
void inst_button(inst *g, int b, int down)
{
    if (b == B_EDIT) { g->mixer = (uint8_t)down; if (down) { static const char *const W[NPAGE] = { "THE REGULATOR", "THE CHANNELS' MIX", "THE MODERATOR'S MIX", "THE LOOP, MORE", "THE ROOM", "THE ROOM" }; inst_say(g, W[g->sel]); } return; }
    if (b == B_PLAY) {                        /* let go: start / stop; held 1 s: a new plant (inst_tick) */
        if (down) { g->play_held = 1; g->play_done = 0; g->play_down = g->now_ms; } else { g->play_held = 0; if (!g->play_done) g->run = !g->run; }
        if (g->run && !g->started) { g->started = 1; g->seed = mix32(g->seed ^ (g->now_ms + 1u) * 2654435761u); inst_new_plant(g); }   /* (the first start: the moment the hand chose draws the plant) */
        return;
    }
    if (b == B_SAVE) {                        /* held 1 s: everything is kept (inst_tick) */
        if (down) { g->save_held = 1; g->save_done = 0; g->save_down = g->now_ms; }
        else { g->save_held = 0; if (!g->save_done) inst_say(g, "HOLD SAVE 1 S: KEEP IT ALL"); }
        return;
    }
    if (b == B_REC) { if (down) { g->rec_held = 1; g->rec_done = 0; g->rec_down = g->now_ms; } else g->rec_held = 0; return; }
    if (!down) return;
    switch (b) {
    case B_HOME:                              /* the board, where it was left; again: its other page, the rods' wall, and round */
        if (g->sel != PG_BOARD) { g->sel = PG_BOARD; g->page = 0; }
        else if (g->page) { g->page = 0; g->cur = g->cur_n; }
        else if (g->cur < NROW_NOTES) { g->cur_n = g->cur; g->cur = g->cur_p >= NROW_NOTES ? g->cur_p : NROW_NOTES; }
        else { g->cur_p = g->cur; g->page = 1; }
        inst_say(g, g->page ? "THE RODS" : g->cur < NROW_NOTES ? "THE NOTES" : "THE PLANT");
        break;
    case B_FX: case B_SEL: case B_ENV: case B_LFO: case B_GLO: {
        static const int8_t SRC[5] = { -1, SRC_FUEL, SRC_MOD, -1, SRC_HALL };
        int pg = b == B_GLO ? PG_HALL : b - B_FX, s = SRC[pg]; g->page = 0;
        if (g->sel == pg && s >= 0) { g->P.on[s] ^= 1; say2(g, inst_page_name[pg], g->P.on[s] ? "ON" : "OFF"); }      /* the one chosen, again: off, or on */
        else { g->sel = pg; say2(g, inst_page_name[pg], s >= 0 && !g->P.on[s] ? "(OFF)" : ""); }
        break;
    }
    case B_ARP: g->hold ^= 1; if (!g->hold) for (int j = 0; j < SY_NZ; j++) g->latch[j] = 0;
        inst_say(g, g->hold ? "HOLD: RODS STAY OUT" : "HOLD OFF"); break;
    case B_SEQ:                                /* the hand that plays by itself: off, or back to where it was (never set: half) */
        if (g->P.gest[F_GEN] > 0.01f) { g->gen_keep = g->P.gest[F_GEN]; g->P.gest[F_GEN] = 0; inst_say(g, "GEN OFF"); }
        else { g->P.gest[F_GEN] = g->gen_keep > 0.01f ? g->gen_keep : 0.5f; say_kind(g, "GEN", F_GEN, 0); }
        break;
    case B_OCTD: case B_OCTU: {
        int o = g->koct + (b == B_OCTU ? 1 : -1); if (o < -2) o = -2; if (o > 1) o = 1; g->koct = o;
        char t[24], *p = sput(t, "KEYS OCTAVE "); if (o < 0) { *p++ = '-'; o = -o; } else *p++ = '+'; *p++ = (char)('0' + o); *p = 0; inst_say(g, t); break;
    }
    default: break;
    }
}
void inst_key(inst *g, int k, int down)
{
    int w = white_of(k);
    if (w >= 0) {
        if (down) { if (g->hold && g->latch[w]) { g->latch[w] = 0; g->down[w] = 0; g->gen_off[w] = 0; return; } g->down[w] = 1; if (g->hold) g->latch[w] = 1; }
        else g->down[w] = 0;
        return;
    }
    int b = black_of(k); if (b < 0 || b >= NSIG) return;
    if (down) { g->bk_held = (int8_t)b; g->key_last = (int8_t)b; g->key_ms = g->now_ms ? g->now_ms : 1; if (FAM_OF[b] >= 0) g->cur = row_of(FAM_OF[b]); } else if (g->bk_held == b) g->bk_held = -1;
    if (b == SIG_RISER) g->P.riser = (uint8_t)down;          /* (held: it lasts as long as the key) */
    if (b == SIG_SWELL) g->P.swell = (uint8_t)down;
    if (!down) return;
    g->sig_ms[b] = g->now_ms; if (FAM_OF[b] >= 0) say_kind(g, inst_sig_name[b], FAM_OF[b], 0); else inst_say(g, inst_sig_name[b]);
    switch (b) {
    case SIG_STEPS: g->walking = 1; g->step_i = 0; g->step_ms = g->now_ms + 300; break;
    case SIG_DOOR: event(g, EV_DOOR); break;
    case SIG_GROAN: event(g, EV_GROAN); break;
    case SIG_RELAYS: event(g, EV_RELAYS); break;
    case SIG_BLACKOUT: g->P.blackout ^= 1; inst_say(g, g->P.blackout ? "BLACKOUT" : "POWER RESTORED"); break;
    case SIG_DROP: event(g, EV_DROP); break;
    case SIG_BRAAM: event(g, EV_BRAAM); break;
    case SIG_STINGER: event(g, EV_STINGER); break;
    case SIG_TRIP: event(g, EV_TRIP); break;
    default: break;
    }
}
void inst_enc(inst *g, int e, int d)
{
    if (e >= E_K1 && e <= E_K4) {
        int i = e - E_K1;
        if (!g->mixer && g->sel == PG_BOARD && i < 3) {            /* the board: which row; its kind; its variant (its level: the knob as any) */
            int f = inst_row_fam[g->cur];
            if (i == 0) { g->cur += d > 0 ? 1 : -1; if (g->cur < 0) g->cur = 0; if (g->cur >= NROW) g->cur = NROW - 1; }
            else if (f < 0) return;
            else if (i == 1) pick_kind(g, f, d);
            else if (!inst_row_variant(g->cur)) return;
            else { int v = (int)g->P.gene[f] + d * 3; g->P.gene[f] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); g->lock[f] = 1; audition(g, f); }
        } else {
            float *v = knob_ptr(g, i); if (!v) return;
            *v += (float)d * 0.01f; if (*v < 0) *v = 0; if (*v > 1) *v = 1;
        }
        g->knob_i = i; g->knob_ms = g->now_ms;
    } else if (e == E_SELECT) {
        d = d > 0 ? 1 : -1; g->root = (g->root + d + 12) % 12; say2(g, "KEY", inst_note_name[g->root]);
    } else if (e == E_ALGORITHM) {
        d = d > 0 ? 1 : -1; g->scale = (g->scale + d + NSCALE) % NSCALE; inst_say(g, inst_scale_name[g->scale]);
    } else if (e == E_PRESETS && g->bk_held >= 0 && FAM_OF[g->bk_held] >= 0) {      /* a black key held: which kind of its sound (and it is heard) */
        pick_kind(g, FAM_OF[g->bk_held], d); g->key_ms = g->now_ms ? g->now_ms : 1;
    } else if (e == E_PRESETS) {
        d = d > 0 ? 1 : -1; scene_load(g, (g->scene + d + NSCENE) % NSCENE); say2(g, "SCENE", SCENE[g->scene].name);
    }
}
/* ---- the hand that plays by itself (3.5; 4: it withdraws rods). Its figures are what horror films do with notes, none
 * of them a film's own tune: STALKER two notes a semitone apart, closing in; DESCENT a slow line that keeps falling;
 * LULLABY a nursery cell repeated, its last note wrong; DIES IRAE the 13th-century chant of the day of wrath (the one
 * figure quoted: it is the oldest of them, and anyone's); TRITONE the root and its tritone; CLUSTER a step against
 * each channel the hands hold; PULSE one note in five, faster as it heats. The notes fall on the channels nearest
 * them (the scale's). Its amount (0..1): how little silence between the figures; past 60 % the lowest channel is
 * held out too, long. */
#define GEN_REST 99
static int key_near(const inst *g, int semis)  /* the channel nearest that note (semitones from the root, in the keys' octave) */
{
    float t = 130.813f * c_exp2((float)(g->root + semis + 12 * g->koct) / 12.0f); int b = 0; float bd = 1e9f;
    for (int w = 0; w < SY_NZ; w++) { float d = c_absf(c_log2(inst_key_hz(g, w) / t)); if (d < bd) { bd = d; b = w; } }
    return b;
}
static void gen_rod(inst *g, int w, uint32_t dur) { if (w >= 0 && w < SY_NZ && !g->down[w]) { g->gen_off[w] = g->now_ms + dur; if (!g->gen_off[w]) g->gen_off[w] = 1; } }
static void gen_phrase(inst *g)               /* a figure: its notes (semitones from the root; GEN_REST), their lengths in ticks */
{
    static const int8_t DIES[8] = { 12, 11, 12, 9, 11, 7, 9, 9 };
    int k = g->P.kind[F_GEN], n = 0; int8_t *nt = g->gnote; uint8_t *du = g->gdur; const uint8_t *sc = SCALE[g->scale];
    g->gacc = 0; g->glen = 0.9f;
    switch (k) {
    default: { int r = 2 + (int)(irnd(g) % 5u); for (int i = 0; i < 2 * r; i++) { nt[n] = (int8_t)(12 + (i & 1)); du[n++] = 4; } g->gtick = 350; g->gacc = 2.5f; g->glen = 0.8f; break; }
    case 1: { int d = 14 + (int)(irnd(g) % 2u), cnt = 6 + (int)(irnd(g) % 5u); for (int i = 0; i < cnt; i++, d--) { nt[n] = (int8_t)(sc[d % 7] + 12 * (d / 7)); du[n++] = 3; } g->gtick = 1100; g->glen = 1.15f; break; }
    case 2: { int c[3], d = 7 + 2 * (int)(irnd(g) % 3u);
        for (int i = 0; i < 3; i++) { c[i] = d; d += (int)(irnd(g) % 5u) - 2; if (d < 5) d = 5; if (d > 13) d = 13; }
        for (int rp = 0; rp < 3; rp++) for (int i = 0; i < 3; i++) { int q = c[i] + (rp == 2 && i == 2 ? ((irnd(g) & 1u) ? 1 : -1) : 0); nt[n] = (int8_t)(sc[q % 7] + 12 * (q / 7)); du[n++] = i == 2 ? 4 : 2; }
        nt[n] = (int8_t)(sc[c[0] % 7] + 12 * (c[0] / 7)); du[n++] = 8; g->gtick = 240; g->gacc = -0.35f; g->glen = 0.6f; break; }
    case 3: { int cnt = (irnd(g) & 3u) ? 8 : 4; for (int i = 0; i < cnt; i++) { nt[n] = DIES[i]; du[n++] = i == cnt - 1 ? 8 : 4; } g->gtick = 450; g->glen = 0.95f; break; }
    case 4: { int rg = (irnd(g) & 1u) ? 12 : 0; nt[0] = (int8_t)rg; du[0] = 8; nt[1] = (int8_t)(rg + 6); du[1] = 8; nt[2] = GEN_REST; du[2] = 6; nt[3] = (int8_t)((irnd(g) & 1u) ? rg + 12 : rg + 1); du[3] = 6; n = 4; g->gtick = 380; break; }
    case 5: nt[0] = 0; du[0] = (uint8_t)(8 + irnd(g) % 7u); n = 1; g->gtick = 400; g->glen = 0.85f; break;
    case 6: { int r = 3 + (int)(irnd(g) % 4u); for (int i = 0; i < 4 * r; i++) { nt[n] = 7; du[n++] = (i & 3) == 0 ? 2 : 1; } g->gtick = 190; g->gacc = 0.8f; g->glen = 0.5f; break; }
    }
    g->gn = (uint8_t)n; g->gi = 0;
}
static void gen_tick(inst *g)
{
    float amt = g->P.gest[F_GEN]; uint32_t ms = g->now_ms;
    for (int w = 0; w < SY_NZ; w++) if (g->gen_off[w] && ((int32_t)(ms - g->gen_off[w]) >= 0 || amt < 0.01f || !g->run)) g->gen_off[w] = 0;
    if (amt < 0.01f || !g->run) { g->gn = 0; g->gnext = ms + 600u; return; }
    if (g->P.kind[F_GEN] != g->gkind) { g->gkind = g->P.kind[F_GEN]; g->gn = 0; g->gnext = ms + 300u; }      /* (another figure: from its start) */
    if (amt > 0.6f && g->gkind != 5 && (int32_t)(ms - g->gped) >= 0) { gen_rod(g, 0, 9000u + irnd(g) % 9000u); g->gped = ms + 14000u + irnd(g) % 12000u; }
    if ((int32_t)(ms - g->gnext) < 0) return;
    if (g->gi >= g->gn) {                      /* the figure is over (or none yet): a silence, shorter as the amount rises, then another */
        int had = g->gn != 0; gen_phrase(g);
        if (had) { g->gnext = ms + (uint32_t)((1500.0f + (float)(irnd(g) % 7000u)) * (1.3f - amt) * (amt < 0.25f ? 2.0f : 1.0f)); return; }
    }
    float tick = (float)g->gtick / (0.6f + 0.9f * (float)g->P.gene[F_GEN] * (1.0f / 255.0f)) / (1.0f + g->gacc * g->heat); if (tick < 60.0f) tick = 60.0f;
    int i = g->gi++, k = g->gkind; uint32_t d = (uint32_t)(tick * (float)g->gdur[i]);
    if (g->gnote[i] != GEN_REST) {
        int w;
        if (k == 5) {                          /* CLUSTER: a step against a channel the hands hold (none: against the root) */
            int cand[SY_NZ], nc = 0; for (int j = 0; j < SY_NZ; j++) if (g->down[j] || g->latch[j]) cand[nc++] = j;
            w = (nc ? cand[irnd(g) % (uint32_t)nc] : 7) + ((irnd(g) & 1u) ? 1 : -1); if (w < 0) w = 1; if (w >= SY_NZ) w = SY_NZ - 2;
        } else {
            int sm = g->gnote[i]; if (k == 2 && (float)(irnd(g) % 100u) < 25.0f * g->heat) sm += (irnd(g) & 1u) ? 1 : -1;      /* (the lullaby, hot: wrong notes) */
            w = key_near(g, sm);
        }
        gen_rod(g, w, (uint32_t)((float)d * g->glen));
    }
    g->gnext = ms + d;
}
void inst_tick(inst *g, uint32_t ms)
{
    uint32_t dt = ms - g->now_ms; if (dt > 200u) dt = 200u;
    g->now_ms = ms;
    gen_tick(g);
    if (g->rec_held && !g->rec_done && ms - g->rec_down > 1000u) { g->rec_done = 1; g->az5_req = 1; inst_say(g, "AZ-5"); }
    if (g->play_held && !g->play_done && ms - g->play_down > 900u) { g->play_done = 1; inst_new_plant(g); g->run = 1; inst_say(g, "A NEW PLANT"); }
    if (g->save_held && !g->save_done && ms - g->save_down > 1000u) { g->save_done = 1; g->save_req = 1; inst_say(g, "SAVING"); }
    if (g->az == 2) { for (int j = 0; j < SY_NZ; j++) { g->latch[j] = 0; g->gen_off[j] = 0; } g->P.blackout = 0; g->P.riser = 0; g->P.swell = 0; g->walking = 0; g->walk = 0; }       /* the cut: nothing is held any more */
    /* the steps: twelve, from far to right here, slower as they arrive */
    if (g->walking && (int32_t)(ms - g->step_ms) >= 0) {
        float u = (float)g->step_i / 11.0f;
        if (g->step_n < 4) g->step_req[g->step_n++] = u;
        g->walk = 0.15f + 0.85f * u;
        g->step_ms = ms + 1150u + irnd(g) % 250u + (uint32_t)(900.0f * u * u * u);
        if (++g->step_i >= 12) g->walking = 0;
    } else if (!g->walking && g->walk > 0) { g->walk -= (float)dt / 6000.0f; if (g->walk < 0) g->walk = 0; }
    if (ms - g->hist_ms >= 250u) {             /* the recorder: the power (seven decades and one over), the reactivity, the heat, the xenon */
        g->hist_ms = ms;
        float lp = (c_log2(g->rx.power > 1e-9f ? g->rx.power : 1e-9f) * 0.30103f + 7.0f) / 8.0f, xe = 0;
        for (int j = 0; j < SY_NZ; j++) xe += g->rx.xe[j];
        float v[NPEN] = { lp, (g->rx.rho + 1.5f) / 2.5f, g->heat, xe / (0.9f * SY_NZ) };
        for (int e = 0; e < NPEN; e++) { float x = v[e] < 0 ? 0 : v[e] > 1 ? 1 : v[e]; g->hist[e][g->hist_i] = (uint8_t)(g->run ? x * 255.0f : 0); }
        g->hist_i = (g->hist_i + 1) % NHIST;
    }
}
void inst_ctl(inst *g, sy_ctl *c)
{
    *c = g->P;
    for (int j = 0; j < SY_NZ; j++) { c->zone_hz[j] = inst_key_hz(g, j); c->rod[j] = (uint8_t)inst_rod_out(g, j); }
    c->root_hz = 130.813f * c_exp2((float)g->root / 12.0f);
    c->room_size = 0.3f + 0.94f * g->room[0]; c->room_t60 = c_exp2(3.6f * g->room[1]); c->room_damp = g->room[2]; c->room_mix = g->room[3];
    sy_kinds(c);
    for (int i = 0; i < g->ev_n; i++) sy_event(g->ev_req[i]);
    g->ev_n = 0;
    for (int i = 0; i < g->step_n; i++) sy_step(g->step_req[i]);
    g->step_n = 0;
    if (g->az5_req) { sy_az5(); g->az5_req = 0; }
}
void inst_meters(inst *g)
{
    g->rx = *sy_reactor();
    for (int s = 0; s < SY_NSRC; s++) g->level[s] = sy_level(s);
    for (int p = 0; p < SY_NPLANT; p++) g->plant[p] = sy_plant(p);
    g->heat = sy_heat(); g->tension = sy_tension(); g->hum_hz = sy_hum_hz(); g->limit = sy_limit(); g->out = sy_out_level(); g->az = sy_az5_phase();
}
uint64_t inst_leds(const inst *g)
{
    uint64_t m = 0;
    if (g->run) m |= 1ull << B_PLAY;
    if (g->rx.ar) m |= 1ull << B_FX;
    if (g->P.on[SRC_FUEL]) m |= 1ull << B_SEL;
    if (g->P.on[SRC_MOD]) m |= 1ull << B_ENV;
    if (!g->P.blackout) m |= 1ull << B_LFO;
    if (g->P.on[SRC_HALL]) m |= 1ull << B_GLO;
    if (g->sel == PG_BOARD) m |= 1ull << B_HOME;
    if (g->save_held) m |= 1ull << B_SAVE;
    if (g->P.gest[F_GEN] > 0.01f) m |= 1ull << B_SEQ;
    if (g->hold) m |= 1ull << B_ARP;
    if (g->mixer) m |= 1ull << B_EDIT;
    if (g->koct < -1) m |= 1ull << B_OCTD;
    if (g->koct > -1) m |= 1ull << B_OCTU;
    if (g->rec_held || g->az || g->rx.trip) m |= 1ull << B_REC;
    for (int j = 0; j < SY_NZ; j++) if (inst_rod_out(g, j)) m |= 1ull << LED_KEY(WHITE[j]);
    if (g->P.riser) m |= 1ull << LED_KEY(BLACK[SIG_RISER]);
    if (g->P.swell) m |= 1ull << LED_KEY(BLACK[SIG_SWELL]);
    if (g->P.blackout) m |= 1ull << LED_KEY(BLACK[SIG_BLACKOUT]);
    if (g->walk > 0.01f) m |= 1ull << LED_KEY(BLACK[SIG_STEPS]);
    return m;
}
