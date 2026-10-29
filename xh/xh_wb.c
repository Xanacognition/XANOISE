/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XENOISE) */
/* XENOISE-H: XENOISE's front end (firmware/src/wb/: the tracker, generator, pages, Pilot System, as XENOISE has them)
 * over HYP-1's instrument (xh/engine/amb.c, RedtailWorks, from AMB-1 by Charles Vestal) instead of JIANT's engines.
 * The same xj_ functions the front end calls on JIANT (firmware/src/xn_wb.c), here on HYP-1's voices, each played by
 * hand (its P_*_MAN: off its generator), its drum lanes fired hit by hit (en_hit):
 *
 *   BRK          the drum lanes: a break's slice as its hits (wb_brk.h), synthesized, no samples
 *   KIK, PER     the drum lanes: kick; snare, clap, closed and open hat, perc
 *   BAS          the DRONE (one voice, its 24 dB low-pass, its sub)
 *   KEY          the PAD (four voices); KEY's counter voice too
 *   SYN          the ACID (one voice, 303 slides when the notes overlap)
 *
 * A browser prototype first (xh/build.sh): what is not mapped yet answers as nothing (the LFO and ARP pages, the
 * meters, the envelope knobs, AGE and CRUSH, the MIDI clock). */

#define XN_WB 1
#include "engine/amb.h"

/* ---- HYP-1, for the front end (the names xn_wb.c gives JIANT's tracks) */
#define NTRK 6u
#define XJ_BRK 0u
#define XJ_DRM 1u
#define XJ_BAS 2u
#define XJ_KEY 3u
#define XJ_SYN 4u
#define XJ_CTR 5u
static uint32_t xj_count[NTRK];                  /* notes sent a track (the tests) */
static uint8_t xh_held[NTRK][128];               /* the notes each voice holds (a release lets them all go) */
static int xh_lane_of(uint32_t note)             /* a GM drum note to HYP-1's lane */
{
    switch (note) {
    case 35: case 36: return 0;                  /* kick */
    case 38: case 40: return 1;                  /* snare */
    case 39: return 2;                           /* clap */
    case 42: case 44: return 3;                  /* closed hat */
    case 46: return 4;                           /* open hat */
    default: return 5;                           /* perc: toms, rim, cowbell */
    }
}
static int xh_voice(uint32_t jt) { return jt == XJ_BAS ? EN_V_DRONE : jt == XJ_SYN ? EN_V_ACID : EN_V_PAD; }
static int xh_key(uint32_t jt, uint32_t note)    /* HYP-1's voices take the keys an octave or two down: given back */
{
    int k = (int)note + (jt == XJ_BAS ? 24 : jt == XJ_SYN ? 12 : 0);
    return k < 0 ? 0 : k > 127 ? 127 : k;
}
static void xj_on(uint32_t jt, uint32_t note, uint32_t vel)
{
    jt %= NTRK;
    xj_count[jt]++;
    if (jt == XJ_BRK || jt == XJ_DRM) { en_hit(xh_lane_of(note), (int)vel); return; }
    xh_held[jt][note & 127u] = 1;
    en_note(xh_voice(jt), xh_key(jt, note), 1);
}
static void xj_off(uint32_t jt, uint32_t note)
{
    jt %= NTRK;
    if (jt == XJ_BRK || jt == XJ_DRM || !xh_held[jt][note & 127u]) return;
    xh_held[jt][note & 127u] = 0;
    en_note(xh_voice(jt), xh_key(jt, note), 0);
}
static void xj_release(uint32_t jt)
{
    uint32_t n;
    jt %= NTRK;
    for (n = 0; n < 128u; n++) if (xh_held[jt][n]) xj_off(jt, n);
}
/* not mapped yet: the LFO and ARP pages, the meters, the presets by name, the envelope knobs */
static const char *const N_LWAVE[5] = {"SIN", "TRI", "SAW", "SQR", "S&H"};
static void xj_lfo_read(uint32_t jt, int8_t *v) { uint32_t i; (void)jt; for (i = 0; i < 7u; i++) v[i] = 0; }
static void xj_lfo_write(uint32_t jt, const int8_t *v) { (void)jt; (void)v; }
static void xj_arp_write(uint32_t jt, int on, const uint8_t *a) { (void)jt; (void)on; (void)a; }
static uint32_t xj_arp_view(uint32_t jt, uint8_t *held, uint32_t *idx) { (void)jt; (void)held; *idx = 0; return 0; }
static const char *xj_lfo_wave(uint32_t v) { return N_LWAVE[v % 5u]; }
static const char *xj_lsync(int32_t v) { (void)v; return "SYNC"; }
static const char *xj_amode(uint32_t v) { (void)v; return "UP"; }
static const char *xj_div(uint32_t v) { (void)v; return "1/16"; }
static int32_t xj_peak(uint32_t jt)              /* the voices' levels as the meters' peaks (Q15) */
{
    float l = jt == XJ_BAS ? en_bass_level : jt == XJ_SYN ? en_acid_level : jt == XJ_KEY || jt == XJ_CTR ? en_pad_level : 0.0f;
    if (jt == XJ_BRK || jt == XJ_DRM) { int d; for (d = 0; d < EN_NDRUM; d++) if (en_drum_glow[d] > l) l = en_drum_glow[d]; }
    return (int32_t)(l > 1.0f ? 32767.0f : l * 32767.0f);
}
static int xj_busy(void) { return 0; }
static void xj_env(uint32_t jt, int32_t att, int32_t dec, int32_t sus, int32_t rel) { (void)jt; (void)att; (void)dec; (void)sus; (void)rel; }
static int xj_fm6_elsewhere(uint32_t jt) { (void)jt; return 0; }
static void xj_tracks6(void) {}
/* the output's last samples, every 8th, for WND-1's scope and the visualizer (xh_web.c fills the ring) */
#define XH_SCOPE_N 2048u
static int16_t xh_scope[XH_SCOPE_N];
static uint32_t xh_scope_w;
static uint32_t xj_scope(int16_t *out, uint32_t n)
{
    uint32_t i, w = xh_scope_w;
    for (i = 0; i < n; i++) out[i] = xh_scope[(w - (n - i) * 8u) & (XH_SCOPE_N - 1u)];
    return n;
}

/* ---- SETTINGS (a byte; xn_wb.c keeps it in JIANT's settings): here in RAM */
static struct { uint8_t factory[16][32]; } favorites;
#define xn_prefs (favorites.factory[15][31])
static uint32_t xj_pref(void) { return xn_prefs; }
static void xj_pref_set(uint32_t v) { xn_prefs = (uint8_t)v; }
static int32_t xj_sin(uint32_t a16)              /* sine of a 16-bit angle, Q15 (the visualizer) */
{
    int32_t x = (int32_t)(a16 & 0xFFFFu) - 32768, ax = x < 0 ? -x : x, y = (x * (32768 - ax)) >> 13;   /* a parabola, */
    int32_t ay = y < 0 ? -y : y;                 /* refined (as wb_seq.c sine) */
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return -(y > 32767 ? 32767 : y < -32767 ? -32767 : y);
}
static uint8_t mix_skip;                         /* (the emulator's stems: not yet) */

/* ---- the front end, its names apart (wb/wb_names.h) */
#define POOL
#ifndef STORE_LO
#define STORE_LO 0x00097000u
#define STORE_HI 0x000E0000u
#endif
#include "../firmware/src/xn_themes.h"
#define WB_NAMES_ON
#include "../firmware/src/wb/wb_names.h"
#include "../firmware/src/wb/wb_platform.h"
#include "../firmware/src/wb/wb_brk.h"
#include "../firmware/src/wb/kawa_art.h"
#include "../firmware/src/wb/wb_font.h"
#include "../firmware/src/wb/wb_tables.h"
#include "../firmware/src/wb/wb_data.h"
#include "../firmware/src/wb/wb_core.c"
#include "../firmware/src/wb/wb_gen.c"
#include "../firmware/src/wb/wb_seq.c"
#include "../firmware/src/wb/kawa_gfx.c"
#include "../firmware/src/wb/wb_ui.c"

/* ---- XENOISE's 27 sounds on HYP-1's: each voice's sound (pad, drone, acid: 16 each) nearest in character */
static const uint8_t XH_PAD[WB_NSYN] = {   /* Rhodes Voices Strings Glass Organ Warm Stabs Dark Dub Trance Poly Hollow Pings Hits Rave Sine */
    15, 12, 8, 0, 5, 0, 3, 12, 4, 6, 9, 14, 14, 10, 10, 12, 11, 2, 6, 7, 15, 13, 1, 1, 1, 1, 1};
static const uint8_t XH_DRONE[WB_NSYN] = { /* Saws Warm Grit Dirty Reese Pulse FM Organ Rumble Sub Square Hoover Fifths Metal Sync Bright */
    9, 6, 1, 1, 0, 1, 6, 6, 7, 15, 0, 11, 11, 14, 10, 5, 5, 0, 3, 4, 9, 2, 12, 12, 12, 12, 12};
static const uint8_t XH_ACID[WB_NSYN] = {  /* Saw Square Driven Sync Pluck PWM Chip Sub Trance Bleep Fifth Bell Organ Laser Fold Soft */
    7, 4, 1, 15, 5, 15, 11, 11, 12, 0, 8, 8, 14, 3, 1, 4, 5, 15, 2, 2, 7, 4, 9, 15, 13, 10, 15};
/* XENOISE's kicks (909 BOOM HARDSTYLE DUSTY DEEP-808 PUNCH GABBER SOFT CLICKY) to HYP-1's kits */
static const uint8_t XH_KIT[WB_NKICK] = {1, 7, 6, 4, 7, 2, 6, 5, 5};   /* Hypno House Techno Acid Break Minimal Hard 808 */
static int16_t xh_par[P_NPARAMS];
static uint8_t xh_par_set[P_NPARAMS];
static void xh_set(int p, int v)                 /* a parameter, clamped to its range, sent when it changes */
{
    const en_param_t *pi = en_param_info(p);
    if (v < pi->lo) v = pi->lo;
    if (v > pi->hi) v = pi->hi;
    if (xh_par_set[p] && xh_par[p] == v) return;
    xh_par_set[p] = 1;
    xh_par[p] = (int16_t)v;
    en_set(p, v);
}
static void xs_sound(void)                       /* the tracks' sounds and levels, as the song has them */
{
    const track_t *k = &song.trk[T_KIK], *d = &song.trk[T_DRM];
    int top = k->vol > d->vol ? k->vol : d->vol, l;
    xh_set(P_PADTONE, XH_PAD[song.trk[T_KEY].patch % WB_NSYN]);
    xh_set(P_DR_INST, XH_DRONE[song.trk[T_BAS].patch % WB_NSYN]);
    xh_set(P_AC_WAVE, XH_ACID[song.trk[T_SYN].patch % WB_NSYN]);
    l = XH_KIT[k->patch % WB_NKICK];
    xh_set(P_KIT_KICK, l); xh_set(P_KIT_SNARE, l); xh_set(P_KIT_HAT, l); xh_set(P_KIT_PERC, l);
    /* the levels: VOL (0..100, 80 as full) on the voices' and the lanes' */
    xh_set(P_PAD, song.trk[T_KEY].mute ? 0 : song.trk[T_KEY].vol * 100 / 80);
    xh_set(P_BASS, song.trk[T_BAS].mute ? 0 : song.trk[T_BAS].vol * 100 / 80);
    xh_set(P_ACID, song.trk[T_SYN].mute ? 0 : song.trk[T_SYN].vol * 100 / 80);
    xh_set(P_PULSE, top * 100 / 80);
    xh_set(P_LANE_LVL(0), k->mute ? 0 : top ? k->vol * 100 / top : 0);
    for (l = 1; l < EN_NDRUM; l++) xh_set(P_LANE_LVL(l), d->mute ? 0 : top ? d->vol * 100 / top : 0);
}
static void xs_mix(void)
{
    xh_set(P_TEMPO, song.bpm ? song.bpm : 100);
    xh_set(P_REVERB, song.space);
    xh_set(P_SWING, 0);                          /* (the tracker swings its own rows) */
}
static void xs_ext(void) {}

/* ---- the hooks: boot, the main loop, the audio interrupt */
static uint8_t wb_ready;
static void wb_init(void)
{
    int p;
    en_init();
    for (p = 0; p < EN_NPAT; p++) { int l; for (l = 0; l < EN_NLANE; l++) en_pats[p][l] = 0; }   /* (its own drums: none) */
    xh_set(P_PAD_MAN, 1); xh_set(P_DR_MAN, 1); xh_set(P_AC_MAN, 1);   /* the voices by hand: the tracker's notes only */
    xh_set(P_NOISE, 0);
    xh_set(P_EVOLVE, 0); xh_set(P_MOTION, 0); xh_set(P_DRIFT, 0);
    xh_set(P_OCT, 0); xh_set(P_PAD_OCT, 0); xh_set(P_BASSOCT, 0); xh_set(P_AC_OCT, 0);
    xh_set(P_DD_CHANGE, 0); xh_set(P_DD_MUTATE, 0); xh_set(P_LINKS, 0);
    en_play(1);
    app_init();
    xs_sound();
    xs_mix();
    wb_ready = 1;
}
static void wb_frame(void)
{
    static int16_t sc[120];
    app_step();
    xs_sound();
    xs_mix();
    xj_scope(sc, 120u);
    memcpy(scope, sc, sizeof sc);
    scope_w = 0;
}
static void wb_isr(uint32_t n) { if (wb_ready) wb_block(n); }

#define WB_NAMES_OFF
#include "../firmware/src/wb/wb_names.h"
#undef POOL
