/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XANOISE)
 * XANOISE: the Wonderblue tracker over JIANT (firmware/src/xn_tracker.c) on the host build of the UI tests: it is the
 * screen at power-on, a function button hands over to JIANT's page and HOME comes back, HOME + SELECT opens GEN and a
 * piece made there comes back to the tracker on its steps, the knobs and REC write steps, F1..F4, and Wonderblue's
 * genres make pieces in JIANT's generator.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/xn_test tests/xn_test.c -lm */
#define XN_BOOT_ON 1
#define XN_HOME_BACK 1
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int xcheck(const char *what, int ok)
{
    printf("xn: %-76s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static uint32_t xn_steps(void)                  /* the steps with something on them, every track */
{
    uint32_t k, i, n = 0;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NSTEP; i++)
            n += (trk[k].step[i].n && trk[k].step[i].time == ST_NOTE) || trk[k].step[i].hit;
    return n;
}
static uint32_t genre_of(const char *name)
{
    uint32_t g;
    for (g = 0; g < NGENRE; g++)
        if (str_eq(GENRE[g].name, name)) return g + 1u;
    return 0;
}
static void key_tap(uint32_t k)
{
    fm1_in.notes |= 1u << k;
    host_notes |= 1u << k;
    frame();
    fm1_in.notes &= ~(1u << k);
    frame();
}

int main(void)
{
    int bad = 0;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();          /* TRACKS 6 */
    frames(200);
    bad += xcheck("power-on: the tracker has the screen", xn_on == 1);
    press(B_FX);
    bad += xcheck("FX: JIANT's FX page (the tracker hands over)", xn_on == 0 && !ui.home);
    press(B_HOME);
    bad += xcheck("HOME there: back to the tracker", xn_on == 1);
    {   /* HOME + SELECT: GEN; OCT+ a piece; OCT-: done, back to the tracker */
        uint32_t before;
        gp.genre = (uint8_t)genre_of("TRIP HOP");
        before = xn_steps();
        held_turn(B_HOME, 1);
        bad += xcheck("HOME + SELECT: GEN over JIANT's pages", gen_on() && xn_on == 0);
        frame();
        bad += xcheck("  HOME let go after the turn is no tap: GEN stays", gen_on() && xn_on == 0);
        press(B_OCTDN);
        frames(100);
        bad += xcheck("  OCT- on GEN: done, back to the tracker", !gen_on() && xn_on == 1);
        held_turn(B_HOME, 1);
        fm1_in.buttons |= 1u << panel.btn[B_HOME];
        frames(1000);
        fm1_in.buttons &= ~(1u << panel.btn[B_HOME]);
        frame();
        bad += xcheck("  HOME held a second after the turn: no MENU, GEN stays", gen_on() && !ui.menu);
        gp.genre = (uint8_t)genre_of("TRIP HOP");
        press(B_OCTUP);
        frames(100);
        press(B_OCTDN);
        frames(100);
        bad += xcheck("GEN, OCT+ then OCT-: a TRIP HOP piece, back to the tracker", !gen_on() && xn_on == 1 &&
                      xn_steps() > before && song.g[G_BPM] >= 76 && song.g[G_BPM] <= 92);
    }
    {   /* the knobs and the keys on a note track */
        uint32_t t = 1;
        track_select(t);
        frames(50);
        memset(trk[t].step, 0, sizeof trk[t].step);
        trk[t].p[P_SLEN] = 16;
        xn_row = 0;
        turn(EN_K1, 3);
        bad += xcheck("KNOB 1 on an empty step: a note there", trk[t].step[0].n == 1 && trk[t].step[0].time == ST_NOTE);
        turn(EN_SELECT, 2);
        bad += xcheck("SELECT: the cursor two steps on", xn_row == 2);
        press(B_REC);
        key_tap(0);
        bad += xcheck("REC + a white key: the note at the cursor, the cursor on", trk[t].step[2].n == 1 && xn_row == 3);
        press(B_REC);
        xn_row = 0;
        xn_fn(0);
        bad += xcheck("F1: the step cleared", !trk[t].step[0].n);
        xn_fn(3);
        bad += xcheck("F4: the length 16 -> 32 (the steps again)", trk[t].p[P_SLEN] == 32 && trk[t].step[18].n == 1);
    }
    {   /* Wonderblue's genres: each makes a piece in its tempo, on several tracks */
        static const char *const WB[] = {"TRIP HOP", "AMBIENT BREAK", "LOFI HIP HOP", "JUNGLE", "RPG TOWN", "RPG BATTLE", "RPG BOSS"};
        uint32_t i, k, ok = 1;
        for (i = 0; i < sizeof WB / sizeof WB[0]; i++) {
            uint32_t g = genre_of(WB[i]), used = 0;
            if (!g) { ok = 0; printf("xn:   %s missing\n", WB[i]); continue; }
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(100u + i);
            for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
            gen_go();
            for (k = 0; k < NTRK; k++) used += !seq_is_empty(&trk[k]);
            if (used < 3u || song.g[G_BPM] < GENRE[g - 1u].bpm_lo || song.g[G_BPM] > GENRE[g - 1u].bpm_hi) {
                ok = 0;
                printf("xn:   %s: %u tracks, %d BPM\n", WB[i], used, song.g[G_BPM]);
            }
        }
        bad += xcheck("Wonderblue's genres: a piece each, three tracks or more, in the genre's tempo", ok);
    }
    printf(bad ? "xn: %d FAILED\n" : "xn: all passed\n", bad);
    return bad ? 1 : 0;
}
