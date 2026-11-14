/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4: a synthesizer powered by a reactor. The sound is not played: it is produced by a model of a core, the
 * way a power station produces its current, and the hands only move rods.
 *
 * THE CORE  sixteen fuel channels in a ring (an annular core), one a white key, each tuned to its key's note. Each
 *   channel is a point reactor of its own (six groups of delayed neutrons, U-235's: Keepin's data; the prompt jump
 *   approximation), coupled to its two neighbours by the neutrons that leak between them (the ring is laid out in
 *   fourths: a channel at power warms the notes a fourth away). Its reactivity, in dollars:
 *     the bank of rods (BANK; or the automatic regulator, AR, which moves it to hold the power where it is set)
 *     + its own rod (its key: withdrawn while held) + its enrichment (drawn with the plant)
 *     - its fuel's temperature (a negative coefficient: what makes a channel settle, breathing, after a rise)
 *     + its steam (VOID: a positive coefficient, as an RBMK's: past a point the channel rings, then runs away)
 *     - its xenon (fed by its iodine: a channel held long is poisoned after it is let go, and the poison moves
 *       round the ring: the power, and with it the harmony, wanders by itself)
 *   A neutron source keeps every channel counting a little. Time runs faster than a real core's (TEMPO).
 * THE CHANNELS  what each channel's neutrons sound like, as a detector would hear them: one by one, a count is a
 *   click struck into the channel's resonances; as the flux rises the counts thicken into a grain, then into a
 *   tone, which brightens with the power as Cherenkov light does (bluer, the more of it). The flux trembles as a
 *   core's does (vibrating fuel, boiling). Four fuels: DETECTOR, RODS, GLASS, MAINS.
 * THE MODERATOR  one deep voice under the hottest channel, which breaks down into what is under it.
 * THE LOOP  what the reactor's heat drives: the turbine, the pumps, the generator's hum, the dosimeter, relays,
 *   groans: all of them follow the thermal power, not the hands.
 * THE HALL  six seconds of memory, played back into a room that keeps only its own resonances (generation after
 *   generation, as in Lucier's room, Kirkegaard's at Chernobyl): what was played dissolves into the hall's drone.
 * AZ-5  all the rods driven in. They enter with their graphite tips first: for a second the bottom of the core
 *   gains reactivity. With a positive void coefficient, at that moment, the core can go prompt critical.
 * No C library; everything at half the FM-1's rate (the plate and the hall at a quarter), stepped up at the end. */
#ifndef SYNTH_H
#define SYNTH_H
#include <stdint.h>
#define SY_SR 44117.647f
#define SY_BLOCK 64
#define SY_NZ 16                                /* the fuel channels */
enum { SRC_FUEL, SRC_MOD, SRC_HALL, SY_NSRC };  /* what has a level, a send to the plate, to the hall */
/* the loop: what is there whether or not a key is held, and follows the reactor's heat */
enum { P_HUM, P_TURBINE, P_PUMP, P_GEIGER, P_RELAY, P_GROAN, P_GHOST, SY_NPLANT };
/* struck or held from the black keys (3.2) */
enum { EV_DOOR, EV_KNOCK, EV_GROAN, EV_TRIP, EV_STEP, EV_RELAYS, EV_BRAAM, EV_DROP, EV_STINGER };
/* every sound that has kinds (the first of each: the one first built); a plant is a draw of one of each */
enum { F_HUM, F_TURBINE, F_PUMP, F_GEIGER, F_RELAY, F_BREAKER, F_GROAN, F_STEPS, F_DOOR, F_RISER, F_SWELL, F_DROP, F_BRAAM, F_STINGER,
       F_FUEL,                                  /* the channels' voice: DETECTOR, RODS, GLASS, MAINS; its variant: the core's loading (enrichments) */
       F_SHIFT,                                 /* every frequency of the channels moved: down, up, or apart; gest[F_SHIFT]: how much (0: none) */
       F_GEN,                                   /* the hand that plays by itself (inst.c): which figure; gest[F_GEN]: how much, 0..1 */
       SY_NFAM };
extern const uint8_t sy_nkind[SY_NFAM];
extern const char *const sy_fam_name[SY_NFAM], *const sy_kind_name[SY_NFAM][7];
typedef struct {
    float zone_hz[SY_NZ];                       /* each channel's pitch (its key's note) */
    uint8_t rod[SY_NZ];                         /* its rod withdrawn (the key held, latched, or the hand that plays by itself) */
    float rx[4];                                /* REACTOR: BANK, VOID, XENON, TEMPO (0..1) */
    float rx2[4];                               /* COUPLING, AR TARGET, AR SPEED (0: the regulator off), SOURCE */
    float fu[4];                                /* the channels: RING, CHERENKOV, NOISE, SPREAD */
    float md[4];                                /* the moderator: TONE, FAULT, DEPTH, DECAY */
    float hl[4];                                /* the hall: TIME, GENERATIONS, GRAINS, SCATTER */
    uint8_t on[SY_NSRC];
    float lvl[SY_NSRC], space[SY_NSRC], hall[SY_NSRC];
    float room_size, room_t60, room_damp, room_mix;
    float plant[SY_NPLANT];                     /* each one's level, 0..1 */
    float horror;                               /* how much the heat bends everything: the pitch sags and wavers */
    uint8_t blackout;                           /* the grid let go (the relays, the breaker; the hum dies, the pumps stop) */
    uint8_t riser, swell;                       /* held: the strings rise; the breath grows */
    float root_hz;                              /* the key's root (the blows and the breath are tuned on it) */
    uint8_t kind[SY_NFAM], gene[SY_NFAM];       /* the plant drawn: which kind of each; its VARIANT, 0..255 */
    float gest[SY_NFAM];                        /* the level of each sound that has no place in plant[]: 0.7 as built */
} sy_ctl;
/* what the reactor says, for the panel */
typedef struct {
    float n[SY_NZ], rod[SY_NZ], xe[SY_NZ], temp[SY_NZ];   /* each channel: its power (1: full), its rod (0 in .. 1 out), its xenon, its fuel's heat */
    float power, thermal;                       /* the whole core's neutron power (the mean); its heat (behind it) */
    float rho, period, sur;                     /* the mean reactivity ($); the period (s; 0: steady), the startup rate (decades a minute) */
    float bank, voids, orm;                     /* the bank's reactivity ($); the steam; the operating reactivity margin, in rods */
    uint8_t trip, prompt, ar;                   /* the protection tripped (1 overpower, 2 period, 3 AZ-5); a channel past prompt critical; the regulator at work */
} sy_rx;
void sy_init(uint32_t seed);
void sy_kinds(const sy_ctl *c);                 /* the plant drawn, known before a block's events (sy_block takes it too) */
void sy_block(const sy_ctl *c, float *l, float *r);      /* SY_BLOCK frames */
const sy_rx *sy_reactor(void);
float sy_tension(void);                         /* how fast the power moves, 0..1 */
float sy_heat(void);                            /* the thermal power, 0..1 */
void sy_event(int ev);
void sy_step(float near);                       /* a step on the steel floor, 0 far .. 1 here */
void sy_az5(void);                              /* all the rods in, tips first; prompt critical: the cut, 2.5 s of nothing, all cold */
int sy_az5_phase(void);                         /* 0 none, 1 the rods going in, 2 the cut */
void sy_economy(int level);                     /* 0 everything; 1, 2: lighter */
float sy_plant(int which);                      /* for the panel: HUM alive 0..1, TURBINE rpm, PUMP's beat, GEIGER's rate, a RELAY just now, GROAN, GHOST: 0..1 */
float sy_hum_hz(void);
float sy_out_level(void);
float sy_limit(void);                           /* what the limiter takes off, 0..1 */
float sy_level(int src);
#endif
