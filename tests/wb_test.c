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
    check("the tabs: HOM SAV CHD MIX GEN SEQ EDT MST; the third track PER",
          !strcmp(PG_NAME[0], "HOM") && !strcmp(PG_NAME[5], "SEQ") && !strcmp(PG_NAME[6], "EDT") && !strcmp(TRK_ID[2], "PER"));
    tap(B_FX);
    check("FX: a tap opens PERFORM, it stays", wb_fx_on());
    tap(B_FX);
    check("  FX again: closed, the page as it was", !wb_fx_on() && page == PG_TRK);
    tap(B_ARP);
    check("ARP: a tap on, it stays; again: off", arp_on());
    tap(B_ARP);
    check("  (off)", !arp_on());
    tap(B_SAVE);
    check("SAVE: the SAV page", page == PG_SNG);
    {
        uint32_t s0 = slot_sel;
        tap(B_SAVE);
        check("  SAVE again: the question (OCT+ yes)", arm_f == 0u);
        tap(B_OCTDN);
        check("  OCT-: cancelled, nothing saved", arm_f == 0xFF && !slot_ok[s0]);
        tap(B_SAVE); tap(B_OCTUP);
        check("  SAVE, OCT+: saved to the slot", slot_ok[s0] != 0);
    }
    tap(B_HOME);
    tap(B_PLAY);
    r = rms_of(8000, &peak, wav, &wn);
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
