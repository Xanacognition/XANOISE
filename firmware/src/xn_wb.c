/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XENOISE) */
/* XENOISE: Wonderblue-OS (WND-1) over JIANT FM. WND-1's front end as it was (wb/: its song, pages, panel, LEDs,
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
static uint8_t xj_arp_on[NTRK];                  /* (XENOISE) the ARP page's arp on a track: its notes go to JIANT's arp */
static void xj_on(uint32_t jt, uint32_t note, uint32_t vel)
{
    track_t *t = &trk[jt % NTRK];
    xj_count[jt % NTRK]++;
    if (xj_arp_on[jt % NTRK]) arp_add(t, note);   /* (seq.c: arp_step plays them, every block) */
    else trk_note_on(t, note, vel);
}
static void xj_off(uint32_t jt, uint32_t note)
{
    if (xj_arp_on[jt % NTRK]) arp_remove(&trk[jt % NTRK], note);
    else trk_note_off(&trk[jt % NTRK], note);
}
static void xj_release(uint32_t jt)
{
    track_t *t = &trk[jt % NTRK];
    if (!t->p[P_AHOLD]) { t->nheld = 0; t->arp_phys = 0; }
    trk_all_off(t);
}
/* (XENOISE) the LFO and ARP pages: JIANT's per-track LFO (RATE WAVE FADE, depths to PITCH FILTER SHAPE AMP) and arp */
static void xj_lfo_read(uint32_t jt, int8_t *v)   /* rate wave fade pit flt shp amp */
{
    const int16_t *p = trk[jt % NTRK].p;
    v[0] = (int8_t)clamp(p[P_LRATE], -12, 127); v[1] = (int8_t)p[P_LWAVE]; v[2] = (int8_t)clamp(p[P_LFADE], 0, 127);
    v[3] = (int8_t)p[P_LD_PIT]; v[4] = (int8_t)p[P_LD_FLT]; v[5] = (int8_t)p[P_LD_SHP]; v[6] = (int8_t)clamp(p[P_LD_AMP], 0, 127);
}
static void xj_lfo_write(uint32_t jt, const int8_t *v)
{
    int16_t *p = trk[jt % NTRK].p;
    p[P_LRATE] = v[0]; p[P_LWAVE] = (int16_t)clamp(v[1], 0, 4); p[P_LFADE] = v[2];
    p[P_LD_PIT] = v[3]; p[P_LD_FLT] = v[4]; p[P_LD_SHP] = v[5]; p[P_LD_AMP] = v[6];
}
static void xj_arp_write(uint32_t jt, int on, const uint8_t *a)   /* mode rate oct gate swing prob hold order */
{
    track_t *t = &trk[jt % NTRK];
    if (!on) {
        if (xj_arp_on[jt % NTRK]) { t->nheld = 0; t->arp_phys = 0; t->p[P_AHOLD] = 0; }
        xj_arp_on[jt % NTRK] = 0;
        t->p[P_AMODE] = 0;
        return;
    }
    xj_arp_on[jt % NTRK] = 1;
    t->p[P_AMODE] = (int16_t)clamp(a[0], 0, 15); t->p[P_ARATE] = (int16_t)clamp(a[1], 0, 9); t->p[P_AOCT] = (int16_t)clamp(a[2], 1, 4);
    t->p[P_AGATE] = (int16_t)clamp(a[3], 1, 127); t->p[P_ASWING] = (int16_t)clamp(a[4], 0, 100); t->p[P_APROB] = (int16_t)clamp(a[5], 0, 127);
    t->p[P_AHOLD] = (int16_t)(a[6] != 0); t->p[P_AORDER] = (int16_t)(a[7] != 0);
}
static uint32_t xj_arp_view(uint32_t jt, uint8_t *held, uint32_t *idx)   /* the notes held, how far the arp is */
{
    const track_t *t = &trk[jt % NTRK];
    uint32_t i, n = t->nheld < 16u ? t->nheld : 16u;
    for (i = 0; i < n; i++) held[i] = t->held[i];
    *idx = t->arp_idx;
    return n;
}
static const char *xj_lfo_wave(uint32_t v) { return N_LWAVE[v % 5u]; }
static const char *xj_lsync(int32_t v) { return N_LSYNC[(uint32_t)(-v - 1) % 12u]; }
static const char *xj_amode(uint32_t v) { return N_AMODE[v % 16u]; }
static const char *xj_div(uint32_t v) { return N_DIV[v % 10u]; }
static int32_t xj_peak(uint32_t jt) { return trk[jt % NTRK].peak; }   /* the track's peak this block, Q15 */
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
    for (k = 0; k < e->npresets && !str_eq(e->presets[k].name, word); k++) {}   /* the name itself, else one holding it */
    if (k >= e->npresets) for (k = 0; k < e->npresets && !dname_has(e->presets[k].name, word); k++) {}
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
static int32_t xj_vol_steps(int32_t vol)          /* WND-1's VOL (0..100, square law, 80 as 0 dB) in LEVEL steps (~0.5 dB) */
{
    int32_t want = 4096 * vol * vol / 6400, i, best = 0;
    for (i = 1; i < 128; i++) {                      /* (no libc abs on the FM-1) */
        int32_t d = (int32_t)LEVEL_Q12[i] - want, e = (int32_t)LEVEL_Q12[best] - want;
        if ((d < 0 ? -d : d) < (e < 0 ? -e : e)) best = i;
    }
    return vol <= 0 ? -104 : best - 104;
}
/* a part's level: VOL, then dB more or less (its role, its sound's trim), its pan and sends */
static void xj_mix(uint32_t jt, int32_t vol, int32_t db, int32_t pan, int32_t rev, int32_t dly)   /* 0..100, dB, -50..50, %, % */
{
    track_t *t = &trk[jt % NTRK];
    t->p[P_LEVEL] = (int16_t)(vol <= 0 ? 0 : clamp(104 + xj_vol_steps(vol) + 2 * db, 1, 127));
    t->p[P_PAN] = (int16_t)clamp(pan * 64 / 50, -64, 63);
    t->p[P_REV] = (int16_t)clamp(rev * 127 / 100, 0, 127);
    t->p[P_DLY] = (int16_t)clamp(dly * 127 / 100, 0, 127);
}
static void xj_lane(uint32_t lane, int32_t vol, int32_t top)   /* KIK and PER share a kit: their VOLs as its lanes' (square */
{                                                                  /* law, as VOL), the louder one at 100 % */
    trk[XJ_DRM].p[P_LN0 + lane % 8u] = (int16_t)(top > 0 ? clamp(vol * 127 / top, 0, 127) : 0);
}
static void xj_e(uint32_t jt, uint32_t i, int32_t v) { trk[jt % NTRK].p[P_E0 + i % 8u] = (int16_t)clamp(v, 0, 127); }
static void xj_tone(uint32_t jt, int32_t cut, int32_t glide)   /* the part's filter (127 open), its glide */
{
    track_t *t = &trk[jt % NTRK];
    t->p[P_FCUT] = (int16_t)clamp(cut, 20, 127);
    t->p[P_GLIDE] = (int16_t)clamp(glide, 0, 127);
}
/* ---- (XENOISE) SETTINGS (wb_ui.c PG_SET), in a byte of JIANT's settings no engine uses (favorites.factory[15][31]:
 * settings_persist.c, "the layers seen", unused since 0.9.5; 0 in every older setting = each one's default), saved
 * with them (settings_save: written by settings_poll when it changed):
 *   bits 0-3  the theme (wb_ui.c WB_THEMES: 0 COMBAT, 1 CLASSIC, KNF's covers after them)
 *   bit 4     HOM's black keys: 0 play (HOME held: the tracks and F1..F4), 1 the tracks and F1..F4 as WND-1 had them */
#define xn_prefs (favorites.factory[15][31])
static uint32_t xj_pref(void) { return xn_prefs; }
static int32_t xj_sin(uint32_t a16) { return bg_sin(a16); }   /* sine of a 16-bit angle, Q15 (the visualizer) */
static void xj_pref_set(uint32_t v)
{
    if (xn_prefs == (uint8_t)v) return;
    xn_prefs = (uint8_t)v;
    settings_save();
}

/* ---- (XENOISE) the master's AGE, CRUSH and VOLUME (WND-1's MST page; JIANT has none of them as such). VOLUME scales
 * JIANT's master gain after its leveler (fx.c mix_block's mg: before it, the leveler would ride it back up); AGE and
 * CRUSH work on the mix, the reverb's return in it, after the DJ filter and the clipper (fx.c, xn_master):
 *   AGE    tape: darker (a pole from open down to ~2.5 kHz), wow (0.6 Hz) and flutter (7 Hz) from a short line
 *   CRUSH  16 - CRUSH / 10 bits (6 to 15; under 6: CLEAN), the rate held down past 40 (to a fifth)
 * The mix there is Q15 at full MASTER (master_out's scale); the tape's line holds it as 16 bits */
static int32_t xn_mvol_q12 = 4096;                /* (xs_mix: VOLUME, square law, 100 % as 0 dB) */
static uint8_t xn_age, xn_crush;
#define XN_WOW 256u
static int16_t xn_wow[2][XN_WOW];
static uint32_t xn_wow_w, xn_wow_ph, xn_flut_ph, xn_hold_n;
static int32_t xn_lp[2], xn_hold[2];
static void xn_master(int32_t *l, int32_t *r, const int32_t *wet, uint32_t n)
{
    uint32_t i, c, bits = 16u - xn_crush / 10u, sh = xn_crush >= 6u ? 16u - bits : 0u;
    uint32_t hold = xn_crush > 40u ? 1u + (xn_crush - 40u) / 15u : 1u;
    int32_t a = 32768 - (int32_t)xn_age * 228, dep = (int32_t)xn_age * 77;   /* (the pole, Q15; wow's depth, Q8 samples) */
    if (!xn_age && !sh && hold == 1u)
        return;
    for (i = 0; i < n; i++) {
        int32_t x[2];
        x[0] = l[i] + wet[i];
        x[1] = r[i] + wet[i];
        if (xn_age) {                               /* the tape: through the line, read wavering */
            int32_t d = 32 * 256 + ((dep * sine_i(xn_wow_ph)) >> 15) + ((dep / 8 * sine_i(xn_flut_ph)) >> 15);
            uint32_t p0 = xn_wow_w - (uint32_t)(d >> 8), f = (uint32_t)d & 255u;
            xn_wow_ph += 58434u;                    /* (0.6 Hz) */
            xn_flut_ph += 681725u;                  /* (7 Hz) */
            for (c = 0; c < 2u; c++) {
                int32_t y0, y1;
                xn_wow[c][xn_wow_w % XN_WOW] = (int16_t)clamp(x[c], -32768, 32767);
                y0 = xn_wow[c][p0 % XN_WOW];
                y1 = xn_wow[c][(p0 - 1u) % XN_WOW];
                x[c] = y0 + (((y1 - y0) * (int32_t)f) >> 8);
                xn_lp[c] += ((clamp(x[c] - xn_lp[c], -131072, 131071) >> 2) * a) >> 13;   /* (no 64 bits) */
                x[c] = xn_lp[c];
            }
            xn_wow_w++;
        }
        if (hold > 1u) {                            /* the rate held */
            if (xn_hold_n == 0u) { xn_hold[0] = x[0]; xn_hold[1] = x[1]; }
            xn_hold_n = (xn_hold_n + 1u) % hold;
            x[0] = xn_hold[0];
            x[1] = xn_hold[1];
        }
        if (sh) {                                   /* the bits (of the mix's full scale, Q15: master_out) */
            x[0] = (x[0] >> sh) << sh;
            x[1] = (x[1] >> sh) << sh;
        }
        l[i] = x[0] - wet[i];                       /* (the return stays where fx.c adds it) */
        r[i] = x[1] - wet[i];
    }
}

static int xj_fm6_elsewhere(uint32_t jt)          /* FM6 on another track already (one at most: its CPU) */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) if (k != jt && trk_eng(&trk[k]) == ENGI_FM6) return 1;
    return 0;
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
/* WND-1's 27 synth sounds on JIANT's: each three presets of its character (a song takes one of them, by its name: the
 * same song, the same sounds), its filter (127 open; EDIT's BRIGHT moves it from there) and glide. FM PLUCK off the
 * bass track takes XS_MEL_FMPLUCK's. FM6 on one track at most (its CPU): the next choice then. trim: dB to one level (tests/wb_test.c WB_CAL: a note
 * of its role, 1 s, to 0.05 RMS; -7 .. +6) */
typedef struct { uint8_t eng[3]; const char *word[3]; uint8_t cut, glide; int8_t trim[3]; } xs_patch_t;
#define XA ENGI_ANALOG
#define XPH ENGI_PHASE
#define XF6 ENGI_FM6
#define XWH ENGI_WHEEL
#define XVO ENGI_VOICE
#define XLO ENGI_LOFI
static const xs_patch_t XS_PATCH[WB_NSYN] = {
    /* SUB BASS    */ {{XA, XLO, XA}, {"SUB BASS", "TRI BASS", "FAT BASS"}, 70, 0, {-4, 2, 4}},
    /* FM PLUCK    */ {{XF6, XPH, XA}, {"FM BASS", "SYN BASS", "SQR BASS"}, 84, 0, {6, 0, 2}},
    /* DUB BASS    */ {{XA, XPH, XA}, {"FAT BASS", "FB BASS", "SQR BASS"}, 62, 0, {4, 0, 2}},
    /* TAPE EP     */ {{XF6, XPH, XWH}, {"TINE EP", "CZ PIANO", "SOFT KEYS"}, 92, 0, {-2, 0, -3}},
    /* DETUNE PAD  */ {{XA, XA, XLO}, {"JUNO PAD", "SOFT PAD", "FLOAT PAD"}, 78, 0, {-2, -3, 6}},
    /* WARM KEYS   */ {{XPH, XWH, XA}, {"CZ PIANO", "SOFT KEYS", "SINE KEY"}, 86, 0, {0, -3, 2}},
    /* GLASS BELL  */ {{XF6, XA, XF6}, {"SOFT BELL", "GLASS KEYS", "ICE BELL"}, 88, 0, {5, 3, 5}},
    /* MUSIC BOX   */ {{XF6, XPH, XLO}, {"MARIMBA", "HARP", "SOFT CHIP"}, 94, 0, {6, 2, 0}},
    /* ORGAN       */ {{XWH, XWH, XWH}, {"JAZZ PERC", "DUB ORGAN", "SOFT KEYS"}, 100, 0, {-2, -3, -4}},
    /* BRASSY      */ {{XA, XPH, XF6}, {"BRASS", "BRASS", "BRASS"}, 80, 0, {-2, -5, 3}},
    /* SUPERSAW    */ {{XA, XA, XA}, {"SAW LEAD", "PWM STR", "STRINGS"}, 84, 0, {0, -3, -3}},
    /* RAVE STAB   */ {{XA, XA, XPH}, {"RAVE", "DUB STAB", "DIGI STAB"}, 84, 0, {-1, 6, 5}},
    /* HOOVER      */ {{XA, XA, XA}, {"HOOVER", "RAVE", "REESE"}, 80, 0, {-1, -1, 5}},
    /* HYPER LEAD  */ {{XA, XA, XPH}, {"SYNC LEAD", "MOOG LEAD", "PD LEAD"}, 84, 0, {-1, -1, -1}},
    /* SQUARE LEAD */ {{XLO, XLO, XA}, {"PULSE LD", "WAVE LEAD", "MOOG LEAD"}, 84, 0, {3, 6, -2}},
    /* SYNTH PLUCK */ {{XA, XA, XF6}, {"PLUCK", "ARP PLUCK", "PLUCK"}, 90, 0, {6, 6, 6}},
    /* PWM PAD     */ {{XA, XA, XPH}, {"PWM STR", "DARK PAD", "GLASS PAD"}, 78, 0, {-3, -2, -2}},
    /* STRINGS     */ {{XA, XPH, XA}, {"STRINGS", "STRING", "CELLO"}, 84, 0, {-3, -4, -2}},
    /* ACID        */ {{XA, XA, XA}, {"ACID", "ACID", "SQR BASS"}, 127, 0, {6, 6, 2}},
    /* REESE       */ {{XA, XPH, XA}, {"REESE", "GROWL", "FAT BASS"}, 72, 0, {2, -2, 4}},
    /* 808 GLIDE   */ {{XA, XLO, XA}, {"SUB BASS", "TRI BASS", "SQR BASS"}, 72, 30, {-4, 2, 2}},
    /* FUNK BASS   */ {{XPH, XA, XF6}, {"FB BASS", "SQR BASS", "FM BASS"}, 86, 0, {0, 2, 6}},
    /* VOX CHOP    */ {{XVO, XVO, XVO}, {"TALK BOX", "CHANT", "OOH PAD"}, 96, 0, {3, -7, -4}},
    /* VOX AH      */ {{XVO, XVO, XPH}, {"CHOIR AAH", "OOH PAD", "VOX PAD"}, 90, 0, {-4, -4, -3}},
    /* HYPER VOX   */ {{XVO, XVO, XVO}, {"VOX LEAD", "CANTORA ES", "TALK BOX"}, 90, 0, {-2, 0, 3}},
    /* TALK LEAD   */ {{XVO, XVO, XVO}, {"TALK BOX", "ROBOT EN", "VOX LEAD"}, 92, 0, {3, 1, -2}},
    /* CHOIR       */ {{XVO, XVO, XPH}, {"OOH PAD", "CHOIR AAH", "VOX PAD"}, 88, 0, {-4, -4, -3}}};
static const xs_patch_t XS_MEL_FMPLUCK = {{XF6, XPH, XA}, {"FM PLUCK", "HARP", "ARP PLUCK"}, 90, 0, {4, 2, 6}};
#undef XA
#undef XPH
#undef XF6
#undef XWH
#undef XVO
#undef XLO
static uint32_t xs_sig[8];
/* the mix (dB on top of VOL): the drums up (WND-1's own mix had them 2-3x the synths' gain), the keys and the synth under
 * the bass; each JIANT track's sound's trim (XS_PATCH, the kits') */
#define XS_DB_DRUMS 2
static const int8_t XS_DB[6] = {2, 2, 2, 0, -3, -4};
static int8_t xs_trim[NTRK];
static const char *xs_word[NTRK];                 /* each JIANT track's sound (XS_PATCH's word) */
static const int8_t XS_KIT_TRIM[WB_NKICK] = {0, 0, 0, 6, 0, 3, 0, 2, 0};   /* (the kits' kicks: LOFI DUST 6 dB under) */
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
    uint32_t k = wt < NT && song.trk[wt].patch < WB_NSYN && p == song.trk[wt].p ? song.trk[wt].patch : xs_patch_of(p);
    static uint16_t loaded[NTRK] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    const xs_patch_t *x = k == 1u && jt != XJ_BAS ? &XS_MEL_FMPLUCK : &XS_PATCH[k];
    uint32_t v = (xs_hash(song.name, sizeof song.name) >> (jt * 3u)) % 3u, i, id;
    for (i = 0; i < 3u && ((x->eng[v] == ENGI_FM6 && xj_fm6_elsewhere(jt)) ||   /* (FM6 once; the synth and the counter */
                           (jt != XJ_KEY && jt != XJ_BAS && xs_word[XJ_KEY] == x->word[v])); i++)   /* not KEY's sound) */
        v = (v + 1u) % 3u;
    xs_word[jt] = x->word[v];
    id = k << 2 | v;
    if (loaded[jt] != id) {                         /* another sound: its preset (a load releases what sounds) */
        loaded[jt] = (uint16_t)id;
        xj_sound(jt, x->eng[v], x->word[v]);
    }
    xs_trim[jt] = x->trim[v];
    xj_env(jt, p[PY_ATT], p[PY_DEC], p[PY_SUS], p[PY_REL]);   /* (the knobs: the envelope only, as it plays) */
    xj_tone(jt, x->cut + (p[PY_INDEX] - WB_SYN[k][PY_INDEX]) * 3 / 2, x->glide);   /* EDIT's BRIGHT: the filter */
    (void)wt;
}
/* the drums: JIANT's DRUM tracks share one kit (the section's, eng_drum.c), the kick's sound picks it; each track its
 * own TUNE TONE DECAY FM ENV DRIVE on it (E1 E2 E3 E5 E7: 64 as the kit has them): the break's from BRK's PITCH TONE
 * GATE (a little driven: a record's), the kit's from KIK's TUNE DECAY PUNCH DRIVE and the snare's TONE */
static const char *const XS_KIT[WB_NKICK] = {"909 CLUB", "808 BOOM", "INDUSTRIAL", "LOFI DUST", "808 BOOM", "TECHNO RAW",
                                              "INDUSTRIAL", "ACOUSTIC", "909 CLUB"};
static void xs_drums(void)
{
    static uint8_t kit = 0xFF;
    const int16_t *k = song.trk[T_KIK].p, *d = song.trk[T_DRM].p, *b = song.trk[T_BRK].p;
    uint32_t want = song.trk[T_KIK].patch % WB_NKICK;
    if (want != kit) {                              /* the kit: both tracks (the second load keeps the first's kit) */
        kit = (uint8_t)want;
        xs_trim[XJ_DRM] = xs_trim[XJ_BRK] = XS_KIT_TRIM[want];
        xj_sound(XJ_DRM, ENGI_DRUM, XS_KIT[want]);
        xj_sound(XJ_BRK, ENGI_DRUM, XS_KIT[want]);
    }
    xj_e(XJ_DRM, 1, 64 + k[PK_TUNE] * 64 / 12);
    xj_e(XJ_DRM, 2, 32 + d[PS_TONE] * 64 / 100);
    xj_e(XJ_DRM, 3, 64 + (k[PK_DECAY] - 600) * 64 / 900);
    xj_e(XJ_DRM, 5, k[PK_PUNCH] * 64 / 100);
    xj_e(XJ_DRM, 7, k[PK_DRIVE] * 100 / 100);
    xj_e(XJ_BRK, 1, 64 + b[PB_PITCH] * 64 / 12);
    xj_e(XJ_BRK, 2, 24 + b[PB_TONE] * 80 / 100);
    xj_e(XJ_BRK, 3, b[PB_GATE] * 64 / 100);
    xj_e(XJ_BRK, 7, 28);
}
static void xs_sound(void)
{
    uint32_t t, h, drums = 0;
    for (t = 0; t < NT; t++) if (wb_resound >> t & 1u) xs_sig[t] = 0;   /* (LFO RESET: the sound's own LFO again) */
    wb_resound = 0;
    for (t = 0; t < NT; t++) {
        const track_t *s = &song.trk[t];
        h = xs_hash(s->p, sizeof s->p) ^ s->patch * 7919u ^ s->patch2 * 104729u;
        if (h == xs_sig[t]) continue;
        xs_sig[t] = h;
        switch (TRK_TYPE[t]) {
        case TY_BREAK: case TY_KICK: case TY_DRUMS: drums = 1; break;
        default:
            xs_synth(t, t == T_BAS ? XJ_BAS : t == T_KEY ? XJ_KEY : XJ_SYN, s->p);
            if (t == T_KEY) { xs_synth(t, XJ_CTR, WB_SYN[clampi(s->p[PY_CTRP], 0, WB_NSYN - 1)]); xs_sig[T_SYN] = 0; }
        }
    }
    if (drums) xs_drums();
}
static void xs_mix(void)
{
    static uint32_t sig;
    uint32_t t, h = xs_hash(song.trk, sizeof song.trk) ^ xs_hash(xs_trim, sizeof xs_trim) ^ song.bpm ^ song.space << 8 ^ song.pump << 16 ^
                    (uint32_t)song.mvol << 24 ^ song.age * 40009u ^ song.crush * 7001u ^ (uint32_t)(uint8_t)fx_flt * 92821u ^
                    fx_chop * 68917u ^ fx_step * 5003u ^ fx_wash * 3301u ^ punch_lpf * 77u ^ punch_dub * 131u ^ punch_wash * 997u;
    static const uint8_t JT[NT] = {XJ_BRK, XJ_DRM, XJ_DRM, XJ_BAS, XJ_KEY, XJ_SYN};
    if (h == sig) return;
    sig = h;
    for (t = 0; t < NT; t++) {
        const track_t *s = &song.trk[t];
        if (t == T_KIK) continue;
        if (t == T_DRM) {                           /* KIK + PER: one JIANT track, the louder one's level, the lanes */
            int32_t vk = song.trk[T_KIK].vol, vp = s->vol, top = vk > vp ? vk : vp;
            uint32_t l;
            xj_lane(0, vk, top);
            for (l = 1; l < 8u; l++) xj_lane(l, vp, top);
            xj_mix(XJ_DRM, top, XS_DB_DRUMS + xs_trim[XJ_DRM], s->pan, s->rev, s->dly);
            continue;
        }
        xj_mix(JT[t], s->vol, XS_DB[t] + xs_trim[JT[t]], s->pan, s->rev, s->dly);
        if (t == T_KEY) xj_mix(XJ_CTR, s->vol, XS_DB[t] + xs_trim[XJ_CTR], s->pan, s->rev, s->dly);
    }
    xj_global(G_BPM, song.bpm ? song.bpm : 100);
    xj_global(G_RSIZE, 40 + song.space * 87 / 100 + (punch_wash ? 40 : 0) > 127 ? 127 : 40 + song.space * 87 / 100 + (punch_wash ? 40 : 0));
    xj_global(G_DFDBK, punch_dub ? 105 : 60);
    xj_global(G_DUCK, song.pump);
    xj_global(G_DJF, punch_lpf ? -44 : 0);
    if (fx_flt && !punch_lpf) xj_global(G_DJF, fx_flt);   /* (XENOISE) PERFORM's knobs: the DJ filter, */
    xj_global(G_SLMODE, !fx_chop ? 0 : fx_chop <= 16u ? 1 : 2);   /* the chop (the slicer: GATE, STUT), its step, */
    if (fx_chop) {
        uint32_t k;
        xj_global(G_SLPAT, (int32_t)((fx_chop - 1u) % 16u + 1u));
        xj_global(G_SLRATE, fx_step);
        xj_global(G_SLDEP, 127);
        xj_global(G_SLFX, 1);
        for (k = 0; k < 6u; k++) xj_global(G_SLT1 + k, 1);
    }
    if (fx_wash) {                              /* the wash: the room larger, the echoes longer */
        int32_t rs = 40 + song.space * 87 / 100 + fx_wash * 60 / 100;
        xj_global(G_RSIZE, rs > 127 ? 127 : rs);
        xj_global(G_DFDBK, (punch_dub ? 105 : 60) + fx_wash * 40 / 100 > 118 ? 118 : (punch_dub ? 105 : 60) + fx_wash * 40 / 100);
    }
    xn_mvol_q12 = 4096 * song.mvol * song.mvol / 10000;   /* (the master: xn_master) */
    xn_age = song.age;
    xn_crush = song.crush;
}

/* (XENOISE) the song's LFO and ARP (its extension: the LFO and ARP pages) on JIANT's tracks, every frame: what is set
 * holds over the sound's own (a preset loaded with a new sound would bring its own back). KIK and PER share a JIANT
 * track: PER's, else KIK's */
static void xs_ext(void)
{
    static const uint8_t JT[NT] = {XJ_BRK, XJ_DRM, XJ_DRM, XJ_BAS, XJ_KEY, XJ_SYN};
    uint32_t t;
    for (t = 0; t < NT; t++) {
        const wb_lfo_t *l = &song.x.lfo[t];
        const wb_arp_t *r = &song.x.arp[t];
        if (t == T_KIK) continue;               /* (KIK's: on PER's turn, the track they share) */
        if (t == T_DRM) {
            if (!l->set) l = &song.x.lfo[T_KIK];
            if (!r->set) r = &song.x.arp[T_KIK];
        }
        if (l->set) { int8_t v[7] = {l->rate, l->wave, l->fade, l->pit, l->flt, l->shp, l->amp}; xj_lfo_write(JT[t], v); }
        {
            uint8_t a[8] = {r->mode, r->rate, r->oct, r->gate, r->swing, r->prob, r->hold, r->order};
            xj_arp_write(JT[t], r->set && r->mode, a);
        }
    }
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
    xs_ext();
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
