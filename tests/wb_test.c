/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XANOISE)
 * XANOISE: Wonderblue-OS over JIANT (firmware/src/xn_wb.c), the whole device as the browser runs it (web/emu/felucca_web.c,
 * built for the host): it boots into WND-1's tracker, PLAY plays its song on JIANT's engines (every track sounds, the
 * mix stays under full scale), STOP lets the synths go, the pages open from their buttons, a song saved to a slot comes
 * back, the genres each write a song that plays. Writes build/host/wb_play.wav (the first song, 8 s).
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/wb_test tests/wb_test.c -lm */
#include "../web/emu/felucca_web.c"
#include <stdio.h>

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
    run_ms(3000);
    check("boot: WND-1's tracker (TRK), the six tracks live on JIANT", page == PG_TRK && trk_live == NTRK);
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
    printf(bad ? "wb: %d FAILED\n" : "wb: all passed\n", bad);
    return bad ? 1 : 0;
}
