/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XENOISE)
 * XENOISE: Wonderblue-OS over JIANT (firmware/src/xn_wb.c), the whole device as the browser runs it (web/emu/felucca_web.c,
 * built for the host): it boots into WND-1's tracker, PLAY plays its song on JIANT's engines (every track sounds, the
 * mix stays under full scale), STOP lets the synths go, the pages open from their buttons, a song saved to a slot comes
 * back, the genres each write a song that plays. Writes build/host/wb_play.wav (the first song, 8 s).
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/wb_test tests/wb_test.c -lm */
#include "../web/emu/felucca_web.c"
#include <stdio.h>
#include <stdlib.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("wb: %-76s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void run_ms(uint32_t ms) { while (ms--) web_ms(); ring_r = ring_w; }   /* (what played meanwhile: not heard) */
static void tap(uint32_t label) { web_buttons(1u << label); run_ms(60); web_buttons(0); run_ms(120); }
static double rms_of(uint32_t ms, float *peak, int16_t *wav, uint32_t *wn)
{
    double s = 0;
    uint32_t n = ms * 441u / 10u, k = 0;
    *peak = 0;
    while (k < n) {
        uint32_t m = n - k > 1024u ? 1024u : n - k, i;
        web_render(m);
        for (i = 0; i < m; i++) {
            float l = out_l[i], r = out_r[i];
            s += (double)l * l + (double)r * r;
            if (l > *peak) *peak = l; if (-l > *peak) *peak = -l;
            if (wav && *wn < 44100u * 8u) { wav[2u * *wn] = (int16_t)(l * 32767.0f); wav[2u * *wn + 1u] = (int16_t)(r * 32767.0f); (*wn)++; }
        }
        k += m;
    }
    return n ? s / (2.0 * n) : 0;
}
static void write_wav(const char *path, const int16_t *w, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t b = n * 4u, v;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36u + b; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 0x00020001u; fwrite(&v, 4, 1, f); v = 44100; fwrite(&v, 4, 1, f);
    v = 44100u * 4u; fwrite(&v, 4, 1, f); v = 0x00100004u; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f); fwrite(&b, 4, 1, f);
    fwrite(w, 4, n, f);
    fclose(f);
}

int main(void)
{
    static int16_t wav[44100 * 8 * 2];
    uint32_t wn = 0, t;
    float peak;
    double r;
    web_nor_erase();
    web_boot();
    run_ms(5000);                               /* (the splash first: its keys go nowhere) */
    check("boot: WND-1's tracker (TRK), the six tracks live on JIANT", page == PG_TRK && trk_live == NTRK);
    check("the tabs: HOM SAV CHD ENV LFO SEQ EDT MST; the third track PER",
          !strcmp(PG_NAME[0], "HOM") && !strcmp(PG_NAME[5], "SEQ") && !strcmp(PG_NAME[6], "EDT") && !strcmp(TRK_ID[2], "PER"));
    {   /* (XENOISE) every genre writes a song (a genre's table that loops the generator would hang the box) */
        uint32_t g;
        for (g = 0; g < WB_NGENRE; g++) song_new(g);
        check("every genre writes a new song (none hangs the generator)", 1);
        song_new(4);
    }
    {   /* (XENOISE) the song's form: SONG by default; SHUFFLE another that makes sense */
        uint32_t k, j, ok = 1, differ = 0;
        uint8_t was[16];
        check("a new song plays the SONG form (INTRO VERSE CHORUS .. OUTRO)", form_of() == 0 && wb_song.chain[0] == S_INTRO &&
              wb_song.chain[wb_song.nchain - 1u] == S_OUTRO && !strcmp(SECT_NAME[S_CHORUS], "CHORUS"));
        for (k = 0; k < 200u; k++) {
            memcpy(was, wb_song.chain, 16);
            form_shuffle();
            for (j = 0; j < wb_song.nchain; j++) {
                uint8_t c = wb_song.chain[j];
                if (c == S_DROP && (!j || (wb_song.chain[j - 1u] != S_BUILD && wb_song.chain[j - 1u] != S_DROP))) ok = 0;
                if (c == S_INTRO && j) ok = 0;
                if (c == S_OUTRO && j + 1u != wb_song.nchain) ok = 0;
            }
            if (wb_song.nchain < 3u || wb_song.nchain > 16u) ok = 0;
            if (memcmp(was, wb_song.chain, wb_song.nchain)) differ++;
        }
        printf("wb:   SHUFFLE x200: %u changed the form; the last: %u parts\n", differ, wb_song.nchain);
        check("SHUFFLE: a drop after a build, an intro first, an outro last, 3..16 parts; a new form each time",
              ok && differ >= 190u);
        form_set(0);
    }
    {   /* (XENOISE) HOM's black keys play; HOME held: the tracks. The track knob: BRK .. SYN, past SYN the visualizer */
        uint32_t c0 = xj_count[1], s0 = sel_trk, kb = KEY_OF_BLACK[0];
        sel_trk = T_KIK;
        web_keys(1u << kb); run_ms(80); web_keys(0); run_ms(80);
        check("HOM: a black key plays (a ghost on KIK), the track stays", xj_count[1] > c0 && sel_trk == T_KIK);
        web_buttons(1u << B_HOME); run_ms(30);
        web_keys(1u << KEY_OF_BLACK[3]); run_ms(60); web_keys(0); run_ms(30);
        web_buttons(0); run_ms(80);
        check("  HOME held + black 4: the track BAS", sel_trk == T_BAS);
        sel_trk = 0;
        web_enc(EN_ALGO, -1); run_ms(60);
        check("the track knob left of BRK: BRK (no wrap)", sel_trk == 0u && !vis_on);
        web_enc(EN_ALGO, 9); run_ms(60);
        check("  right past SYN: the visualizer", sel_trk == NT - 1u && vis_on);
        tap(B_PLAY); run_ms(9000);
        printf("wb:   the visualizer, 9 s of the song: wave %u, kills %u, shield %d\n", vis_wave, vis_kills, vis_shield);
        check("  playing: waves come, the tracks' hits shoot them down", vis_wave >= 1u && vis_kills >= 1u);
        web_buttons(1u << B_ARP); run_ms(600); web_buttons(0); run_ms(60);
        check("  ARP held: ENGAGING ALL RANGE PROTOCOL", vis_ar == 1u && vis_on && page == PG_TRK);
        run_ms(2200);
        check("  then ALL RANGE", vis_ar == 2u);
        web_enc(EN_ALGO, 4); run_ms(60);
        check("  ALGORITHM: it looks right", vis_yaw == 0x1000u);
        web_enc(EN_PRESET, -2); run_ms(60);
        check("  PRESETS left: it turns back left", vis_yaw == 0x0800u);
        web_enc(EN_PRESET, 1); web_enc(EN_ALGO, -1); run_ms(60);
        check("  either knob, either way", vis_yaw == 0x0800u);
        run_ms(3000);
        printf("wb:   ALL RANGE, 3 s: wave %u, kills %u, shield %d\n", vis_wave, vis_kills, vis_shield);
        tap(B_HOME);
        check("  HOME: DISENGAGING", vis_ar == 3u && vis_on);
        run_ms(1300);
        check("  then home: the tracker", !vis_on && !vis_ar && page == PG_TRK);
        tap(B_PLAY); run_ms(500);
        web_enc(EN_ALGO, 9); run_ms(60);
        web_enc(EN_ALGO, -1); run_ms(60);
        check("  left again: the tracker, SYN", !vis_on && sel_trk == NT - 1u);
        sel_trk = (uint8_t)s0;
    }
    {   /* (XENOISE) MST the settings: SELECT its group, ALGORITHM the parameter, PRESETS turns it; GLO again the mixer */
        tap(B_GLO);
        check("GLO: MST, its TEMPO page", page == PG_MST && sub == 0u);
        web_enc(EN_PRESET, 3); run_ms(60);
        check("  PRESETS x3 (the rows): SYSTEM", page == PG_MST && sub == 3u && pilot_col == 0u);
        web_enc(EN_K1, 1); run_ms(60);
        check("  K1: the next theme (COMBAT), kept in the settings' byte", TH == &WB_THEMES[1] && (xn_prefs & 15u) == 1u);
        web_enc(EN_K1, (int32_t)WB_NTHEME - 2); run_ms(60);
        check("  all the way: the last of the covers' themes", TH == &WB_THEMES[WB_NTHEME - 1u]);
        web_enc(EN_K1, 1); run_ms(60);
        check("  one more: CUSTOM (CONCRETE's colours until the THEME LAB sends one)", TH == &xn_custom && xn_custom.bg == WB_THEMES[0].bg);
        web_enc(EN_K1, 1); run_ms(60);
        check("  one more: round to CONCRETE", TH == &WB_THEMES[0]);
        web_enc(EN_SELECT, 1); run_ms(60);
        web_enc(EN_SELECT, -1); run_ms(60);
        check("  SELECT on THEME: on and back, CONCRETE (the default)", TH == &WB_THEMES[0] && (xn_prefs & 15u) == 0u);
        {   /* (XENOISE) THEME LAB: a theme on the wire (editor 99 / the emulator's web_xn_theme_put), CUSTOM, kept */
            uint8_t w[THEME_WIRE];
            uint32_t k;
            theme_wire_get(1u, w);                  /* COMBAT's colours ... */
            memcpy(w, "my theme\0\0\0", 11);       /* ... under a name of ours */
            w[11] = 0x1F; w[12] = 0x00; w[13] = 0x00;   /* bg: RGB565 0x001F */
            k = theme_wire_put(w);
            check("  THEME LAB: a theme from the wire is CUSTOM, picked, named, its colours", TH == &xn_custom && (xn_prefs & 15u) == WB_NTHEME
                  && !strcmp(xn_custom.tname, "MY THEME") && xn_custom.bg == 0x001Fu && xn_custom.sel == WB_THEMES[1].sel && k == 0u);
            xn_custom.bg = 0; theme_load();
            check("  ... saved: read back at the next start", xn_custom.bg == 0x001Fu);
            k = theme_wire_get(127u, w);
            check("  ... and read back over the wire as the one shown", k == WB_NTHEME && w[11] == 0x1F && w[0] == 'M');
            xj_pref_set(xj_pref() & ~15u); theme_apply();
        }
        web_enc(EN_PRESET, 1); run_ms(60);
        check("  PRESETS: round to TEMPO", sub == 0u);
        {
            uint32_t b0 = wb_song.bpm;
            web_enc(EN_SELECT, 2); run_ms(60);
            check("  SELECT: the BPM (ALGORITHM on the first)", wb_song.bpm != b0);
            web_enc(EN_SELECT, -2); run_ms(60);
            web_enc(EN_ALGO, 1); run_ms(60);
            check("  ALGORITHM: the next parameter", pilot_col == 1u);
            web_enc(EN_ALGO, -5); run_ms(60);
            check("  (no further than the first)", pilot_col == 0u);
        }
        tap(B_GLO);
        check("  GLO again: the mixer", page == PG_MIX);
        {
            uint32_t v0 = wb_song.mvol;
            web_enc(EN_SELECT, -4); run_ms(60);
            check("  SELECT: the master volume", wb_song.mvol + 4u == v0 || (v0 < 4u && wb_song.mvol == 0u));
            web_enc(EN_SELECT, 4); run_ms(60);
        }
        tap(B_GLO);
        check("  GLO again: the settings", page == PG_MST);
        tap(B_HOME);
    }
    {   /* (XENOISE) ENV, LFO, ARP: the track's sound, its LFO, its arp (JIANT's) */
        char lab[16], val[24];
        uint32_t s0 = sel_trk;
        sel_trk = T_BAS;
        tap(B_ENV);
        check("ENV: its page", page == PG_ENV);
        tap(B_ENV);
        {
            char lab[16] = "", val[24] = "";
            wb_knob(0, 0, lab, val);
            check("  ENV again (a synth): page 2, the filter envelope", sub == 1u && !strcmp(lab, "F.ENV"));
        }
        tap(B_ENV);
        run_ms(100);
        tap(B_LFO);
        check("LFO: its page", page == PG_LFO);
        web_enc(EN_K3, 10); run_ms(60);
        check("  K3: the pitch depth, the song's own LFO now", wb_song.x.lfo[T_BAS].set && wb_song.x.lfo[T_BAS].pit != 0);
        run_ms(100);
        tap(B_LFO);
        check("  LFO again: its page 2", page == PG_LFO && lfo_pg == 1u);
        tap(B_LFO);
        tap(B_ARP);
        check("ARP: its page", page == PG_ARP && sub == 0u);
        lab[0] = val[0] = 0;
        web_enc(EN_K1, 2); run_ms(60);
        check("  K1: the mode, the arp on", wb_song.x.arp[T_BAS].set && wb_song.x.arp[T_BAS].mode == 3u);
        run_ms(100);
        check("  JIANT's bass arp on", xj_arp_on[XJ_BAS]);
        tap(B_ARP);
        check("  ARP again: its second page", page == PG_ARP && sub == 1u);
        (void)lab; (void)val;
        tap(B_ARP);
        fn_press(3);
        run_ms(100);
        check("  F4: off (JIANT's too)", wb_song.x.arp[T_BAS].mode == 0u && !xj_arp_on[XJ_BAS]);
        sel_trk = (uint8_t)s0;
        tap(B_HOME);
    }
    {   /* (XENOISE) PERFORM's knobs: FILTER CHOP STEP WASH, gone when it closes */
        tap(B_FX);
        web_enc(EN_K1, 10); web_enc(EN_K2, 3); web_enc(EN_K4, 20); run_ms(100);
        check("FX: K1 high-pass, K2 the chop, K4 the wash", fx_flt > 0 && fx_chop == 3u && fx_wash > 0u);
        web_enc(EN_ALGO, 2); run_ms(60); web_enc(EN_SELECT, 1); run_ms(60);
        check("  PILOT: ALGORITHM the third punch-in, SELECT on", (punch_keys >> KEY_OF_WHITE[2]) & 1u);
        web_enc(EN_SELECT, -1); run_ms(60);
        check("  SELECT back: off", !((punch_keys >> KEY_OF_WHITE[2]) & 1u));
        web_enc(EN_PRESET, 1); run_ms(60); web_enc(EN_SELECT, 1); run_ms(60);
        check("  PRESETS: the tracks; SELECT: the third (PER) muted", fx_row == 1u && wb_song.wb_trk[2].mute);
        web_enc(EN_SELECT, -1); run_ms(60);
        check("  SELECT back: on again", !wb_song.wb_trk[2].mute);
        fx_row = 0;
        tap(B_FX);
        check("  FX closed: all off", !fx_flt && !fx_chop && !fx_wash);
        tap(B_HOME);
    }
    {   /* (XENOISE) HOM: the tracker alone; HOME again: the cell's knobs; again: alone */
        tap(B_HOME);
        check("HOM: plain (the knobs rest)", page == PG_TRK && !hom_edit);
        tap(B_HOME);
        check("  HOME again: NOTE VEL FX AMT", hom_edit && !strcmp(knob_lab[0], "") == 0);
        tap(B_HOME);
        check("  again: plain", !hom_edit);
    }
    {   /* (XENOISE) SEQ: copy, paste, insert, outsert, add, delete; SEQ again: LIVE, the white keys queue */
        uint32_t n0;
        tap(B_SEQ);
        check("SEQ: the sequence", page == PG_ARR && !arr_live);
        form_set(1);                            /* (CLUB: 12 slots, room for one more) */
        arr_pos = 2; n0 = wb_song.nchain;
        web_enc(EN_K1, 1); run_ms(60);
        check("  K1: COPY asked", arm_f == ARR_ASK && arr_clip == 0xFF);
        tap(B_OCTUP);
        check("  OCT+: copied the slot's pattern", arr_clip == wb_song.chain[2]);
        arr_pos = 0;
        web_enc(EN_K3, 1); run_ms(60); tap(B_OCTUP);
        check("  K3 at slot 1: inserted, the rest moved on", wb_song.nchain == n0 + 1u && wb_song.chain[0] == arr_clip);
        web_enc(EN_K4, 1); run_ms(60); tap(B_OCTUP);
        check("  K4: outserted, the rest moved back", wb_song.nchain == n0 && wb_song.chain[0] == S_INTRO);
        arr_pos = 1;
        web_enc(EN_K2, 1); run_ms(60);
        tap(B_OCTDN);
        check("  K2, OCT-: not pasted", arm_f == 0xFF);
        web_enc(EN_K2, 1); run_ms(60); tap(B_OCTUP);
        check("  K2 at slot 2, OCT+: pasted", wb_song.chain[1] == arr_clip);
        arr_fn(3);
        check("  F4 UNDO: the paste undone", wb_song.chain[1] != arr_clip || arr_clip == S_INTRO);
        web_enc(EN_K2, 1); run_ms(60); tap(B_OCTUP);
        tap(B_OCTUP);
        check("  OCT+: a slot added", wb_song.nchain == n0 + 1u);
        tap(B_OCTDN);
        check("  OCT-: the last deleted", wb_song.nchain == n0);
        arr_pos = 1;
        web_keys(1u << KEY_OF_BLACK[3]); run_ms(60); web_keys(0); run_ms(60);
        check("  black 4: BAS out of slot 2 (the matrix)", (wb_song.x.smute[1] >> T_BAS) & 1u);
        arr_fn(3);
        check("  F4 UNDO: back in", !((wb_song.x.smute[1] >> T_BAS) & 1u));
        tap(B_SEQ);
        check("  SEQ again: LIVE", arr_live);
        web_keys(1u << KEY_OF_WHITE[2]); run_ms(80); web_keys(0); run_ms(80);
        check("  LIVE, white 3: pattern 3, looped", wb_song.cur == 2u && wb_song.loop);
        web_enc(EN_K1, 1); run_ms(60);
        check("  LIVE, K1: the next one queued (pattern 4)", wb_song.cur == 3u);
        web_enc(EN_PRESET, -1); run_ms(60);
        check("  LIVE, PRESETS: back to 3", wb_song.cur == 2u);
        wb_song.loop = 0;
        tap(B_HOME);
    }
    {   /* (XENOISE) SAV: K1 save K2 load K3 copy K4 paste (a turn asks: OCT+ yes, OCT- no); SAVE again: K1 insert K2
         * outsert K3 new K4 delete; OCT- deletes (again: sure); OCT+ a new empty song, x4 a generated one */
        char nm[16];
        tap(B_SAVE);
        check("SAVE: the SAV page", page == PG_SNG && sub == 0u);
        web_enc(EN_PRESET, 5 - (int32_t)slot_sel); run_ms(60);
        check("  PRESETS (the rows): slot 5", slot_sel == 5u);
        web_enc(EN_K1, 1); run_ms(60);
        check("  K1: SAVE asked", arm_f == SA_SAVE);
        tap(B_OCTDN);
        check("  OCT-: cancelled, nothing saved", arm_f == 0xFF && !slot_ok[5]);
        web_enc(EN_K1, 1); run_ms(60); tap(B_OCTUP);
        check("  K1, OCT+: saved to slot 5", slot_ok[5] != 0);
        memcpy(nm, wb_song.wb_name, 16);
        web_enc(EN_K3, 1); run_ms(60); tap(B_OCTUP);
        check("  K3, OCT+: slot 5 copied", sng_clip == 5u);
        slot_sel = 7;
        web_enc(EN_K4, 1); run_ms(60); tap(B_OCTUP);
        check("  K4 at 7, OCT+: pasted there", slot_ok[7] && !memcmp(slot_name[7], nm, 15));
        tap(B_SAVE);
        check("  SAVE again: page 2", page == PG_SNG && sub == 1u);
        slot_sel = 6;
        web_enc(EN_K1, 1); run_ms(60); tap(B_OCTUP);
        check("  K1 INSERT at 6: the copy in 6, 7 now 8", slot_ok[6] && !slot_ok[7] && slot_ok[8]);
        web_enc(EN_K2, 1); run_ms(60); tap(B_OCTUP);
        check("  K2 OUTSERT 6: 7 now 6, 8 now 7", !slot_ok[6] && slot_ok[7] && !slot_ok[8]);
        slot_sel = 7;
        web_enc(EN_K4, 1); run_ms(60); tap(B_OCTUP);
        check("  K4 DELETE 7, OCT+", !slot_ok[7]);
        tap(B_SAVE);
        check("  SAVE again: page 1", sub == 0u);
        slot_sel = 5;
        tap(B_OCTUP); run_ms(900);
        check("  OCT+ a tap: a new empty song", !strcmp(wb_song.wb_name, "EMPTY SONG"));
        check("  the default kit: PUNCH, CRACK, WARM KEYS an octave down, TAPE EP, a light reverb",
              wb_song.wb_trk[T_KIK].patch == 5u && wb_song.wb_trk[T_DRM].patch == 3u && wb_song.wb_trk[T_BAS].patch == 5u &&
              wb_song.wb_trk[T_BAS].wb_p[PY_OCT] == -1 && wb_song.wb_trk[wb_T_KEY].patch == 3u && wb_song.wb_trk[wb_T_KEY].rev > 20u && wb_song.wb_trk[T_BRK].rev > 0u);
        web_enc(EN_K2, 1); run_ms(60); tap(B_OCTUP);
        check("  K2, OCT+: the song back from slot 5", !memcmp(wb_song.wb_name, nm, 15));
        tap(B_OCTUP); tap(B_OCTUP); tap(B_OCTUP); tap(B_OCTUP);
        check("  OCT+ four times: GENERATE asked", arm_f == 1u);
        tap(B_OCTUP);
        check("  OCT+: a new song, the parts in a form", memcmp(wb_song.wb_name, nm, 15) && strcmp(wb_song.wb_name, "EMPTY SONG") && form_of() == 0);
        check("  its bass: warm keys or a sine, an octave down", (wb_song.wb_trk[T_BAS].patch == 5u || wb_song.wb_trk[T_BAS].patch == 0u) && wb_song.wb_trk[T_BAS].wb_p[PY_OCT] == -1);
        tap(B_OCTDN);
        check("  OCT-: DELETE asked", arm_f == 3u && slot_ok[5]);
        tap(B_OCTDN);
        check("  OCT- again: slot 5 deleted", !slot_ok[5]);
        web_buttons(1u << B_SEQ); run_ms(60); web_buttons((1u << B_SEQ) | (1u << B_OCTDN)); run_ms(60); web_buttons(0); run_ms(120);
        check("  SEQ + OCT-: UNDO, slot 5 back, the page stays", page == PG_SNG && slot_ok[5] && !memcmp(slot_name[5], nm, 15));
        {
            char now[16];
            memcpy(now, wb_song.wb_name, 16);
            tap(B_OCTUP); tap(B_OCTUP); tap(B_OCTUP); tap(B_OCTUP); tap(B_OCTUP);
            check("  a new song generated", memcmp(wb_song.wb_name, now, 15) != 0);
            web_buttons(1u << B_SEQ); run_ms(60); web_buttons((1u << B_SEQ) | (1u << B_OCTDN)); run_ms(60); web_buttons(0); run_ms(120);
            check("  UNDO: the song before it", !memcmp(wb_song.wb_name, now, 15));
        }
        /* SAVE held: GEN, the next song's options */
        web_buttons(1u << B_SAVE); run_ms(600); web_buttons(0); run_ms(120);
        check("SAVE held: GEN", page == PG_GEN && sub == 0u);
        web_enc(EN_ALGO, 1); run_ms(60);
        web_enc(EN_SELECT, 2); run_ms(60);
        check("  ALGORITHM the knob (PROG), SELECT turns it", pilot_col == 1u && ng_prog == 2u);
        web_enc(EN_K3, 2); run_ms(60);
        tap(B_SAVE);
        check("  SAVE: its page 2", page == PG_GEN && sub == 1u);
        web_enc(EN_K1, 70); web_enc(EN_K2, 3); run_ms(60);
        check("  K1 BPM, K2 KEY", ng_bpm == 129u && ng_key == 3u);
        tap(B_OCTUP);
        check("  OCT+: GENERATE asked", arm_f == SA_NEW);
        tap(B_OCTUP);
        check("  OCT+: the song as asked (AXIS POP, a bar a chord, 129 BPM, D)",
              wb_song.prog == 1u && wb_song.rows == 16u && wb_song.bpm == 129u && wb_song.key == 2u);
        web_buttons(1u << B_SAVE); run_ms(600); web_buttons(0); run_ms(120);
        check("  SAVE held again: SAV", page == PG_SNG);
        ng_prog = ng_cspace = ng_bpm = ng_key = 0;
        tap(B_HOME);
    }
    {   /* (XENOISE) MIDI clock IN: START plays, the pulses set the tempo (20 ms apart: 125 BPM), STOP stops; OUT: JIANT's sender on */
        uint32_t k, b0 = wb_song.bpm;
        xn_prefs = (uint8_t)((xn_prefs & ~0x60u) | 2u << 5);
        run_ms(40);
        web_midi(0xFA, 0, 0); run_ms(40);
        for (k = 0; k < 60u; k++) { web_midi(0xF8, 0, 0); run_ms(20); }
        check("MIDI CLOCK IN: START plays, the tempo followed (125)", sq_play && wb_song.bpm == 125u);
        web_midi(0xFC, 0, 0); run_ms(40);
        check("  STOP stops", !sq_play);
        xn_prefs = (uint8_t)((xn_prefs & ~0x60u) | 1u << 5);
        run_ms(40);
        check("  CLOCK OUT: JIANT's clock sender on", mo_lvl == MO_CLOCK);
        xn_prefs = (uint8_t)(xn_prefs & ~0x60u);
        run_ms(40);
        wb_song.bpm = (uint8_t)b0;
    }
    web_buttons(1u << B_SEQ); run_ms(800);
    check("SEQ held: the PILOT's help over the page", help_on() && page == PG_TRK);
    web_buttons(0); run_ms(60);
    check("  let go: gone, the page as it was", !help_on() && page == PG_TRK);
    web_buttons(1u << B_ARP); run_ms(600); web_buttons(0); run_ms(60);
    check("ARP held on HOM: NOTE REPEAT on, the page stays", arp_on() && page == PG_TRK);
    web_buttons(1u << B_ARP); run_ms(600); web_buttons(0); run_ms(60);
    check("  held again: off", !arp_on() && page == PG_TRK);
    tap(B_FX);
    check("FX: a tap opens PERFORM, it stays", wb_fx_on());
    tap(B_FX);
    check("  FX again: closed, the page as it was", !wb_fx_on() && page == PG_TRK);
    tap(B_HOME);
    wb_song.loop = 1;                           /* (XENOISE: the CHORUS, every track in it; the INTRO leaves the synth out) */
    wb_song.cur = S_CHORUS;
    tap(B_PLAY);
    r = rms_of(8000, &peak, wav, &wn);
    wb_song.loop = 0;
    printf("wb:   the song: rms %.4f, peak %.3f; notes a track: %u %u %u %u %u %u; BPM %u, pattern %c\n", sqrt(r), peak,
           xj_count[0], xj_count[1], xj_count[2], xj_count[3], xj_count[4], xj_count[5], wb_song.bpm, 'A' + wb_song.cur);
    write_wav("build/host/wb_play.wav", wav, wn);
    check("PLAY: the song sounds (rms > 0.01), under full scale", sqrt(r) > 0.01 && peak < 1.0f);
    check("  notes on the break, the kit, the bass, the keys and the synth (JIANT tracks 1-5)",
          xj_count[0] && xj_count[1] && xj_count[2] && xj_count[3] && xj_count[4]);
    for (t = 0; t < NTRK; t++)
        printf("wb:   JIANT track %u: engine %s, level %d\n", t + 1, ENGINES[trk_eng(&trk[t])]->name, trk[t].p[P_LEVEL]);
    tap(B_PLAY);
    run_ms(3000);
    r = rms_of(500, &peak, 0, &wn);
    {
        uint32_t act = 0, i;
        for (t = 0; t < NTRK; t++) for (i = 0; i < NVOICE; i++) if (trk[t].v[i].active) {
            act++; printf("wb:   held: track %u voice %u note %u gate %u stage %u\n", t + 1, i, trk[t].v[i].note, trk[t].v[i].gate, trk[t].v[i].stage); }
        printf("wb:   after STOP: rms %.4f, playing %u, voices active %u\n", sqrt(r), sq_play, act);
    }
    check("STOP: quiet again after 3 s", sqrt(r) < 0.005);
    {   /* (XENOISE) the master's VOLUME, AGE and CRUSH (MST): xn_wb.c xn_master, fx.c */
        double full, half, worn;
        float pk2;
        tap(B_PLAY);
        run_ms(1000);
        full = sqrt(rms_of(4000, &peak, 0, &wn));
        wb_song.mvol = 50;
        run_ms(300);
        half = sqrt(rms_of(4000, &peak, 0, &wn));
        printf("wb:   VOLUME 100 %%: rms %.4f; 50 %%: %.4f (%.1f dB)\n", full, half, 20 * log10(half / full));
        check("MST VOLUME 50 %: the master ~12 dB down (square law)", half < full * 0.4 && half > full * 0.1);
        wb_song.mvol = 100; wb_song.age = 100; wb_song.crush = 100;
        run_ms(300);
        worn = sqrt(rms_of(4000, &pk2, 0, &wn));
        printf("wb:   AGE 100 %%, CRUSH 6 BIT: rms %.4f, peak %.3f\n", worn, pk2);
        check("MST AGE and CRUSH at full: it plays on, in range", worn > 0.01 && pk2 < 1.0f && xn_age == 100u && xn_crush == 100u);
        wb_song.age = wb_song.crush = 0;
        tap(B_PLAY);
        run_ms(3000);
    }
    if (getenv("WB_RENDER")) {   /* a genre's new song, 30 s of it from its first part (WB_RENDER=genre): build/host/wb_genre.wav */
        static int16_t w2[44100 * 30 * 2];
        uint32_t n2 = 0, j;
        float pk;
        song_new((uint32_t)atoi(getenv("WB_RENDER")));
        run_ms(500);
        tap(B_PLAY);
        for (j = 0; j < 30u; j++) {
            static int16_t w1[44100 * 8 * 2];
            uint32_t n1 = 0;
            rms_of(1000, &pk, w1, &n1);
            memcpy(w2 + 2u * n2, w1, n1 * 4u);
            n2 += n1;
        }
        write_wav("build/host/wb_genre.wav", w2, n2);
        printf("wb:   %s, %s: 30 s written to build/host/wb_genre.wav\n", WB_GENRES[wb_song.genre].wb_name, wb_song.wb_name);
        tap(B_PLAY);
    }
    if (getenv("WB_PER")) {   /* debug: PER alone */
        uint32_t j, w1 = 0, l; float pk;
        song_new(0);
        for (j = 0; j < 6u; j++) wb_song.wb_trk[j].mute = (uint8_t)(j != 2u);
        run_ms(200); tap(B_PLAY);
        for (j = 0; j < 6u; j++) xj_count[j] = 0;
        r = rms_of(3000, &pk, wav, &w1);
        printf("PER: rms %.4f pk %.3f notes %u, lanes:", sqrt(r), pk, xj_count[1]);
        for (l = 0; l < 8u; l++) printf(" %d", trk[1].p[P_LN0 + l]);
        printf(" level %d mute %d eng %s dx_mute %u vol %u\n", trk[1].p[P_LEVEL], trk[1].p[P_MUTE], ENGINES[trk_eng(&trk[1])]->name, dx_mute, wb_song.wb_trk[2].vol);
        tap(B_PLAY);
    }
    if (getenv("WB_CAL")) {   /* each preset variant alone (JIANT track 3, a note of its role, 1.5 s): its trim to one level */
        static const uint8_t BASSY[27] = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0};
        uint32_t k, v, w1;
        float pk;
        trk_live = NTRK;
        for (k = 0; k < 27u; k++) {
            printf("cal %-12s", WB_SYN_NAME[k]);
            for (v = 0; v < 3u; v++) {
                const xs_patch_t *x = &XS_PATCH[k];
                uint32_t note = BASSY[k] ? 36u : 60u, i;
                double rr;
                xj_sound(2, x->eng[v], x->word[v]);
                xj_tone(2, x->cut, 0);
                trk[2].p[P_LEVEL] = 104; trk[2].p[P_REV] = 0; trk[2].p[P_DLY] = 0; trk[2].p[P_CHOR] = 0;
                run_ms(300);
                for (i = 0; i < (BASSY[k] ? 1u : 3u); i++) xj_on(2, note + (uint32_t[]){0, 4, 7}[i], 100);
                w1 = 0; rr = rms_of(1000, &pk, wav, &w1);
                for (i = 0; i < 3u; i++) xj_off(2, note + (uint32_t[]){0, 4, 7}[i]);
                run_ms(2500);
                printf(" %-10s %6.4f %+5.1fdB", x->word[v], sqrt(rr), rr > 0 ? -10.0 * log10(rr / (0.05 * 0.05)) : 0.0);
            }
            printf("\n");
        }
        {   /* the kits: a kick, a snare, a closed hat */
            uint32_t kit;
            for (kit = 0; kit < 9u; kit++) {
                static const char *const KN[9] = {"909 CLUB", "808 BOOM", "INDUSTRIAL", "LOFI DUST", "808 BOOM", "TECHNO RAW", "INDUSTRIAL", "ACOUSTIC", "909 CLUB"};
                uint32_t d;
                xj_sound(1, ENGI_DRUM, KN[kit]);
                trk[1].p[P_LEVEL] = 104; trk[1].p[P_REV] = 0; trk[1].p[P_DLY] = 0;
                printf("kit %-10s", KN[kit]);
                for (d = 0; d < 3u; d++) {
                    double rr;
                    run_ms(400);
                    xj_on(1, (uint32_t[]){36, 38, 42}[d], 100);
                    w1 = 0; rr = rms_of(250, &pk, wav, &w1);
                    printf(" %u: %6.4f pk %5.3f", (uint32_t[]){36, 38, 42}[d], sqrt(rr), pk);
                }
                printf("\n");
            }
        }
    }
    if (getenv("WB_SURVEY")) {   /* each genre's song, each track alone: its level and brightness (the mix's balance) */
        uint32_t g, k;
        for (g = 0; g < WB_NGENRE; g += (uint32_t)atoi(getenv("WB_SURVEY"))) {
            song_new(g);
            printf("survey %-14s", WB_GENRES[g].wb_name);
            for (k = 0; k < 6u; k++) {
                uint32_t j, n = 0, w1 = 0;
                double e = 0, c = 0, m = 0;
                float pk;
                for (j = 0; j < 6u; j++) wb_song.wb_trk[j].mute = (uint8_t)(j != k);
                run_ms(200);
                tap(B_PLAY);
                r = rms_of(6000, &pk, wav, &w1);
                tap(B_PLAY);
                run_ms(1500);
                for (j = 1; j < w1; j++) {          /* brightness: the first difference against the signal (a crude centroid) */
                    double a0 = wav[2u * j - 2u], a1 = wav[2u * j];
                    e += (a1 - a0) * (a1 - a0); m += a1 * a1; n++;
                }
                c = m > 0 ? sqrt(e / m) * 44100.0 / 6.283 : 0;
                printf(" %s %5.3f/%4.2f", TRK_ID[k], sqrt(r), pk); (void)c;
            }
            for (k = 0; k < 6u; k++) wb_song.wb_trk[k].mute = 0;
            printf("\n");
        }
    }
    printf(bad ? "wb: %d FAILED\n" : "wb: all passed\n", bad);
    return bad ? 1 : 0;
}
