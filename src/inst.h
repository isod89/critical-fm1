/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: the instrument. The sound is a reactor's (synth.h); the hands move its rods.
 *   white keys       the sixteen fuel channels, each tuned to its key's note in the key and the scale. A key held:
 *                    that channel's rod withdrawn (its power rises, as fast as its period lets it; let go, it falls
 *                    back, slowly: the delayed neutrons)
 *   FX SEL ENV LFO GLO   the pages: REACTOR (BANK, VOID, XENON, TEMPO; EDIT: COUPLING, AR TARGET, AR SPEED,
 *                    SOURCE), CHANNELS (RING, CHERENKOV, NOISE, SPREAD; EDIT: LEVEL, SPACE, HALL), MODERATOR (TONE,
 *                    FAULT, DEPTH, DECAY; EDIT: LEVEL, SPACE, HALL), LOOP (HUM, TURBINE, PUMPS, GEIGER; EDIT: RELAYS,
 *                    GROANS, GHOST, HORROR), HALL (TIME, GENERATIONS, GRAINS, SCATTER; EDIT: the room). Pressed
 *                    again: CHANNELS, MODERATOR, HALL off, or on
 *   HOME             the board of all that is drawn, in two pages (THE NOTES: the fuel, the moderator, the hall,
 *                    the shift, the hand that plays by itself; THE PLANT); again: the rods' wall, and back. KNOB 1
 *                    which row, KNOB 2 its kind, KNOB 3 its variant, KNOB 4 its level. A black key: its row
 *   OCT- OCT+        the keyboard's octave
 *   ARP              HOLD: a rod stays out when its key is let go (the key again: back in)
 *   SEQ              the hand that plays by itself (GEN): off, or back to its amount
 *   black keys       by their groups: STEPS, DOOR, GROAN | RELAYS, BLACKOUT (on / off) | RISER (held), SWELL (held),
 *                    DROP | BRAAM, STINGER | TRIP (the turbine's). One held + PRESETS: its kind
 *   PRESETS          the scene: every knob of the five pages, and the room
 *   SELECT the key's root, ALGORITHM the scale
 *   PLAY start / stop, held 1 s: a new plant (a new core's loading); SAVE held 1 s: all of it kept in the FM-1;
 *   REC held 1 s: AZ-5 (every rod in, graphite tips first)
 * The plant is drawn at the first start, and again when PLAY is held: one kind of each sound and its variant. A kind
 * taken by hand is locked: the draws leave it; turned left of its first kind: FREE again. */
#ifndef INST_H
#define INST_H
#include <stdint.h>
#include "synth.h"

#define NSCALE 6
#define NHIST 112
#define NPEN 4                                  /* the recorder's pens: the power (log), the reactivity, the heat, the xenon */
enum { PG_REACTOR, PG_FUEL, PG_MOD, PG_LOOP, PG_HALL, PG_BOARD, NPAGE };
#define NROW 21
#define NROW_NOTES 5                            /* the board's first page: THE NOTES; the rest: THE PLANT */
extern const char *const inst_sig_name[];
enum { SIG_STEPS, SIG_DOOR, SIG_GROAN, SIG_RELAYS, SIG_BLACKOUT, SIG_RISER, SIG_SWELL, SIG_DROP, SIG_BRAAM, SIG_STINGER, SIG_TRIP, NSIG };
typedef struct {
    sy_ctl P;                                 /* the sound's values, as the scene gave them and the knobs moved them */
    float room[4];                            /* the plate, 0..1: SIZE, DECAY, DAMP, MIX */
    int sel, scene, root, scale, koct, run, page;
    uint8_t hold, mixer;
    uint8_t down[SY_NZ], latch[SY_NZ];        /* a key held; a rod left out by HOLD */
    uint32_t gen_off[SY_NZ];                  /* a rod the hand that plays by itself holds out, until then (0: none) */
    uint8_t az5_req;
    int8_t ev_req[6]; uint8_t ev_n;           /* events struck, for the sound */
    float step_req[4]; uint8_t step_n;
    int walking, step_i; uint32_t step_ms; float walk;
    uint32_t now_ms, splash_until, toast_until, rec_down, hist_ms, sig_ms[NSIG], knob_ms; int knob_i;
    int rec_held, rec_done;
    char toast[32];
    uint8_t hist[NPEN][NHIST]; int hist_i;     /* the recorder's paper, four a second */
    /* from the sound, for the panel */
    sy_rx rx; float level[SY_NSRC], heat, tension, plant[SY_NPLANT], hum_hz, limit, out;
    int az;
    uint32_t seed;
    float load_pct, load_peak;
    uint8_t started, save_req, play_held, play_done, save_held, save_done, lock[SY_NFAM]; int8_t bk_held, key_last; uint32_t play_down, save_down, key_ms, aud_ms; int cur, cur_n, cur_p;
    /* the hand that plays by itself */
    int8_t gnote[24]; uint8_t gdur[24], gn, gi, gkind, gflip; uint16_t gtick; uint32_t gnext, gped; float gacc, glen, gen_keep;
} inst;

extern const char *const inst_scale_name[NSCALE], *const inst_note_name[12], *const inst_scene_name[], *const inst_page_name[NPAGE];
extern const int8_t inst_row_fam[NROW];             /* the board's rows: a family of sounds (synth.h), or -1 HORROR, -2 GHOST, -3 HALL, -4 MODERATOR */
int inst_nscene(void);
void inst_new(inst *g, uint32_t seed);
void inst_button(inst *g, int b, int down);
void inst_key(inst *g, int k, int down);      /* k 0..26 (F3..G5) */
void inst_enc(inst *g, int e, int d);
void inst_tick(inst *g, uint32_t ms);         /* the panel's pace: holds, toasts, the steps, the recorder, the hand that plays by itself */
void inst_ctl(inst *g, sy_ctl *c);            /* what the sound needs now; strikes what was asked (call once a block) */
void inst_meters(inst *g);                    /* what the sound says, for the panel (after a block) */
uint64_t inst_leds(const inst *g);
void inst_say(inst *g, const char *s);
int inst_rod_out(const inst *g, int j);       /* is that channel's rod asked out (a key, HOLD, the hand that plays by itself) */
const char *inst_row_name(int r);
int inst_row_variant(int r);                  /* has that row a variant */
int inst_sig_fam(int b);                      /* a black key's family of sounds, or -1 (the trip) */
float inst_row_level(const inst *g, int r);
int inst_save(const inst *g, uint8_t *b);     /* what is kept (the sound's values, the key, the locks): its length, under 1 KB */
int inst_load(inst *g, const uint8_t *b, int n);    /* 1: restored (what is not locked is drawn again) */
void inst_new_plant(inst *g);
const char *inst_knob_name(const inst *g, int i);   /* the four knobs as they are now (the page, EDIT) */
float inst_knob_value(const inst *g, int i);        /* 0..1; -1: no knob there */
const char *inst_key_note(const inst *g, int w);    /* the white key's note name */
float inst_key_hz(const inst *g, int w);
#endif
