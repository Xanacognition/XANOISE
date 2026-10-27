/* SPDX-License-Identifier: GPL-3.0-only */
/* XENOISE: the pages over JIANT's sound (wb_ui.c includes it before its frame):
 *   ENV  the track's envelope drawn over its sound (the wave, shaped by it), the knobs its stages; a note's way along it
 *   LFO  the track's LFO on JIANT's engine (RATE WAVE, its depths to PITCH FILTER SHAPE AMP, FADE), its wave running
 *   ARP  the track's arp on JIANT's engine (MODE RATE OCT GATE, SWING PROB HOLD ORDER): what it will play, a piano roll
 *   MIX  the mixer (GLO on MST): a strip a track, its meter, its level and pan, muted or soloed; the master's meter
 *   FX   the PERFORM page's knobs: the DJ filter, the chop (the slicer's gate and stutter), its step, the wash
 * The LFO and the ARP are the song's (song.x): set on a track, they hold over its sound's own (xs_ext, xn_wb.c). */

static uint8_t lfo_pg;                          /* LFO's knob page (F1) */
static int8_t fx_flt;                           /* FX: the DJ filter (-64 low-pass .. 63 high-pass, 0 off) */
static uint8_t fx_chop, fx_step = 1, fx_wash;   /* FX: 0 off, 1..16 GATE, 17..32 STUT patterns; its step; the wash % */
static const uint8_t JT_OF[NT] = {XJ_BRK, XJ_DRM, XJ_DRM, XJ_BAS, XJ_KEY, XJ_SYN};

/* ---- ENV: the stages that shape the track's sound */
static const pd_t ENV_SYN[4] = {{"ATTACK", PY_ATT, 2, 3000, F_MS, 1}, {"DECAY", PY_DEC, 50, 4000, F_MS, 1},
                                {"SUSTN", PY_SUS, 0, 100, F_PCT, 1}, {"RELEAS", PY_REL, 20, 4000, F_MS, 1}};
static const pd_t ENV_KICK[4] = {{"TUNE", PK_TUNE, -12, 12, F_ST, 1}, {"DECAY", PK_DECAY, 100, 1500, F_MS, 1},
                                 {"PUNCH", PK_PUNCH, 0, 100, F_PCT, 1}, {"DRIVE", PK_DRIVE, 0, 100, F_PCT, 1}};
static const pd_t ENV_DRM[4] = {{"TUNE", PS_TUNE, -12, 12, F_ST, 1}, {"DECAY", PS_DECAY, 50, 600, F_MS, 1},
                                {"TONE", PS_TONE, 0, 100, F_PCT, 1}, {"SNAP", PS_SNAP, 0, 100, F_PCT, 1}};
static const pd_t ENV_BRK[4] = {{"PITCH", PB_PITCH, -12, 12, F_ST, 1}, {"GATE", PB_GATE, 10, 200, F_PCT, 1},
                                {"TONE", PB_TONE, 0, 100, F_HZ, 1}, PD_END};
/* (XENOISE) ENV again: its page 2, where the track has one: the synth's filter envelope, the kit's hats */
static const pd_t ENV_SYN2[4] = {{"F.ENV", PY_FENV, 0, 400, F_X100, 1}, {"RESO", PY_RESO, 5, 180, F_X10, 1},
                                 {"SWOOP", PY_GLIDE, -12, 12, F_ST, 1}, {"VIB", PY_VIB, 0, 60, F_CT, 1}};
static const pd_t ENV_HAT[4] = {{"H.TUNE", PH_TUNE, -12, 12, F_ST, 1}, {"H.DEC", PH_DECAY, 20, 400, F_MS, 1},
                                {"H.TONE", PH_TONE, 0, 100, F_PCT, 1}, {"METAL", PH_METAL, 0, 100, F_PCT, 1}};
static uint32_t env_npages(uint32_t t) { return TRK_TYPE[t] == TY_SYNTH || TRK_TYPE[t] == TY_DRUMS ? 2u : 1u; }
static const pd_t *env_pd(uint32_t t, uint32_t i)
{
    const pd_t *d = TRK_TYPE[t] == TY_SYNTH ? ENV_SYN : TRK_TYPE[t] == TY_KICK ? ENV_KICK : TRK_TYPE[t] == TY_DRUMS ? ENV_DRM : ENV_BRK;
    if (sub % env_npages(t) == 1u) d = TRK_TYPE[t] == TY_SYNTH ? ENV_SYN2 : ENV_HAT;
    return i < 4u && d[i].lab ? &d[i] : 0;
}
static uint32_t env_note(uint32_t t) { return TRK_TYPE[t] == TY_DRUMS ? DN_SNR : TRK_TYPE[t] == TY_SYNTH ? (last_note[t] ? last_note[t] : 60u) : pad_note(t, 0); }
static int env_knob(uint32_t t, uint32_t i, int32_t dd, char *lab, char *val)
{
    const pd_t *d = env_pd(t, i);
    int32_t v;
    if (!d) return 0;
    str_cat(lab, d->lab);
    v = pd_get(d, t);
    if (dd) {
        pd_set(d, t, step_of(d, v, dd));
        v = pd_get(d, t);
        touch();
        if (!sq_play) audition(t, TRK_TYPE[t] == TY_DRUMS && sub % 2u ? DN_HAT : env_note(t));
    }
    pd_fmt(val, d, v, t);
    return 1;
}
/* the envelope at time x ms (0..256) and its length: the synth's ADSR (held a while at its sustain), a drum's decay */
static int32_t env_total(uint32_t t)
{
    const int16_t *p = song.trk[t].p;
    switch (TRK_TYPE[t]) {
    case TY_SYNTH: { int32_t h = (p[PY_ATT] + p[PY_DEC]) / 2; return p[PY_ATT] + p[PY_DEC] + (h < 250 ? 250 : h) + p[PY_REL]; }
    case TY_KICK: return p[PK_DECAY] * 5 / 4 + 20;
    case TY_DRUMS: return p[PS_DECAY] * 5 / 4 + 20;
    default: return 120 + p[PB_GATE] * 4;
    }
}
static int32_t env_at(uint32_t t, int32_t x)
{
    const int16_t *p = song.trk[t].p;
    if (TRK_TYPE[t] == TY_SYNTH) {
        int32_t a = p[PY_ATT] > 1 ? p[PY_ATT] : 1, d = p[PY_DEC] > 1 ? p[PY_DEC] : 1, s = p[PY_SUS] * 256 / 100, r = p[PY_REL] > 1 ? p[PY_REL] : 1;
        int32_t h = (a + d) / 2 < 250 ? 250 : (a + d) / 2;
        if (x < a) return x * 256 / a;
        if (x < a + d) { int32_t f = 256 - (x - a) * 256 / d; return s + (256 - s) * f * f / 65536; }   /* (falling fast, then easing) */
        if (x < a + d + h) return s;
        x -= a + d + h;
        return x >= r ? 0 : s * (r - x) * (r - x) / r / r;
    }
    {
        int32_t T = TRK_TYPE[t] == TY_KICK ? p[PK_DECAY] : TRK_TYPE[t] == TY_DRUMS ? p[PS_DECAY] : 100 + p[PB_GATE] * 3, q;
        if (TRK_TYPE[t] == TY_BREAK) return x < T ? 256 : x < T + 20 ? 256 - (x - T) * 12 : 0;
        if (x >= T) return 0;
        q = 256 - x * 256 / (T ? T : 1);
        return x < 3 ? x * 80 : q * q / 256;
    }
}
static void draw_env(void)
{
    uint32_t t = trk(), k;
    const int16_t *p = song.trk[t].p;
    int32_t x, total = env_total(t), mid = TOP_H + 78, amp = 52, x0 = 8, w = 224, ph = -1;
    uint16_t col = TRK_COL[t], dim = g_mix(C_BG, TRK_COL[t], 70);
    uint32_t seed = 12345u;
    char s[32];
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    s[0] = 0;
    str_cat(str_cat(s, TRK_ID[t]), " ENVELOPE");
    g_text(6, TOP_H + 3, s, col, T_CROP | T_BOLD);
    g_text(234 - g_text_w(patch_name(t, song.trk[t].patch)), TOP_H + 3, patch_name(t, song.trk[t].patch), C_MUTED, T_CROP);
    g_rect(x0, mid, w, 1, g_mix(C_BG, C_TXT, 40));
    if (fm1_ms - flash_ms[t] < (uint32_t)total) ph = (int32_t)(fm1_ms - flash_ms[t]) * w / (total ? total : 1);
    for (x = 0; x < w; x++) {                   /* the sound: its wave under the envelope (a few dozen cycles across) */
        int32_t ms = x * total / w, e = env_at(t, ms), wv, y;
        uint32_t phs = (uint32_t)x * (TRK_TYPE[t] == TY_KICK ? (uint32_t)(2600 - x * 9) : 2100u);
        if (TRK_TYPE[t] == TY_SYNTH) {
            uint32_t wa = (uint32_t)p[PY_WAVE] & 3u, ph16 = phs & 0xFFFFu;
            wv = p[PY_ENG] == 1 ? (wa == 0u ? (int32_t)ph16 / 128 - 256 : wa == 1u ? (ph16 < 32768u ? 230 : -230) :
                                   wa == 2u ? (ph16 < 32768u ? (int32_t)ph16 / 64 - 256 : 768 - (int32_t)ph16 / 64) : xj_sin(ph16) / 128)
                                : (xj_sin(phs) + xj_sin(phs * 2u + (uint32_t)p[PY_INDEX] * 300u) * p[PY_INDEX] / 120) / 128;
        } else if (TRK_TYPE[t] == TY_KICK) wv = xj_sin(phs) / 128;
        else {                                  /* noise and a body: a snare, a break */
            seed = seed * 1103515245u + 12345u;
            wv = (int32_t)((seed >> 16) & 511u) - 256;
            wv = (wv * (TRK_TYPE[t] == TY_DRUMS ? p[PS_TONE] + 30 : 100) + xj_sin(phs) / 128 * 60) / 160;
        }
        y = wv * e / 256 * amp / 256;
        g_rect(x0 + x, y < 0 ? mid + y : mid, 1, (y < 0 ? -y : y) + 1, x < ph ? g_mix(C_BG, C_ORANGE, 150) : dim);
    }
    for (x = 1; x < w; x++) {                   /* the envelope's line, above and below */
        int32_t e0 = env_at(t, (x - 1) * total / w) * amp / 256, e1 = env_at(t, x * total / w) * amp / 256;
        g_line(x0 + x - 1, mid - e0, x0 + x, mid - e1, col);
        g_line(x0 + x - 1, mid + e0, x0 + x, mid + e1, g_mix(C_BG, col, 120));
    }
    if (TRK_TYPE[t] == TY_SYNTH) {              /* its stages: where each ends */
        int32_t a = p[PY_ATT], d = a + p[PY_DEC], h = d + ((a + p[PY_DEC]) / 2 < 250 ? 250 : (a + p[PY_DEC]) / 2);
        static const char *const L[3] = {"A", "D", "S"};
        int32_t at[3] = {a, d, h};
        for (k = 0; k < 3u; k++) {
            int32_t xx = x0 + at[k] * w / (total ? total : 1);
            g_rect(xx, mid - amp, 1, amp * 2, g_mix(C_BG, C_TXT, 50));
            g_text(xx + 2, mid - amp - 2, L[k], C_MUTED, T_CROP);
        }
        g_text(x0 + w - 8, mid - amp - 2, "R", C_MUTED, T_CROP);
    }
    if (ph >= 0 && ph < w) g_rect(x0 + ph, mid - amp, 2, amp * 2, C_ORANGE);   /* a note's way along it */
    {                                           /* the output, live */
        int32_t sx;
        g_text(6, TOP_H + 132, "OUT", C_MUTED, T_CROP);
        for (sx = 0; sx < 196; sx += 2) {
            int32_t v = scope[(scope_w + (uint32_t)sx * 120u / 196u) % 120u];
            g_rect(36 + sx, TOP_H + 137 + clampi(v / 1600, -6, 6), 2, 1, C_BLUE);
        }
    }
}

/* ---- LFO: JIANT's, on the track's engine (the song holds it once set; until then the sound's own shows) */
static void lfo_vals(uint32_t t, int8_t *v)
{
    const wb_lfo_t *l = &song.x.lfo[t];
    if (l->set) { v[0] = l->rate; v[1] = l->wave; v[2] = l->fade; v[3] = l->pit; v[4] = l->flt; v[5] = l->shp; v[6] = l->amp; }
    else xj_lfo_read(JT_OF[t], v);
}
static int lfo_knob(uint32_t t, uint32_t i, int32_t dd, char *lab, char *val)
{
    static const char *const L[2][4] = {{"RATE", "WAVE", "PITCH", "FILTER"}, {"AMP", "SHAPE", "FADE", ""}};
    static const uint8_t IX[2][4] = {{0, 1, 3, 4}, {6, 5, 2, 0xFF}};
    static const int16_t LO[7] = {-12, 0, 0, -64, -64, -64, 0}, HI[7] = {127, 4, 127, 63, 63, 63, 127};
    uint32_t q = IX[lfo_pg & 1u][i];
    int8_t v[7];
    if (q == 0xFF) return 0;
    lfo_vals(t, v);
    str_cat(lab, L[lfo_pg & 1u][i]);
    if (dd) {
        wb_lfo_t *l = &song.x.lfo[t];
        v[q] = (int8_t)clampi(v[q] + dd, LO[q], HI[q]);
        l->set = 1; l->rate = v[0]; l->wave = v[1]; l->fade = v[2]; l->pit = v[3]; l->flt = v[4]; l->shp = v[5]; l->amp = v[6];
        touch();
    }
    if (q == 0u) { if (v[0] < 0) str_cat(val, xj_lsync(v[0])); else fmt_u(val, (uint32_t)v[0], 0); }
    else if (q == 1u) str_cat(val, xj_lfo_wave((uint32_t)v[1]));
    else fmt_s(val, v[q]);
    return 1;
}
static int32_t lfo_wave_at(uint32_t wave, uint32_t ph)   /* -256..256, ph 16 bits */
{
    switch (wave % 5u) {
    case 0: return xj_sin(ph) / 128;
    case 1: return ph < 32768u ? (int32_t)ph / 64 - 256 : 768 - (int32_t)ph / 64;
    case 2: return (int32_t)ph / 128 - 256;
    case 3: return ph < 32768u ? 230 : -230;
    default: { uint32_t h = (ph >> 13) * 2654435761u; return (int32_t)(h >> 23) - 256; }
    }
}
static void draw_lfo(void)
{
    uint32_t t = trk(), k, period;
    int8_t v[7];
    int32_t x, mid = TOP_H + 60, amp = 34;
    uint16_t col = TRK_COL[t];
    char s[32];
    static const char *const DN[4] = {"PITCH", "FILTER", "SHAPE", "AMP"};
    lfo_vals(t, v);
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    s[0] = 0;
    str_cat(str_cat(s, TRK_ID[t]), " LFO");
    g_text(6, TOP_H + 3, s, col, T_CROP | T_BOLD);
    g_text(120, TOP_H + 3, song.x.lfo[t].set ? "SET HERE" : "THE SOUND'S OWN", C_MUTED, T_CROP);
    if (v[0] < 0) {                             /* the period: in time (the tempo's), or free */
        static const uint16_t Q[12] = {64, 32, 16, 8, 4, 3, 3, 2, 1, 1, 1, 1};   /* quarter beats a cycle, roughly */
        period = (uint32_t)Q[(uint32_t)(-v[0] - 1) % 12u] * 15000u / (song.bpm ? song.bpm : 120u);
    } else period = 8000u >> ((uint32_t)v[0] / 16u);
    if (period < 40u) period = 40u;
    g_rect(8, mid, 224, 1, g_mix(C_BG, C_TXT, 40));
    for (x = 1; x < 224; x++) {                 /* two cycles of its wave */
        int32_t a = clampi(lfo_wave_at((uint32_t)v[1], (uint32_t)(x - 1) * 65536u / 112u & 0xFFFFu) * amp / 256, -amp, amp);   /* (a cycle's 16 bits) */
        int32_t b = clampi(lfo_wave_at((uint32_t)v[1], (uint32_t)x * 65536u / 112u & 0xFFFFu) * amp / 256, -amp, amp);
        g_line(8 + x - 1, mid - a, 8 + x, mid - b, col);
    }
    {                                           /* where it is now (its rate, roughly) */
        uint32_t ph = fm1_ms % (period * 2u) * 65536u / period;   /* (two cycles: 0 .. 2^17) */
        int32_t px = 8 + (int32_t)((ph & 0x1FFFFu) * 224u / 131072u), py = mid - clampi(lfo_wave_at((uint32_t)v[1], ph & 0xFFFFu) * amp / 256, -amp, amp);
        g_rect(px - 2, py - 2, 5, 5, C_ORANGE);
    }
    for (k = 0; k < 4u; k++) {                  /* its depths, two by two: bipolar bars (AMP from nothing) */
        int32_t x = 6 + (int32_t)(k & 1u) * 118, y = TOP_H + 108 + (int32_t)(k >> 1) * 16, d = k == 3u ? v[6] / 2 : v[3 + k];
        int32_t bx = x + 50, bw = 56, cx = bx + bw / 2;
        g_text(x, y - 5, DN[k], C_MUTED, T_CROP);
        g_rect(bx, y, bw, 3, g_mix(C_BG, C_TXT, 30));
        if (k == 3u) g_rect(bx, y, d * bw / 64, 3, col);
        else g_rect(d < 0 ? cx + d * (bw / 2) / 64 : cx, y, (d < 0 ? -d : d) * (bw / 2) / 64 + 1, 3, col);
    }
}

/* ---- ARP: JIANT's, on the track's engine */
static const uint8_t ARP_DEF[8] = {1, 2, 1, 64, 0, 127, 0, 0};   /* UP 1/16 one octave, gate half, no swing, always */
static void arp_vals(uint32_t t, uint8_t *a)
{
    const wb_arp_t *r = &song.x.arp[t];
    uint32_t i;
    if (!r->set) { for (i = 0; i < 8u; i++) a[i] = ARP_DEF[i]; a[0] = 0; return; }
    a[0] = r->mode; a[1] = r->rate; a[2] = r->oct; a[3] = r->gate; a[4] = r->swing; a[5] = r->prob; a[6] = r->hold; a[7] = r->order;
}
static void arp_put(uint32_t t, const uint8_t *a)
{
    wb_arp_t *r = &song.x.arp[t];
    r->set = 1; r->mode = a[0]; r->rate = a[1]; r->oct = a[2]; r->gate = a[3]; r->swing = a[4]; r->prob = a[5]; r->hold = a[6]; r->order = a[7];
    touch();
}
static int arp_knob(uint32_t t, uint32_t i, int32_t dd, char *lab, char *val)
{
    static const char *const L[2][4] = {{"MODE", "RATE", "OCT", "GATE"}, {"SWING", "PROB", "HOLD", "ORDER"}};
    static const int16_t LO[8] = {0, 0, 1, 1, 0, 0, 0, 0}, HI[8] = {15, 9, 4, 127, 100, 127, 1, 1};
    uint32_t q = (sub & 1u) * 4u + i;
    uint8_t a[8];
    char *e;
    arp_vals(t, a);
    str_cat(lab, L[sub & 1u][i]);
    if (dd) {
        if (!song.x.arp[t].set) a[0] = 1;       /* (the first turn: on, UP) */
        a[q] = (uint8_t)clampi((int32_t)a[q] + dd, LO[q], HI[q]);
        arp_put(t, a);
    }
    switch (q) {
    case 0: str_cat(val, xj_amode(a[0])); break;
    case 1: str_cat(val, xj_div(a[1])); break;
    case 2: e = fmt_u(val, a[2], 0); str_cat(e, " OCT"); break;
    case 3: case 5: e = fmt_u(val, (uint32_t)a[q] * 100u / 127u, 0); str_cat(e, "%"); break;
    case 4: e = fmt_u(val, a[4], 0); str_cat(e, "%"); break;
    case 6: str_cat(val, a[6] ? "ON" : "OFF"); break;
    default: str_cat(val, a[7] ? "PLAYED" : "NOTE"); break;
    }
    return 1;
}
static void draw_arp(void)
{
    uint32_t t = trk(), k, n, idx = 0, len = 0, lo = 127, hi = 0;
    uint8_t a[8], held[16], list[64], seq[16];
    int live;
    uint16_t col = TRK_COL[t];
    char s[32];
    arp_vals(t, a);
    n = xj_arp_view(JT_OF[t], held, &idx);
    live = n && song.x.arp[t].set && a[0];
    if (!n) {                                   /* nothing held: the chord the song is on (its root, third, fifth) */
        int32_t r = deg_at(60 + song.key) + prog_deg(sq_play ? sq_row : cur_row);
        held[0] = (uint8_t)deg_midi(r); held[1] = (uint8_t)deg_midi(r + 2); held[2] = (uint8_t)deg_midi(r + 4);
        n = 3;
    }
    for (k = 1; k < n; k++) {                   /* sorted (ORDER NOTE), then over its octaves */
        uint32_t j = k;
        if (a[7]) break;
        while (j && held[j - 1u] > held[j]) { uint8_t q = held[j]; held[j] = held[j - 1u]; held[j - 1u] = q; j--; }
    }
    for (k = 0; k < (uint32_t)(a[2] ? a[2] : 1u) * n && len < 64u; k++) list[len++] = (uint8_t)(held[k % n] + 12u * (k / n));
    for (k = 0; k < 16u; k++) {                 /* the first 16 notes it plays */
        uint32_t m = a[0], c = len ? len : 1u, j;
        if (m == 2u || m == 7u) j = c - 1u - k % c;                       /* DN, DNUP */
        else if (m == 3u) { uint32_t pd = c > 1u ? 2u * c - 2u : 1u; j = k % pd; if (j >= c) j = pd - j; }   /* UPDN */
        else if (m == 4u) j = (k * 2654435761u >> 7) % c;               /* RND */
        else if (m == 6u) j = c - 1u;                                     /* REPEAT */
        else j = k % c;                                                   /* UP (and the rest, as near) */
        seq[k] = list[j % (len ? len : 1u)];
        if (seq[k] < lo) lo = seq[k];
        if (seq[k] > hi) hi = seq[k];
    }
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    s[0] = 0;
    str_cat(str_cat(str_cat(s, TRK_ID[t]), " ARP  "), a[0] ? xj_amode(a[0]) : "OFF");
    g_text(6, TOP_H + 3, s, col, T_CROP | T_BOLD);
    g_text(150, TOP_H + 3, live ? "PLAYING" : "PLAY PADS", live ? C_ORANGE : C_MUTED, T_CROP);
    {
        int32_t gy = TOP_H + 20, gh = 104, cw = 14, span = (int32_t)(hi - lo) + 1, rh;
        if (span < 12) { lo = lo > 6u ? lo - 6u + (uint32_t)(span / 2) : 0u; span = 12; }
        rh = gh / span;
        if (rh < 2) rh = 2;
        for (k = 0; k < 16u; k++) {
            int32_t x = 8 + (int32_t)k * cw, y = gy + gh - ((int32_t)seq[k] - (int32_t)lo + 1) * rh;
            int on = live && (idx % 16u) == k;
            g_rect(x, gy, cw - 1, gh, k % 4u ? C_ROW : C_BEAT);
            if (!a[0]) continue;
            g_rect(x, y, (cw - 2) * a[3] / 127 + 1, rh, on ? C_ORANGE : col);
        }
    }
    s[0] = 0;
    str_cat(str_cat(s, "RATE "), xj_div(a[1]));
    g_text(8, TOP_H + 128, s, C_MUTED, T_CROP);
    g_text(110, TOP_H + 128, a[6] ? "HOLD" : "", C_ORANGE, T_CROP);
}

/* ---- MIX: a strip a track (its meter, its level, its pan, muted, soloed), the master */
static uint8_t mix_pk[NT + 1];
static int32_t meter_h(int32_t pk, int32_t h)   /* a peak (Q15) as a height: 48 dB over h */
{
    int32_t lg = 0, v;
    if (pk < 16) return 0;
    while ((pk >> lg) > 1) lg++;
    v = lg * 8 + (((pk << 3) >> lg) & 7);       /* 8 log2 */
    return clampi((v - 32) * h / 96, 0, h);
}
static void draw_mixer(void)
{
    uint32_t t, sel = trk();
    int32_t mtop = TOP_H + 16, mh = 92;
    char s[8];
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    g_text(6, TOP_H + 2, "MIXER", C_BLUE, T_CROP | T_BOLD);
    g_text(62, TOP_H + 2, "PRESETS: MASTER  GLO: BACK", C_MUTED, T_CROP);
    for (t = 0; t <= NT; t++) {                 /* the tracks, then the master */
        int32_t x = 4 + (int32_t)t * 33, pk = t < NT ? xj_peak(JT_OF[t]) : 0, h, fy;
        const track_t *tr = t < NT ? &song.trk[t] : 0;
        uint16_t c = t < NT ? TRK_COL[t] : C_TXT;
        if (t == NT) { uint32_t i; for (i = 0; i < 120u; i += 4u) { int32_t q = scope[i]; q = q < 0 ? -q : q; if (q > pk) pk = q; } }
        h = meter_h(pk, mh);
        mix_pk[t] = (uint8_t)(h > mix_pk[t] ? h : mix_pk[t] > 2 ? mix_pk[t] - 2 : 0);   /* (it falls slowly) */
        g_rect(x, mtop, 30, mh + 26, t == sel ? g_mix(C_BG, C_BLUE, 40) : C_ROW);
        if (t == sel) g_frame(x, mtop, 30, mh + 26, C_BLUE);
        g_rect(x + 4, mtop + 4, 8, mh, C_INSET);   /* the meter */
        if (!(tr && tr->mute)) {
            g_rect(x + 4, mtop + 4 + mh - h, 8, h, h > mh * 7 / 8 ? C_REDC : c);
            g_rect(x + 4, mtop + 4 + mh - mix_pk[t], 8, 1, C_TXT);
        }
        fy = mtop + 4 + mh - (t < NT ? tr->vol : song.mvol) * mh / 100;   /* the level */
        g_rect(x + 16, mtop + 4, 2, mh, C_FAINT);
        g_rect(x + 14, fy - 2, 8, 5, t < NT ? c : C_TXT);
        if (tr && tr->pan) g_rect(x + 15 + tr->pan * 7 / 50, mtop + 1, 2, 2, C_MUTED);
        g_text_c(x + 15, mtop + mh + 9, t < NT ? TRK_ID[t] : "MST", tr && tr->mute ? C_FAINT : c, T_CROP | T_BOLD);
        if (tr && (tr->mute || tr->solo)) {
            s[0] = tr->mute ? 'M' : 'S';
            s[1] = 0;
            g_rect(x + 22, mtop + 4, 7, 10, tr->mute ? C_REDC : C_BLUE);
            g_text(x + 22, mtop + 3, s, C_BG, T_CROP);
        }
    }
}

/* ---- FX (PERFORM): its knobs */
static int fx_knob(uint32_t i, int32_t dd, char *lab, char *val)
{
    static const char *const L[4] = {"FILTER", "CHOP", "STEP", "WASH"};
    static const char *const STEP[6] = {"1/8", "1/16", "1/32", "8T", "16T", "32T"};
    char *e;
    str_cat(lab, L[i]);
    if (i == 0u) {
        if (dd) fx_flt = (int8_t)clampi(fx_flt + dd * 2, -64, 63);
        if (!fx_flt) str_cat(val, "OFF");
        else { e = str_cat(val, fx_flt < 0 ? "LP " : "HP "); fmt_u(e, (uint32_t)(fx_flt < 0 ? -fx_flt : fx_flt), 0); }
    } else if (i == 1u) {
        if (dd) fx_chop = (uint8_t)clampi((int32_t)fx_chop + dd, 0, 32);
        if (!fx_chop) str_cat(val, "OFF");
        else { e = str_cat(val, fx_chop <= 16u ? "GATE " : "STUT "); fmt_u(e, (fx_chop - 1u) % 16u + 1u, 0); }
    } else if (i == 2u) {
        if (dd) fx_step = (uint8_t)clampi((int32_t)fx_step + dd, 0, 5);
        str_cat(val, STEP[fx_step % 6u]);
    } else {
        if (dd) fx_wash = (uint8_t)clampi((int32_t)fx_wash + dd * 2, 0, 100);
        e = fmt_u(val, fx_wash, 0);
        str_cat(e, "%");
    }
    return 1;
}
static void fx_reset(void) { fx_flt = 0; fx_chop = 0; fx_wash = 0; }

/* ---- (XENOISE) GEN: the next song (SAVE held). Page 1 GENRE PROG CHORDS MELODY, page 2 BPM KEY FORM BASS (SAVE: the
 * next page); a row a knob, its number at its side, the one ALGORITHM is on framed (SELECT turns it) */
static void draw_gen(void)
{
    static const char *const RN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    static const char SL[8] = {'I', 'V', 'C', 'V', 'B', 'U', 'D', 'O'};   /* intro verse chorus verse bridge bUild drop outro */
    const wb_genre_t *g = &WB_GENRES[genre_pick % WB_NGENRE];
    char s[48], lab[16], val[24], *e;
    uint32_t r, k, now = fm1_ms, pg = sub & 1u;
    int32_t y0 = TOP_H + 50;
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    gen_holo();
    if (!pg) {
        knob_badge(226, TOP_H + 16, 0);
        if (pilot_col == 0u) g_frame(0, TOP_H, 240, 46, C_TXT);
    }
    for (r = 0; r < 4u; r++) {
        int32_t y = y0 + (int32_t)r * 23, x = 80, w = 156;
        uint32_t kn = pg ? r : r + 1u;
        int has = pg || r < 3u, foc = has && kn == pilot_col;
        g_rect(2, y, 236, 21, g_mix(C_BG, C_PANEL, foc ? 220u : 120u));
        if (foc) g_frame(2, y, 236, 21, C_TXT);
        lab[0] = val[0] = 0;
        if (has) { ngen_knob(kn, 0, lab, val); knob_badge(5, y + 4, kn); g_text(21, y + 4, lab, C_MUTED, T_CROP); }
        s[0] = 0;
        if (!pg && r == 0u) {                   /* PROG: its name */
            g_text(x, y + 4, ng_prog > 1u ? WB_PROGS[ng_prog - 1u].name : ng_prog ? "THE GENRE'S OWN" : "AUTO (GENRE'S PICK)", C_TXT, T_CROP);
        } else if (!pg && r == 1u) {            /* CHORDS: a pattern's 4 bars, a block a chord, its numeral */
            const int8_t *dg = ng_prog > 1u ? WB_PROGS[ng_prog - 1u].deg : g->deg;
            uint32_t n = ng_prog > 1u ? WB_PROGS[ng_prog - 1u].n : g->ndeg ? g->ndeg : 1u, rows = ng_cspace ? 4u << ng_cspace : n > 4u ? 8u : 16u, at = 0;
            while (n * rows > NROW && rows > 8u) rows /= 2u;
            for (k = 0; at < NROW; k++, at += rows) {
                int32_t bx = x + (int32_t)(at * (uint32_t)w / NROW), bw = (int32_t)(rows * (uint32_t)w / NROW);
                uint32_t dd = (uint32_t)(dg[k % n] % 7 + 7) % 7u;
                g_rect(bx, y + 3, bw - 1, 15, k % n == 0u ? g_mix(C_BG, C_CYAN, 140) : g_mix(C_BG, C_BLUE, 60 + (k & 1u) * 40u));
                if (bw >= 18) g_text_c(bx + bw / 2, y + 4, RN[dd], C_TXT, T_CROP);
            }
        } else if (!pg && r == 2u) {            /* MELODY: two bars of it, what stays (the downbeats, each bar's last) */
            static const uint8_t RH[6] = {0, 3, 6, 8, 11, 14};
            uint32_t drop = ng_mspace == 1u ? 40u : ng_mspace == 2u ? 70u : 0u;
            for (k = 0; k < 12u; k++) {
                uint32_t step = (k / 6u) * 16u + RH[k % 6u], keep = RH[k % 6u] % 8u == 0u || k % 6u == 5u || (k * 2654435761u >> 9) % 100u >= drop;
                int32_t px = x + (int32_t)(step * (uint32_t)w / 32u), h = keep ? 6 + (int32_t)((k * 7u) % 5u) * 2 : 2;
                g_rect(px, y + 18 - h, 6, h, keep ? C_TXT : C_FAINT);
            }
        } else if (!pg) {                       /* the rest of it, at a glance */
            e = str_cat(s, "BPM ");
            e = ng_bpm ? fmt_u(e, ng_bpm, 0) : str_cat(e, "AUTO");
            e = str_cat(str_cat(e, "  KEY "), ng_key ? NOTE_NM[(ng_key - 1u) % 12u] : "AUTO");
            str_cat(str_cat(e, "  FORM "), FORM_NAME[ng_form % WB_NFORM]);
            g_text(6, y + 4, s, C_MUTED, T_CROP);
        } else if (r == 0u) {                   /* BPM: it, a light at its beat */
            uint32_t bpm = ng_bpm ? ng_bpm : (g->bpm[0] + g->bpm[1]) / 2u;
            if (ng_bpm) fmt_u(s, ng_bpm, 0);
            else { e = fmt_u(str_cat(s, "AUTO "), g->bpm[0], 0); *e++ = '-'; fmt_u(e, g->bpm[1], 0); }
            g_text(x, y + 4, s, C_TXT, T_CROP);
            g_rect(222, y + 6, 9, 9, now % (60000u / (bpm ? bpm : 120u)) < 90u ? C_ORANGE : C_FAINT);
        } else if (r == 1u) {                   /* KEY: twelve, the one lit */
            g_text(x, y + 4, val, C_TXT, T_CROP);
            for (k = 0; k < 12u; k++)
                g_rect(120 + (int32_t)k * 10, y + 5, 8, 11, ng_key && ng_key - 1u == k ? C_ORANGE : NOTE_NM[k][1] ? C_FAINT : g_mix(C_BG, C_TXT, 90));
        } else if (r == 2u) {                   /* FORM: its parts */
            uint32_t f = ng_form % WB_NFORM, n = FORM_LEN[f];
            for (k = 0; k < n; k++) {
                int32_t bx = x + (int32_t)(k * (uint32_t)w / n), bw = (int32_t)((uint32_t)w / n);
                char c2[2] = {SL[FORM[f][k] % 8u], 0};
                g_rect(bx, y + 3, bw - 1, 15, FORM[f][k] == S_CHORUS || FORM[f][k] == S_DROP ? g_mix(C_BG, C_ORANGE, 150) : g_mix(C_BG, C_BLUE, 90));
                g_text_c(bx + bw / 2, y + 4, c2, C_TXT, T_CROP);
            }
        } else {                                /* BASS */
            g_text(x, y + 4, (const char *const[]){"WARM KEYS / SINE -1", "WARM KEYS -1 OCT", "SINE -1 OCT", "THE GENRE'S OWN"}[ng_bass & 3u], C_TXT, T_CROP);
        }
    }
    if (arm_f == SA_NEW || (sng_fx == 4u && now - sng_fx_ms < 900u)) {   /* the question; then the new song coming */
        int ask = arm_f == SA_NEW, blink = (now / 260u) & 1u;
        g_rect(14, TOP_H + 66, 212, 50, C_BG);
        g_frame(14, TOP_H + 66, 212, 50, blink ? C_BLUE : g_mix(C_BG, C_BLUE, 120));
        g_text_c(120, TOP_H + 71, ask ? "GENERATE A SONG" : "NEW SONG", C_BLUE, T_BOLD);
        g_text_c(120, TOP_H + 89, ask ? (genre_pick < WB_NGENRE ? g->name : "EMPTY SONG") : song.name, C_TXT, T_CROP);
        if (ask) g_text_c(120, TOP_H + 102, "OCT+: YES   OCT-: NO", blink ? C_TXT : C_MUTED, T_CROP);
        else g_rect(28, TOP_H + 104, (int32_t)(now - sng_fx_ms) * 170 / 900, 4, C_CYAN);
    }
}
