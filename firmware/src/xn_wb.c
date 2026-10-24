/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XANOISE) */
/* XANOISE: Wonderblue-OS (WND-1) over JIANT FM. WND-1's front end as it was (wb/: its song, pages, panel, LEDs,
 * generator, sequencer and store, from github.com/Xanacognition/WND-1), JIANT's sound under it: the engines, the
 * voices, the effects and the mix, on both cores, the hot code in RAM. WND-1's own voices and master are not here
 * (they overran the audio interrupt on the FM-1); its sequencer plays JIANT's tracks (wb/wb_seq.c), its sounds pick
 * JIANT's engines and presets (xs_sound below), its mixer and master set JIANT's (xs_mix).
 *
 *   JIANT track  1 BRK (DRUM: the breaks as hits)   2 KIK + DRM (DRUM)   3 BAS   4 KEY   5 SYN   6 KEY's counter voice
 *
 * WND-1's names clash with JIANT's (song, track_t, trk ..), both being one compilation unit each: its files are
 * included between the renames below; JIANT is reached from them only through the xj_ functions before them. */

#ifndef XN_WB
#define XN_WB 1
#endif

/* ---- JIANT, for WND-1's code (its names as JIANT has them) */
#define XJ_BRK 0u
#define XJ_DRM 1u
#define XJ_BAS 2u
#define XJ_KEY 3u
#define XJ_SYN 4u
#define XJ_CTR 5u
static uint32_t xj_count[NTRK];                  /* notes sent a track (the tests) */
static void xj_on(uint32_t jt, uint32_t note, uint32_t vel) { xj_count[jt % NTRK]++; trk_note_on(&trk[jt % NTRK], note, vel); }
static void xj_off(uint32_t jt, uint32_t note) { trk_note_off(&trk[jt % NTRK], note); }
static void xj_release(uint32_t jt) { trk_all_off(&trk[jt % NTRK]); }
static int xj_busy(void)                         /* a voice sounding on any track (the autosave waits) */
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++)
        for (i = 0; i < NVOICE; i++)
            if (trk[t].v[i].active) return 1;
    return 0;
}
static uint32_t xj_ms_idx(int32_t ms)            /* JIANT's time index (0..127) nearest a time in ms */
{
    uint32_t i = 0;
    while (i < 127u && (int32_t)TIME_MS_X10[i + 1u] <= ms * 10) i++;
    return i;
}
/* a JIANT track to an engine and the first preset whose name holds the word (none: the engine's first) */
static void xj_sound(uint32_t jt, uint32_t eng, const char *word)
{
    track_t *t = &trk[jt % NTRK];
    const engine_t *e = ENGINES[eng % NENGINES];
    uint32_t k;
    if (trk_eng(t) != eng) set_engine_of(t, eng);
    for (k = 0; k < e->npresets && !dname_has(e->presets[k].name, word); k++) {}
    apply_preset_to(t, k < e->npresets ? k : 0u);
}
static void xj_env(uint32_t jt, int32_t att, int32_t dec, int32_t sus, int32_t rel)   /* ms, ms, %, ms */
{
    track_t *t = &trk[jt % NTRK];
    t->p[P_ATK] = (int16_t)xj_ms_idx(att);
    t->p[P_DEC] = (int16_t)xj_ms_idx(dec);
    t->p[P_SUS] = (int16_t)clamp(sus * 127 / 100, 0, 127);
    t->p[P_REL] = (int16_t)xj_ms_idx(rel);
    if (trk_eng(t) == ENGI_FM6) {                /* FM6: its carriers' release as JIANT's generator leaves it (gen.c) */
        uint32_t r4 = fm6_rel_r4(t), mx = r4 > 40u ? (r4 - 40u) * 127u / 90u : 0u;
        if ((uint32_t)t->p[P_REL] > mx) t->p[P_REL] = (int16_t)mx;
    }
}
static void xj_mix(uint32_t jt, int32_t vol, int32_t pan, int32_t rev, int32_t dly)   /* 0..100, -50..50, %, % */
{
    track_t *t = &trk[jt % NTRK];
    t->p[P_LEVEL] = (int16_t)clamp(vol * 127 / 100, 0, 127);
    t->p[P_PAN] = (int16_t)clamp(pan * 64 / 50, -64, 63);
    t->p[P_REV] = (int16_t)clamp(rev * 127 / 100, 0, 127);
    t->p[P_DLY] = (int16_t)clamp(dly * 127 / 100, 0, 127);
}
static void xj_lane(uint32_t lane, int32_t vol)   /* KIK and DRM share a kit: their levels as its lanes' */
{
    trk[XJ_DRM].p[P_LN0 + lane % 8u] = (int16_t)clamp(vol * 127 / 100, 0, 127);
}
static void xj_global(uint32_t g, int32_t v) { song.g[g % G_COUNT] = (int16_t)v; }
static void xj_tracks6(void) { trk_live = NTRK; }
static uint32_t xj_scope(int16_t *out, uint32_t n)   /* the output's last n samples, every 8th (WND-1's scope) */
{
    uint32_t i, w = scope_w;
    for (i = 0; i < n; i++)
        out[i] = scope_buf[(w - (n - i) * 8u) & (SCOPE_N - 1u)];
    return n;
}

/* ---- WND-1, its names apart from JIANT's (wb/wb_names.h: tools/wb_names.py) */
#if defined(XN_WB_RAMSTORE)
#define POOL                                        /* (the tests) */
#else
#define POOL __attribute__((section(".pool")))
#endif
#ifndef STORE_LO
#define STORE_LO 0x00097000u                        /* WND-1's store, as on WND-1 (JIANT's FL_DATA: its projects were there) */
#define STORE_HI 0x000E0000u
#endif
#define WB_NAMES_ON
#include "wb/wb_names.h"
#include "wb/wb_platform.h"
#include "wb/wb_brk.h"
#include "wb/kawa_art.h"
#include "wb/wb_font.h"
#include "wb/wb_tables.h"
#include "wb/wb_data.h"
#include "wb/wb_core.c"
#include "wb/wb_gen.c"
#include "wb/wb_seq.c"
#include "wb/kawa_gfx.c"
#include "wb/wb_ui.c"

/* ---- WND-1's sounds, mixer and master on JIANT's (the main loop, when they change) */
typedef struct { uint8_t eng; const char *word; } xs_patch_t;
static const xs_patch_t XS_PATCH[WB_NSYN] = {      /* WB_SYN_NAME's, each the nearest JIANT preset (light engines first) */
    {ENGI_ANALOG, "SUB BASS"}, {ENGI_FM6, "FM PLUCK"}, {ENGI_ANALOG, "FAT BASS"}, {ENGI_FM6, "TINE EP"},
    {ENGI_ANALOG, "JUNO PAD"}, {ENGI_PHASE, "CZ PIANO"}, {ENGI_PHASE, "BELL"}, {ENGI_PHASE, "HARP"},
    {ENGI_WHEEL, "JAZZ PERC"}, {ENGI_ANALOG, "BRASS"}, {ENGI_ANALOG, "SAW LEAD"}, {ENGI_ANALOG, "RAVE"},
    {ENGI_ANALOG, "HOOVER"}, {ENGI_ANALOG, "SYNC LEAD"}, {ENGI_LOFI, "PULSE LD"}, {ENGI_ANALOG, "PLUCK"},
    {ENGI_ANALOG, "PWM STR"}, {ENGI_ANALOG, "STRINGS"}, {ENGI_ANALOG, "ACID"}, {ENGI_ANALOG, "REESE"},
    {ENGI_ANALOG, "SUB BASS"}, {ENGI_PHASE, "FB BASS"}, {ENGI_VOICE, "TALK BOX"}, {ENGI_VOICE, "CHOIR AAH"},
    {ENGI_VOICE, "VOX LEAD"}, {ENGI_VOICE, "TALK BOX"}, {ENGI_VOICE, "OOH PAD"}};
static uint32_t xs_sig[8];
static uint32_t xs_hash(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}
/* the synth sound a WND-1 patch index names: its preset (the patch: the closest WB_SYN to the track's values) */
static uint32_t xs_patch_of(const int16_t *p)
{
    uint32_t k, best = 0, bd = 0xFFFFFFFFu;
    for (k = 0; k < WB_NSYN; k++) {
        uint32_t d = 0, i;
        for (i = 0; i < 16u; i++) {
            int32_t x = p[i] - WB_SYN[k][i];
            d += (uint32_t)(x < 0 ? -x : x) * (i == PY_ENG ? 100000u : 1u);
        }
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}
static void xs_synth(uint32_t wt, uint32_t jt, const int16_t *p)
{
    uint32_t k = xs_patch_of(p);
    xj_sound(jt, XS_PATCH[k].eng, XS_PATCH[k].word);
    xj_env(jt, p[PY_ATT], p[PY_DEC], p[PY_SUS], p[PY_REL]);
    (void)wt;
}
static void xs_sound(void)
{
    uint32_t t, h;
    for (t = 0; t < NT; t++) {
        const track_t *s = &song.trk[t];
        h = xs_hash(s->p, sizeof s->p) ^ s->patch * 7919u ^ s->patch2 * 104729u;
        if (h == xs_sig[t]) continue;
        xs_sig[t] = h;
        switch (TRK_TYPE[t]) {
        case TY_BREAK: xj_sound(XJ_BRK, ENGI_DRUM, "BREAKS"); break;
        case TY_KICK: xj_sound(XJ_DRM, ENGI_DRUM, "LOFI DUST"); xs_sig[T_DRM] = 0; break;
        case TY_DRUMS: break;
        default:
            xs_synth(t, t == T_BAS ? XJ_BAS : t == T_KEY ? XJ_KEY : XJ_SYN, s->p);
            if (t == T_KEY) xs_synth(t, XJ_CTR, WB_SYN[clampi(s->p[PY_CTRP], 0, WB_NSYN - 1)]);
        }
    }
}
static void xs_mix(void)
{
    static uint32_t sig;
    uint32_t t, h = xs_hash(song.trk, sizeof song.trk) ^ song.bpm ^ song.space << 8 ^ song.pump << 16 ^
                    (uint32_t)song.mvol << 24 ^ punch_lpf * 77u ^ punch_dub * 131u ^ punch_wash * 997u;
    static const uint8_t JT[NT] = {XJ_BRK, XJ_DRM, XJ_DRM, XJ_BAS, XJ_KEY, XJ_SYN};
    if (h == sig) return;
    sig = h;
    for (t = 0; t < NT; t++) {
        const track_t *s = &song.trk[t];
        if (t == T_KIK) { xj_lane(0, s->vol); continue; }
        if (t == T_DRM) {
            uint32_t l;
            for (l = 1; l < 8u; l++) xj_lane(l, s->vol);
            xj_mix(XJ_DRM, 80, s->pan, s->rev, s->dly);
            continue;
        }
        xj_mix(JT[t], s->vol, s->pan, s->rev, s->dly);
        if (t == T_KEY) xj_mix(XJ_CTR, s->vol, s->pan, s->rev, s->dly);
    }
    xj_global(G_BPM, song.bpm ? song.bpm : 100);
    xj_global(G_RSIZE, 40 + song.space * 87 / 100 + (punch_wash ? 40 : 0) > 127 ? 127 : 40 + song.space * 87 / 100 + (punch_wash ? 40 : 0));
    xj_global(G_DFDBK, punch_dub ? 105 : 60);
    xj_global(G_DUCK, song.pump);
    xj_global(G_DJF, punch_lpf ? -44 : 0);
}

/* ---- the hooks: JIANT's boot, main loop and audio interrupt (main.c, seq.c) */
static uint8_t wb_ready;
static void wb_init(void)
{
    xj_tracks6();
    app_init();
    xs_sound();
    xs_mix();
    wb_ready = 1;
}
static void wb_frame(void)                       /* the main loop: WND-1's panel, screen and LEDs, then the sound */
{
    static int16_t sc[120];
    app_step();
    xs_sound();
    xs_mix();
    xj_scope(sc, 120u);
    memcpy(scope, sc, sizeof sc);
    scope_w = 0;
}
static void wb_isr(uint32_t n)                   /* the audio interrupt, between two blocks (fx.c events_block) */
{
    if (wb_ready) wb_block(n);
}

#define WB_NAMES_OFF
#include "wb/wb_names.h"
#undef POOL
#undef g_buf
#undef g_txb
