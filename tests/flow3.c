/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL 4 on the PC: the hands' ways through the instrument, checked one by one (the rods, the pages, HOLD, AZ-5,
 * the board, the locks, the draws, the hand that plays by itself, what SAVE keeps). No sound is listened to here. */
#include "../src/inst.h"
#include <stdio.h>
#include <string.h>
static const int WHITE[16] = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 }, BLACK[11] = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
enum { B_FX = 2, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC }; enum { E_K1 = 2, E_K2, E_K3, E_K4, E_PRESETS };
static inst G; static uint32_t ms; static int fails;
static void wait(uint32_t d) { for (uint32_t t = 0; t < d; t += 10) { ms += 10; inst_tick(&G, ms); } }
static void press(int b) { inst_button(&G, b, 1); wait(50); inst_button(&G, b, 0); wait(50); }
static void hold(int b, uint32_t d) { inst_button(&G, b, 1); wait(d); inst_button(&G, b, 0); wait(50); }
static int rods_out(void) { int n = 0; for (int j = 0; j < SY_NZ; j++) n += inst_rod_out(&G, j) != 0; return n; }
#define CHECK(c, what) do { if (!(c)) { printf("  FAILED: %s\n", what); fails++; } else printf("  ok: %s\n", what); } while (0)
int main(void)
{
    inst_new(&G, 4242); wait(3000);
    CHECK(!G.run, "before PLAY nothing runs");
    press(B_PLAY); CHECK(G.run && G.started, "PLAY: it runs, a plant is drawn");
    CHECK(G.sel == PG_REACTOR && !strcmp(inst_knob_name(&G, 0), "BANK") && !strcmp(inst_knob_name(&G, 1), "VOID"), "the first page: the reactor (BANK, VOID, XENON, TEMPO)");
    {   /* the rods */
        CHECK(rods_out() == 0, "no key held: every rod in");
        inst_key(&G, WHITE[3], 1); CHECK(inst_rod_out(&G, 3) && rods_out() == 1, "a white key held: its channel's rod out, only that one");
        inst_key(&G, WHITE[3], 0); CHECK(rods_out() == 0, "let go: back in");
        for (int k = 0; k < 27; k++) { int w = -1; for (int j = 0; j < 16; j++) if (WHITE[j] == k) w = j; if (w < 0) { inst_key(&G, k, 1); inst_key(&G, k, 0); } }
        CHECK(rods_out() == 0, "the black keys move no rod");
        press(B_ARP); inst_key(&G, WHITE[0], 1); inst_key(&G, WHITE[0], 0); inst_key(&G, WHITE[9], 1); inst_key(&G, WHITE[9], 0);
        CHECK(G.hold && inst_rod_out(&G, 0) && inst_rod_out(&G, 9) && rods_out() == 2, "HOLD: rods stay out when their keys are let go");
        inst_key(&G, WHITE[0], 1); inst_key(&G, WHITE[0], 0); CHECK(!inst_rod_out(&G, 0) && inst_rod_out(&G, 9), "HOLD: the key again puts that rod back in");
        press(B_ARP); CHECK(!G.hold && rods_out() == 0, "HOLD off: every rod left out goes back in");
        uint64_t led = inst_leds(&G); inst_key(&G, WHITE[5], 1); CHECK((inst_leds(&G) ^ led) != 0, "a rod out: its key lit"); inst_key(&G, WHITE[5], 0);
    }
    {   /* the pages */
        press(B_SEL); CHECK(G.sel == PG_FUEL && !strcmp(inst_knob_name(&G, 1), "CHERENKOV"), "SEL: the channels' page");
        press(B_SEL); CHECK(!G.P.on[SRC_FUEL], "SEL again: the channels off"); press(B_SEL); CHECK(G.P.on[SRC_FUEL], "and on");
        press(B_ENV); CHECK(G.sel == PG_MOD, "ENV: the moderator"); press(B_LFO); CHECK(G.sel == PG_LOOP, "LFO: the loop"); press(B_GLO); CHECK(G.sel == PG_HALL, "GLO: the hall");
        press(B_FX); float v0 = G.P.rx[1]; inst_enc(&G, E_K2, 5); CHECK(G.sel == PG_REACTOR && G.P.rx[1] > v0, "FX: the reactor; KNOB 2 turns the void");
        inst_button(&G, B_EDIT, 1); CHECK(!strcmp(inst_knob_name(&G, 1), "AR TARGET"), "EDIT held on the reactor: the regulator's knobs");
        float a0 = G.P.rx2[2]; inst_enc(&G, E_K3, -3); CHECK(G.P.rx2[2] < a0, "EDIT + KNOB 3: AR SPEED"); inst_button(&G, B_EDIT, 0);
        for (int i = 0; i < 300; i++) { inst_enc(&G, E_K1, 1); } CHECK(G.P.rx[0] == 1.0f, "a knob stops at its end");
        for (int i = 0; i < 300; i++) { inst_enc(&G, E_K1, -1); } CHECK(G.P.rx[0] == 0.0f, "and at its other end");
        inst_enc(&G, E_PRESETS, -1); inst_enc(&G, E_PRESETS, 1);
    }
    {   /* AZ-5 */
        press(B_REC); CHECK(!G.az5_req, "REC pressed short: no AZ-5");
        inst_button(&G, B_REC, 1); wait(1100); CHECK(G.az5_req, "REC held 1 s: AZ-5 asked"); inst_button(&G, B_REC, 0); G.az5_req = 0;
        press(B_ARP); inst_key(&G, WHITE[2], 1); inst_key(&G, WHITE[2], 0); G.az = 2; wait(20); G.az = 0;
        CHECK(rods_out() == 0, "the cut: a rod left out by HOLD goes back in"); press(B_ARP);
    }
    press(B_SAVE); CHECK(G.sel != PG_BOARD && !G.save_req, "SAVE pressed short: nothing moves, nothing is written");
    press(B_HOME); CHECK(G.sel == PG_BOARD && !G.page, "HOME: the board, where it was left");
    inst_key(&G, BLACK[SIG_DOOR], 1); wait(100); inst_key(&G, BLACK[SIG_DOOR], 0);
    CHECK(inst_row_fam[G.cur] == F_DOOR && G.cur >= NROW_NOTES, "a black key: the board goes to its row (the plant's page)");
    int k0 = G.P.kind[F_DOOR];
    inst_enc(&G, E_K2, -1); CHECK(!G.lock[F_DOOR] && G.P.kind[F_DOOR] == k0, "KIND turned left on a free sound: still free, unchanged");
    inst_enc(&G, E_K2, 1); CHECK(G.lock[F_DOOR] && G.P.kind[F_DOOR] == k0, "KIND turned right: the kind drawn is taken as it is, locked");
    for (int i = 0; i < 8; i++) inst_enc(&G, E_K2, 1);
    CHECK(G.P.kind[F_DOOR] == sy_nkind[F_DOOR] - 1, "KIND to the right: the last kind, no further");
    for (int i = 0; i < 8; i++) inst_enc(&G, E_K2, -1);
    CHECK(!G.lock[F_DOOR], "KIND to the left of the first: free again");
    inst_enc(&G, E_K2, 1); inst_enc(&G, E_K2, 1); int kd = G.P.kind[F_DOOR];
    inst_enc(&G, E_K3, 4); CHECK(G.P.gene[F_DOOR] > 0 && G.lock[F_DOOR], "VARIANT: it moves, and locks");
    float l0 = inst_row_level(&G, G.cur); inst_enc(&G, E_K4, -10); CHECK(inst_row_level(&G, G.cur) < l0, "LEVEL: the row's level");
    inst_key(&G, BLACK[SIG_BRAAM], 1); wait(100); inst_enc(&G, E_PRESETS, 1); inst_enc(&G, E_PRESETS, 1); int kb = G.P.kind[F_BRAAM], sc = G.scene; inst_key(&G, BLACK[SIG_BRAAM], 0);
    CHECK(G.lock[F_BRAAM] && sc == 0, "a black key held + PRESETS: its kind, locked (the scene does not move)");
    {   /* the notes' rows: the fuel, the moderator, the hall, the shift, the hand */
        for (int i = 0; i < NROW; i++) inst_enc(&G, E_K1, -1);
        CHECK(G.cur == 0 && inst_row_fam[0] == F_FUEL, "KNOB 1 to the top: the first row, the FUEL");
        inst_enc(&G, E_K2, 1); for (int i = 0; i < 4; i++) inst_enc(&G, E_K2, -1); inst_enc(&G, E_K2, 1); inst_enc(&G, E_K2, 1);
        CHECK(G.lock[F_FUEL] && G.P.kind[F_FUEL] == 1, "the fuel chosen by hand: RODS, locked");
        inst_enc(&G, E_K1, 1); float m0 = G.P.lvl[SRC_MOD]; inst_enc(&G, E_K2, 3); inst_enc(&G, E_K4, -5); CHECK(G.P.lvl[SRC_MOD] < m0, "the moderator's row: no kind; KNOB 4 its level");
        inst_enc(&G, E_K1, 1); inst_enc(&G, E_K1, 1); CHECK(inst_row_fam[G.cur] == F_SHIFT && !inst_row_variant(G.cur), "the shift: a kind, no variant");
        inst_enc(&G, E_K1, 1); CHECK(inst_row_fam[G.cur] == F_GEN, "then the hand that plays by itself");
    }
    int changed = 0; uint8_t gd = G.P.gene[F_DOOR];
    for (int i = 0; i < 12; i++) { uint8_t was[SY_NFAM]; memcpy(was, G.P.kind, SY_NFAM); hold(B_PLAY, 1100); if (memcmp(was, G.P.kind, SY_NFAM)) changed++;
        if (G.P.kind[F_DOOR] != kd || G.P.kind[F_BRAAM] != kb || G.P.kind[F_FUEL] != 1 || G.P.gene[F_DOOR] != gd || !G.run) { changed = -100; break; } }
    CHECK(changed >= 8, "PLAY held: a new plant each time; what is locked stays; it keeps running");
    float hum = G.P.plant[P_HUM], dl = G.P.gest[F_DOOR]; uint8_t kn[SY_NFAM]; memcpy(kn, G.P.kind, SY_NFAM);
    inst_enc(&G, E_PRESETS, 1); CHECK(G.scene == 1 && G.P.plant[P_HUM] == hum && G.P.gest[F_DOOR] == dl && !memcmp(kn, G.P.kind, SY_NFAM), "PRESETS: another scene; the plant, its levels and its kinds stay");
    inst_button(&G, B_EDIT, 1); float r0 = G.room[0]; inst_enc(&G, E_K1, -5); CHECK(G.room[0] < r0, "EDIT held on the board: the room"); inst_button(&G, B_EDIT, 0);
    press(B_HOME); press(B_HOME); CHECK(G.sel == PG_BOARD && G.page, "HOME again, from the plant's page: the rods' wall");
    press(B_HOME); CHECK(!G.page && G.cur < NROW_NOTES, "HOME again: the notes' page"); press(B_HOME); CHECK(G.cur >= NROW_NOTES, "HOME again: the plant's page");
    press(B_FX); CHECK(G.sel == PG_REACTOR && !G.page, "FX: back to the reactor");
    {   /* the hand that plays by itself: it withdraws rods */
        CHECK(G.P.gest[F_GEN] == 0, "the hand that plays by itself is off until asked");
        press(B_SEQ); CHECK(G.P.gest[F_GEN] == 0.5f, "SEQ: it plays, at half");
        int heard = 0; for (int i = 0; i < 2000 && !heard; i++) { wait(10); for (int j = 0; j < SY_NZ; j++) if (G.gen_off[j]) heard = 1; }
        CHECK(heard, "within twenty seconds it withdraws a rod");
        inst_key(&G, WHITE[6], 1); int mine = G.down[6]; inst_key(&G, WHITE[6], 0); CHECK(mine, "the hands still move their rods");
        G.P.gest[F_GEN] = 1.0f; wait(30000); int many = 0; for (int i = 0; i < 3000; i++) { wait(10); if (rods_out() > 0) many++; }
        CHECK(many > 1500, "at full it keeps rods out most of the time");
        press(B_SEQ); wait(50);
        CHECK(G.P.gest[F_GEN] == 0 && rods_out() == 0, "SEQ again: it stops, its rods go back in");
        press(B_SEQ); CHECK(G.P.gest[F_GEN] == 1.0f, "SEQ again: back where it was"); press(B_SEQ);
    }
    hold(B_SAVE, 1200); CHECK(G.save_req, "SAVE held: asks for everything to be kept");
    static uint8_t buf[2048]; static inst H; int n = inst_save(&G, buf); inst_new(&H, 99); int ok = inst_load(&H, buf, n);
    CHECK(ok && n < 1024 && H.scene == G.scene && H.root == G.root && !memcmp(H.lock, G.lock, sizeof G.lock) && H.P.kind[F_DOOR] == kd && H.P.kind[F_BRAAM] == kb && H.P.kind[F_FUEL] == 1 && H.P.gene[F_DOOR] == gd && H.P.gest[F_DOOR] == dl && H.P.plant[P_HUM] == hum
          && !memcmp(H.P.rx, G.P.rx, sizeof G.P.rx) && !memcmp(H.P.rx2, G.P.rx2, sizeof G.P.rx2),
          "what was kept comes back: the scene, the key, the reactor's knobs, the locks, the locked kinds and variants, the levels");
    for (int j = 0; j < SY_NZ; j++) { if (H.P.rod[j]) ok = 0; } CHECK(ok, "and no rod comes back out");
    buf[7] ^= 1; inst_new(&H, 99); CHECK(inst_load(&H, buf, n - 1) == 0, "a save of another length is refused");
    {   /* the draw: over 6000 starts a millisecond apart, every kind of every sound comes as often as the others (within a fifth) */
        static int cnt[SY_NFAM][8]; int bad = 0; static uint32_t seen[6000]; int same = 0;
        for (int i = 0; i < 6000; i++) { inst_new(&H, 777); H.now_ms = 3000u + (uint32_t)i; inst_button(&H, B_PLAY, 1); inst_button(&H, B_PLAY, 0);
            uint32_t h = 0; for (int f = 0; f < SY_NFAM; f++) { cnt[f][H.P.kind[f]]++; h = h * 8u + H.P.kind[f]; } seen[i] = h; }
        for (int f = 0; f < SY_NFAM; f++) for (int k = 0; k < sy_nkind[f]; k++) { float e = 6000.0f / sy_nkind[f]; if (cnt[f][k] < 0.8f * e || cnt[f][k] > 1.2f * e) { bad++; printf("    %s %s: %d of %.0f\n", sy_fam_name[f], sy_kind_name[f][k], cnt[f][k], e); } }
        for (int i = 1; i < 6000; i++) if (seen[i] == seen[i - 1]) same++;
        CHECK(!bad && same < 3, "the draw is even, and two starts a millisecond apart give two plants");
    }
    printf(fails ? "flow3: %d FAILED\n" : "flow3: all the ways hold\n", fails);
    return fails != 0;
}
