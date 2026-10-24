/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XANOISE) */
/* XANOISE: the Wonderblue tracker over JIANT FM. When it is on (xn_on), it draws the screen and takes the panel; JIANT's sequencer, engines, effects and storage run as ever under it,
 * and the grid shows and edits their own data: the six tracks' 64 steps (core.h step_t).
 *
 *   SELECT     the cursor's step            ALGORITHM  the track            PLAY  play / stop    REC  the keys write
 *   KNOB 1..4  a note step: NOTE, VEL, TIME (NOTE TIE REST), FX (ACCENT SLIDE RATCH x2..x4)
 *              a DRUM-X step: LANE, HIT, VEL, ACCENT (the lane's)
 *   WHITE      play the selected track (JIANT plays them); with REC, written at the cursor (playing: on the playhead)
 *   BLACK 1-6  the track                    BLACK 8-11  F1 CLEAR, F2 REST, F3 TIE, F4 LENGTH 16 / 32 / 64
 *   HOME held + SELECT  GEN (JIANT's song generator, with Wonderblue's genres); done there: back here
 *   FX SCL ENV LFO EDIT GLO SAVE ARP SEQ  JIANT's own page for the button; HOME there: back to the tracker
 * The look is Wonderblue's: Renoise's warm greys, orange for what plays, blue for what is picked. */

#ifndef XN_HOME_BACK
#define XN_HOME_BACK 1                                  /* HOME on JIANT's pages: back to the tracker (the tests: 0) */
#endif

static uint8_t xn_row, xn_lane, xn_rec, xn_force = 1;
static uint8_t xn_last[NTRK] = {36, 60, 60, 48, 60, 60};
static char xn_msg[32];
static uint32_t xn_msg_ms, xn_flash_ms[NTRK];
static uint8_t xn_flash_idx[NTRK];

#define XC_BG RGB(24, 22, 20)
#define XC_ROW RGB(34, 31, 28)
#define XC_BEAT RGB(46, 41, 36)
#define XC_PANEL RGB(60, 58, 54)
#define XC_INSET RGB(30, 29, 27)
#define XC_TXT RGB(226, 218, 202)
#define XC_NOTE RGB(222, 190, 140)
#define XC_MUTED RGB(142, 132, 118)
#define XC_FAINT RGB(74, 67, 60)
#define XC_ORANGE RGB(242, 164, 42)
#define XC_BLUE RGB(52, 116, 232)
#define XC_CYAN RGB(52, 214, 204)
#define XC_RED RGB(232, 82, 66)
static const uint16_t XN_COL[NTRK] = {RGB(238, 142, 42), RGB(52, 116, 232), RGB(96, 156, 244), RGB(52, 214, 204),
                                     RGB(228, 192, 122), RGB(214, 214, 214)};
static const uint16_t XN_KCOL[4] = {RGB(240, 92, 80), RGB(244, 196, 64), RGB(96, 214, 120), RGB(84, 152, 244)};
static const char *const XN_LANE[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};

static void xn_cat(char *d, const char *s) { d += str_len(d); while (*s) *d++ = *s++; *d = 0; }
static void xn_say(const char *s)
{
    str_cpy(xn_msg, s, sizeof xn_msg);
    xn_msg_ms = fm1_ms | 1u;
}
static uint32_t xn_len(const track_t *t) { int32_t l = t->p[P_SLEN]; return l >= 1 && l <= (int32_t)NSTEP ? (uint32_t)l : 16u; }
static uint32_t xn_play_idx(uint32_t ti) { return trk[ti].seq_idx % xn_len(&trk[ti]); }
static int xn_drum(const track_t *t) { return drum_track(t); }
static uint32_t xn_bit(uint32_t b) { return 1u << panel.btn[b]; }

/* ---- a step's text: "C-4", "BD+", "~~", "--" */
static void xn_note_name(uint32_t n, char *o)
{
    static const char NN[] = "C-C#D-D#E-F-F#G-G#A-A#B-";
    int32_t oc = (int32_t)n / 12 - 1;
    o[0] = NN[(n % 12u) * 2u];
    o[1] = NN[(n % 12u) * 2u + 1u];
    o[2] = (char)(oc < 0 ? '-' : '0' + (oc > 9 ? 9 : oc));
    o[3] = 0;
}
static uint32_t xn_hits(const step_t *s) { uint32_t n = 0, l; for (l = 0; l < NLANE; l++) n += (s->hit >> l) & 1u; return n; }
static void xn_cell(const track_t *t, const step_t *s, char *o, int wide)
{
    o[0] = 0;
    if (xn_drum(t)) {
        uint32_t l, n = 0;
        if (!s->hit && !s->n) { str_cpy(o, wide ? "--" : ".", 8); return; }
        for (l = 0; l < NLANE && n < (wide ? 2u : 1u); l++)
            if ((s->hit >> l) & 1u) {
                if (n) xn_cat(o, " ");
                xn_cat(o, XN_LANE[l]);
                n++;
            }
        if (!n) str_cpy(o, "HIT", 8);
        if (xn_hits(s) > n) xn_cat(o, "+");
        return;
    }
    if (s->time == ST_TIE) { str_cpy(o, "~~", 8); return; }
    if (s->time == ST_REST || !s->n) { str_cpy(o, wide ? "--" : ".", 8); return; }
    xn_note_name(s->note[0], o);
    if (s->n > 1u) xn_cat(o, "+");
}

/* ================================================================ drawing: two strips of 240 x 120 */
#define XN_TOP 20
#define XN_TAB 35
#define XN_HEAD 35
#define XN_GRID 51
#define XN_RH 14
#define XN_NVIS 9
#define XN_INFO 178
#define XN_KNOB 194
#define XN_GUT 18
#define XN_FOC 70
#define XN_NAR 30
static int32_t xn_y0;                            /* the strip's first screen row */
static void xnR(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { cv_rect(x, y - xn_y0, w, h, c); }
static void xnT(int32_t x, int32_t y, const char *s, uint16_t c, uint16_t bg) { cv_text_on(x, y - xn_y0 - 1, &AF_S, s, c, bg); }
static void xnTC(int32_t cx, int32_t y, const char *s, uint16_t c, uint16_t bg) { xnT(cx - text_w(&AF_S, s) / 2, y, s, c, bg); }
static void xnTR(int32_t xr, int32_t y, const char *s, uint16_t c, uint16_t bg) { xnT(xr - text_w(&AF_S, s), y, s, c, bg); }
static int32_t xn_lane_x(uint32_t t)
{
    int32_t x = XN_GUT;
    uint32_t k;
    for (k = 0; k < t; k++) x += k == song.sel ? XN_FOC : XN_NAR;
    return x;
}
static int32_t xn_lane_w(uint32_t t) { return t == song.sel ? XN_FOC : XN_NAR; }
static void xn_hex2(char *o, uint32_t v) { static const char H[] = "0123456789ABCDEF"; o[0] = H[(v >> 4) & 15u]; o[1] = H[v & 15u]; o[2] = 0; }

static void xn_draw_top(void)
{
    char s[24];
    xnR(0, 0, 240, XN_TOP, XC_PANEL);
    xnR(3, 3, 30, 14, XC_INSET);
    xnTC(18, 3, "TRK", XC_ORANGE, XC_INSET);
    project_cur_name(s);
    if (!s[0]) str_cpy(s, "XANOISE", sizeof s);
    xnT(39, 3, s, XC_TXT, XC_PANEL);
    xnR(181, 3, 30, 14, XC_INSET);
    s[0] = 0;
    fmt_int(s, song.g[G_BPM]);
    xnTC(196, 3, s, XC_TXT, XC_INSET);
    if (song.playing) { int32_t k; for (k = 0; k < 5; k++) xnR(217 + k * 2, 6 + k, 2, 9 - 2 * k, XC_ORANGE); }
    else xnR(217, 6, 8, 8, XC_MUTED);
    if (xn_rec) xnR(229, 6, 8, 8, XC_RED);
    xnR(0, XN_TAB - 15, 240, 15, XC_BG);         /* the tabs: the tracker, and how to reach JIANT's pages */
    xnR(0, XN_TAB - 14, 44, 13, XC_ORANGE);
    xnTC(22, XN_TAB - 14, "TRK", XC_BG, XC_ORANGE);
    xnTR(237, XN_TAB - 14, "HOME+SEL: GEN", XC_MUTED, XC_BG);
}
static void xn_draw_heads(void)
{
    uint32_t t;
    xnR(0, XN_HEAD, 240, XN_GRID - XN_HEAD, XC_BG);
    for (t = 0; t < NTRK; t++) {
        int32_t x = xn_lane_x(t), w = xn_lane_w(t);
        const track_t *tr = &trk[t];
        int fl = fm1_ms - xn_flash_ms[t] < 90u, off = tr->p[P_MUTE] || t >= trk_live, sel = t == song.sel;
        uint16_t c = off ? XC_INSET : sel ? XN_COL[t] : ux_mix(XC_BG, XN_COL[t], 43), ink;
        char s[16];
        if (fl && !off) c = ux_mix(XN_COL[t], RGB(255, 255, 255), 58);
        ink = off ? XC_MUTED : (sel || fl) ? XC_BG : ux_mix(XN_COL[t], RGB(255, 255, 255), 66);
        xnR(x, XN_HEAD + 1, w - 2, 14, c);
        s[0] = (char)('1' + t);
        s[1] = 0;
        if (sel) { xn_cat(s, " "); xn_cat(s, ENGINES[trk_eng(tr)]->name); }
        xnT(x + 3, XN_HEAD + 1, s, ink, c);
    }
}
static void xn_draw_grid(void)
{
    const track_t *ts = &trk[song.sel];
    uint32_t L = xn_len(ts), t, j;
    int32_t center = song.playing ? (int32_t)xn_play_idx(song.sel) : xn_row;
    int32_t top = center - XN_NVIS / 2;
    char s[16];
    if (top > (int32_t)L - XN_NVIS) top = (int32_t)L - XN_NVIS;
    if (top < 0) top = 0;
    for (j = 0; j < XN_NVIS; j++) {
        int32_t r = top + (int32_t)j, y = XN_GRID + (int32_t)j * XN_RH;
        int cur = r == xn_row;
        uint16_t bg = (r & 3) == 0 ? XC_BEAT : XC_ROW;
        if (y + XN_RH <= xn_y0 || y >= xn_y0 + 120) continue;
        if (r >= (int32_t)NSTEP) { xnR(0, y, 240, XN_RH, XC_BG); continue; }
        xnR(0, y, 240, XN_RH, bg);
        xn_hex2(s, (uint32_t)r);
        xnT(2, y + 1, s, cur ? XC_BLUE : (r & 3) == 0 ? XC_TXT : XC_MUTED, bg);
        for (t = 0; t < NTRK; t++) {
            const track_t *tr = &trk[t];
            int32_t x = xn_lane_x(t), w = xn_lane_w(t);
            uint32_t len = xn_len(tr);
            int sel = t == song.sel, play = song.playing && (uint32_t)r == xn_play_idx(t) && t < trk_live;
            uint16_t cb = play ? XC_ORANGE : sel ? ux_mix(bg, XC_TXT, 5) : bg;
            const step_t *st;
            if ((uint32_t)r >= len) { xnR(x, y, w - 2, XN_RH, XC_BG); continue; }   /* (past this track's length) */
            st = &tr->step[r];
            xnR(x, y, w - 2, XN_RH, cb);
            xn_cell(tr, st, s, sel);
            if (sel) {
                int empty = s[0] == '-' || s[0] == '.';
                xnT(x + 3, y + 1, s, play ? XC_BG : empty ? XC_FAINT : XC_NOTE, cb);
                if (!empty && !xn_drum(tr)) {
                    char v[4];
                    xn_hex2(v, st->vel >> 3);
                    xnT(x + 34, y + 1, v + 1, play ? XC_BG : XC_MUTED, cb);
                    v[0] = (st->flags & SF_ACCENT) ? 'A' : (st->flags & SF_SLIDE) ? 'S' :
                           ((st->flags & SF_RATCH) >> SF_RATCH_SH) ? 'R' : 0;
                    v[1] = v[0] == 'R' ? (char)('1' + ((st->flags & SF_RATCH) >> SF_RATCH_SH)) : 0;
                    v[2] = 0;
                    if (v[0]) xnT(x + 50, y + 1, v, play ? XC_BG : XC_CYAN, cb);
                }
            } else {
                int empty = s[0] == '.';
                char n2[4] = {s[0], s[1] == '-' ? s[2] : s[1], 0, 0};
                if (!empty && !xn_drum(tr) && s[0] != '~' && n2[1] == 0) n2[1] = s[2];
                xnT(x + 5, y + 1, empty ? "." : n2, play ? XC_BG : empty ? XC_FAINT :
                  xn_drum(tr) ? ux_mix(XN_COL[t], XC_TXT, 43) : XC_NOTE, cb);
            }
        }
        if (cur) cv_frame(xn_lane_x(song.sel), y - xn_y0, XN_FOC - 2, XN_RH, XC_BLUE);
    }
}

/* the knobs' row: what KNOB 1..4 do on the step under the cursor */
static void xn_knob(uint32_t i, char *lab, char *val)
{
    const track_t *t = &trk[song.sel];
    const step_t *s = &t->step[xn_row % NSTEP];
    lab[0] = val[0] = 0;
    if (xn_drum(t)) {
        static const char *const L[4] = {"LANE", "HIT", "VEL", "ACCENT"};
        str_cpy(lab, L[i], 12);
        if (i == 0u) str_cpy(val, XN_LANE[xn_lane % NLANE], 12);
        else if (i == 1u) str_cpy(val, (s->hit >> xn_lane) & 1u ? "ON" : "OFF", 12);
        else if (i == 2u) fmt_int(val, s->vel);
        else str_cpy(val, (s->acc >> xn_lane) & 1u ? "ON" : "OFF", 12);
        return;
    }
    {
        static const char *const L[4] = {"NOTE", "VEL", "TIME", "FX"};
        str_cpy(lab, L[i], 12);
    }
    if (i == 0u) { if (s->n && s->time == ST_NOTE) xn_note_name(s->note[0], val); else str_cpy(val, "--", 12); }
    else if (i == 1u) fmt_int(val, s->vel);
    else if (i == 2u) str_cpy(val, s->time == ST_TIE ? "TIE" : s->time == ST_REST || !s->n ? "REST" : "NOTE", 12);
    else {
        uint32_t r = (s->flags & SF_RATCH) >> SF_RATCH_SH;
        if (r) { str_cpy(val, "RATCH x", 12); val[7] = (char)('1' + r); val[8] = 0; }
        else str_cpy(val, (s->flags & SF_ACCENT) ? "ACCENT" : (s->flags & SF_SLIDE) ? "SLIDE" : "--", 12);
    }
}
static void xn_draw_foot(void)
{
    static const char *const F[4] = {"CLEAR", "REST", "TIE", "LENGTH"};
    uint32_t i;
    char lab[16], val[16];
    xnR(0, XN_INFO, 240, XN_KNOB - XN_INFO, XC_BG);
    xnR(0, XN_INFO, 240, 1, XC_FAINT);
    if (xn_msg_ms && fm1_ms - xn_msg_ms < 1800u) xnT(5, XN_INFO + 2, xn_msg, XC_ORANGE, XC_BG);
    else {
        char s[24];
        str_cpy(s, ENGINES[trk_eng(&trk[song.sel])]->name, sizeof s);
        xnT(5, XN_INFO + 2, s, XC_MUTED, XC_BG);
        str_cpy(s, "LEN ", sizeof s);
        fmt_int(s + 4, (int32_t)xn_len(&trk[song.sel]));
        xnTR(236, XN_INFO + 2, s, XC_CYAN, XC_BG);
    }
    xnR(0, XN_KNOB, 240, 240 - XN_KNOB, XC_PANEL);
    for (i = 0; i < 4u; i++) {
        int32_t x = (int32_t)i * 60;
        xn_knob(i, lab, val);
        xnR(x + 3, XN_KNOB + 4, 3, 8, XN_KCOL[i]);
        xnT(x + 9, XN_KNOB + 1, lab, XC_TXT, XC_PANEL);
        xnR(x + 2, XN_KNOB + 15, 56, 13, XC_INSET);
        xnT(x + 5, XN_KNOB + 15, val, XC_NOTE, XC_INSET);
        xnR(x + 2, XN_KNOB + 31, 56, 13, RGB(86, 82, 76));
        xnTC(x + 30, XN_KNOB + 31, i == 3u ? "LENGTH" : F[i], XC_TXT, RGB(86, 82, 76));
    }
}
static void xn_draw(void)
{
    uint32_t b;
    dc_frame(xn_force || ui.force);
    xn_force = 0;
    ui.force = 0;
    for (b = 0; b < 2u; b++) {
        xn_y0 = (int32_t)(b * 120u);
        cv_begin(240, 120, XC_BG);
        xn_draw_top();
        xn_draw_heads();
        xn_draw_grid();
        xn_draw_foot();
        cv_blit_cells(0, (uint32_t)xn_y0);
    }
}

/* ================================================================ the LEDs */
static void xn_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c, b = 0, w = 0, ps = xn_play_idx(song.sel);
    if (fm1_led_anim_on())
        return;
    for (k = 0; k < NB; k++) led_put(nd, panel.btn[k], 1);
    led_put(nl, panel.btn[B_HOME], 1);
    led_put(nl, panel.btn[B_PLAY], song.playing);
    led_put(nl, panel.btn[B_REC], xn_rec);
    for (k = 0; k < 27u; k++) {
        if (key_black(k)) {                     /* black: the tracks (the selected lit), F1..F4 */
            if (b < NTRK) led_put(b == song.sel ? nl : nd, 14u + k, b == song.sel || !trk[b].p[P_MUTE]);
            else if (b >= 7u) led_put(nd, 14u + k, 1);
            b++;
        } else {                                /* white: held, or the playhead's step in its bar */
            if ((fm1_in.notes >> k) & 1u) led_put(nl, 14u + k, 1);
            else if (song.playing && ps % 16u == w) led_put(nd, 14u + k, 1);
            w++;
        }
    }
    if (song.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led_hi[c] = 0;
        fm1_led_mid[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* ================================================================ input */
static void xn_step_note(track_t *t, uint32_t i, uint32_t note, uint32_t vel)
{
    step_t *s = &t->step[i % NSTEP];
    s->note[0] = (uint8_t)note;
    s->n = 1;
    s->time = ST_NOTE;
    s->vel = (uint8_t)vel;
}
static void xn_knob_turn(uint32_t i, int32_t d)
{
    track_t *t = &trk[song.sel];
    step_t *s = &t->step[xn_row % NSTEP];
    if (xn_drum(t)) {
        if (i == 0u) xn_lane = (uint8_t)((xn_lane + NLANE + (d > 0 ? 1u : NLANE - 1u)) % NLANE);
        else if (i == 1u) { s->hit = (uint8_t)(d > 0 ? s->hit | (1u << xn_lane) : s->hit & ~(1u << xn_lane)); if (!s->vel) s->vel = 100; }
        else if (i == 2u) s->vel = (uint8_t)clamp(s->vel + d * 4, 1, 127);
        else s->acc = (uint8_t)(d > 0 ? s->acc | (1u << xn_lane) : s->acc & ~(1u << xn_lane));
        return;
    }
    if (i == 0u) {
        int32_t n = s->n && s->time == ST_NOTE ? s->note[0] : xn_last[song.sel];
        n = clamp(n + d, 0, 127);
        xn_step_note(t, xn_row, (uint32_t)n, s->vel ? s->vel : 100u);
        xn_last[song.sel] = (uint8_t)n;
    } else if (i == 1u) {
        if (!s->n) xn_step_note(t, xn_row, xn_last[song.sel], 100);
        s->vel = (uint8_t)clamp(s->vel + d * 4, 1, 127);
    } else if (i == 2u) {
        int32_t m = (s->time == ST_TIE ? 1 : s->time == ST_REST || !s->n ? 2 : 0) + (d > 0 ? 1 : 2);
        m %= 3;
        if (m == 0) { if (!s->n) xn_step_note(t, xn_row, xn_last[song.sel], 100); s->time = ST_NOTE; }
        else s->time = (uint8_t)(m == 1 ? ST_TIE : ST_REST);
    } else {                                    /* FX: -- ACCENT SLIDE RATCH x2 x3 x4 */
        uint32_t r = (s->flags & SF_RATCH) >> SF_RATCH_SH;
        int32_t v = r ? 2 + (int32_t)r : (s->flags & SF_SLIDE) ? 2 : (s->flags & SF_ACCENT) ? 1 : 0;
        v = clamp(v + (d > 0 ? 1 : -1), 0, 5);
        s->flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE | SF_RATCH);
        if (v == 1) s->flags |= SF_ACCENT;
        else if (v == 2) s->flags |= SF_SLIDE;
        else if (v >= 3) s->flags |= (uint8_t)((uint32_t)(v - 2) << SF_RATCH_SH);
    }
}
static void xn_fn(uint32_t f)
{
    track_t *t = &trk[song.sel];
    step_t *s = &t->step[xn_row % NSTEP];
    if (f == 0u) { memset(s, 0, sizeof *s); xn_say("STEP CLEARED"); }
    else if (f == 1u) { s->time = ST_REST; s->hit = 0; xn_row = (uint8_t)((xn_row + 1u) % xn_len(t)); }
    else if (f == 2u) { s->time = ST_TIE; xn_row = (uint8_t)((xn_row + 1u) % xn_len(t)); }
    else {
        uint32_t L = xn_len(t), n = L < 16u ? 16u : L < 32u ? 32u : L < 64u ? 64u : 16u, k;
        for (k = L; k < n; k++) t->step[k] = t->step[k % L];   /* (longer: the steps so far, again) */
        t->p[P_SLEN] = (int16_t)n;
        if (xn_row >= n) xn_row = 0;
        {
            char m[20];
            str_cpy(m, "LENGTH ", sizeof m);
            fmt_int(m + 7, (int32_t)n);
            xn_say(m);
        }
    }
}
static void xn_key_rec(uint32_t k)              /* REC: a white key's note (or the kit's lane) into the step */
{
    track_t *t = &trk[song.sel];
    uint32_t note = kb_map(t, k), i = song.playing ? xn_play_idx(song.sel) : xn_row;
    if (note == KB_SILENT)
        return;
    if (xn_drum(t)) {
        uint32_t l;
        for (l = 0; l < NLANE; l++)
            if (DRUM_LANE_NOTE[l] == note) {
                t->step[i].hit |= (uint8_t)(1u << l);
                if (!t->step[i].vel) t->step[i].vel = 100;
                xn_lane = (uint8_t)l;
            }
    } else {
        xn_step_note(t, i, note, 100);
        xn_last[song.sel] = (uint8_t)note;
    }
    if (!song.playing)
        xn_row = (uint8_t)((xn_row + 1u) % xn_len(t));
}
/* the panel, while the tracker is on (ui_input.c ui_input_frame, after its upkeep); 1: taken */
static int xn_frame(uint32_t pressed, uint32_t notes)
{
    static uint8_t gen_back;                        /* GEN opened from here: back here when it is done */
    uint32_t fns = xn_bit(B_FX) | xn_bit(B_SCL) | xn_bit(B_ENV) | xn_bit(B_LFO) | xn_bit(B_EDIT) | xn_bit(B_GLO) |
                   xn_bit(B_SAVE) | xn_bit(B_ARP) | xn_bit(B_SEQ), k, b, w;
    int32_t d;
    if (!xn_on) {                                   /* JIANT's pages */
        if ((gen_back && !gen_on()) ||
            (XN_HOME_BACK && (pressed & xn_bit(B_HOME)) && !ui.menu && !ui.confirm && !name_on())) {
            gen_back = 0;
            xn_on = 1;
            xn_force = 1;
            ui.force = 1;
            return 1;
        }
        return 0;
    }
    if (ui.menu || dlg_on() || name_on())
        return 0;
    if (pressed & fns) {                            /* a function button: JIANT's page for it (it takes the press) */
        xn_on = 0;
        ui.force = 1;
        return 0;
    }
    if (fm1_in.buttons & xn_bit(B_HOME)) {          /* HOME held: SELECT opens GEN (as on JIANT's HOME) */
        if (panel_enc(EN_SELECT) != 0) {
            nav_go(SEC_HOME, 4u, 1);
            if (gen_on()) {
                xn_on = 0;
                gen_back = 1;
                ui.home_t0 = 3u;                    /* (HOME used: its release no tap, its hold no MENU; as JIANT's) */
            }
        }
        return 1;
    }
    if (pressed & xn_bit(B_PLAY)) transport_req = song.playing ? 2u : 1u;
    if (pressed & xn_bit(B_REC)) { xn_rec = (uint8_t)!xn_rec; xn_say(xn_rec ? "REC: THE KEYS WRITE" : "REC OFF"); }
    if ((pressed & xn_bit(B_OCTDN)) && !(fm1_in.buttons & xn_bit(B_OCTUP))) song.octave = (int8_t)(song.octave > -3 ? song.octave - 1 : -3);
    if ((pressed & xn_bit(B_OCTUP)) && !(fm1_in.buttons & xn_bit(B_OCTDN))) song.octave = (int8_t)(song.octave < 3 ? song.octave + 1 : 3);
    if ((d = panel_enc(EN_SELECT)) != 0) {
        uint32_t L = xn_len(&trk[song.sel]);
        xn_row = (uint8_t)(((int32_t)xn_row + d % (int32_t)L + (int32_t)L) % (int32_t)L);
    }
    if ((d = panel_enc(EN_ALGO)) != 0)
        track_select((song.sel + NTRK + (d > 0 ? 1u : NTRK - 1u)) % NTRK);
    (void)panel_enc(EN_PRESET);
    for (k = 0; k < 4u; k++)
        if ((d = panel_enc(EN_K1 + k)) != 0) xn_knob_turn(k, d);
    for (k = 0, b = 0, w = 0; k < 27u; k++) {
        int black = key_black(k);
        if ((notes >> k) & 1u) {
            if (black) {
                if (b < NTRK) track_select(b);
                else if (b >= 7u) xn_fn(b - 7u);
            } else if (xn_rec)
                xn_key_rec(k);
        }
        if (black) b++; else w++;
    }
    if (xn_row >= xn_len(&trk[song.sel])) xn_row = 0;
    for (k = 0; k < NTRK; k++) {                /* a track's step that sounds now: its head lights */
        uint32_t i = xn_play_idx(k);
        if (song.playing && i != xn_flash_idx[k]) {
            const step_t *s = &trk[k].step[i];
            xn_flash_idx[k] = (uint8_t)i;
            if ((s->n && s->time == ST_NOTE) || s->hit) xn_flash_ms[k] = fm1_ms;
        }
    }
    return 1;
}
