/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XENOISE) */
/* XENOISE: WND-1's sequencer (its wb_dsp.c, the voices and the master left out) playing JIANT's engines. A row's cells
 * become notes on JIANT's tracks through xj_on / xj_off (xn_wb.c), the gates as note-offs counted down here:
 *   BRK          JIANT track 1 (XJ_BRK), DRUM: the slice's hits of the break (wb_brk.h: WND-1's breaks as hits)
 *   KIK, DRM     JIANT track 2 (XJ_DRM), DRUM: the kick; the snare, clap, closed and open hat
 *   BAS KEY SYN  JIANT tracks 3 4 5, the sound's engine (xn_wb.c xs_sound); KEY's COUNTER: track 6 (XJ_CTR)
 * Integer maths as before; it runs in JIANT's audio interrupt (fx.c events_block), every 16 frames of each block. */

/* ---- from WND-1's wb_dsp.c: the helpers the sequencer and the screen use */
static int32_t noise(void) { return (int32_t)(xr(&rnd_isr) >> 16) - 32768; }
static int32_t sine(uint32_t ph)                /* a full cycle over 2^32 -> -32767..32767 (refined parabola) */
{
    int32_t x = (int32_t)ph >> 16, ax = x < 0 ? -x : x, y, ay;
    y = (x * (32768 - ax)) >> 13;
    ay = y < 0 ? -y : y;
    y += ((((y * ay) >> 15) - y) * 7373) >> 15;
    return y > 32767 ? 32767 : y < -32767 ? -32767 : y;
}

/* ---- the notes on JIANT's tracks, and their gates */
#define NOFF 40u
static struct { int32_t left; uint8_t jt, note; } xoff[NOFF];
static void xs_hit(uint32_t jt, uint32_t note, int32_t vel)           /* a drum: a one-shot (no gate) */
{
    xj_on(jt, note, (uint32_t)clampi(vel >> 8, 1, 127));
}
static void xs_note(uint32_t jt, uint32_t note, int32_t vel, int32_t gate)   /* gate: frames (< 0: held) */
{
    uint32_t i;
    for (i = 0; i < NOFF; i++)                  /* the same note again: its old gate goes */
        if (xoff[i].left && xoff[i].jt == jt && xoff[i].note == note) { xj_off(jt, note); xoff[i].left = 0; }
    xj_on(jt, note, (uint32_t)clampi(vel >> 8, 1, 127));
    if (gate < 0) return;
    for (i = 0; i < NOFF; i++)
        if (!xoff[i].left) { xoff[i].left = gate > 16 ? gate : 16; xoff[i].jt = (uint8_t)jt; xoff[i].note = (uint8_t)note; return; }
    xj_off(jt, note);                           /* (no room: short) */
}
static void xs_gates(uint32_t frames)
{
    uint32_t i;
    for (i = 0; i < NOFF; i++)
        if (xoff[i].left && (xoff[i].left -= (int32_t)frames) <= 0) { xoff[i].left = 0; xj_off(xoff[i].jt, xoff[i].note); }
}
static void xs_stop(void)                       /* STOP: the synths let go (the drums ring out) */
{
    uint32_t i;
    for (i = 0; i < NOFF; i++) if (xoff[i].left) { xoff[i].left = 0; xj_off(xoff[i].jt, xoff[i].note); }
    xj_release(XJ_BAS); xj_release(XJ_KEY); xj_release(XJ_SYN); xj_release(XJ_CTR);
}
static const uint8_t XS_DRM_NOTE[4] = {38, 42, 46, 39};     /* DRM's kinds: snare, closed hat, open hat, clap (GM) */
static const uint8_t XS_LANE_NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};   /* JIANT's DRUM lanes (DRUM_LANE_NOTE) */
static const uint8_t XS_JT[NT] = {XJ_BRK, XJ_DRM, XJ_DRM, XJ_BAS, XJ_KEY, XJ_SYN};

/* ---- triggers (the sequencer's and the pads') */
static uint32_t row_frames(void) { uint32_t f = 661500u / (song.bpm ? song.bpm : 100u); return punch_half ? f * 2u : f; }
static void counter_voice(uint32_t t, uint32_t n, int32_t vel, int32_t gate);
typedef struct {
    int32_t wait;
    uint8_t trk, note, rev, retrig;
    int8_t pitch;
    int16_t vel;
    int32_t gate;
    uint8_t ctr, pad[3];
} pend_t;
#define NPEND 48u
static pend_t pend[NPEND];
static void pend_add(int32_t wait, uint32_t t, uint32_t note, int32_t vel, int32_t gate, int rev, int32_t pitch, int retrig, int ctr)
{
    uint32_t i;
    for (i = 0; i < NPEND; i++)
        if (!pend[i].note && !pend[i].ctr) {
            pend_t *e = &pend[i];
            e->wait = wait; e->trk = (uint8_t)t; e->note = (uint8_t)(note ? note : 0); e->vel = (int16_t)vel; e->gate = gate;
            e->rev = (uint8_t)rev; e->pitch = (int8_t)pitch; e->retrig = (uint8_t)retrig; e->ctr = (uint8_t)ctr;
            if (!e->note) e->note = 0, e->ctr = (uint8_t)(ctr | 2);   /* (slice 0: kept by the flag) */
            return;
        }
}
/* a slice of the break: its step's hits on the DRUM lanes, a roll / drag / flam as the hits after it (WND-1 played the
 * recorded slice: GATE cut it, PITCH and P its speed, B it backwards; here GATE and PITCH wait for the kit's sound) */
static void brk_slice(uint32_t slice, int32_t vel)
{
    uint32_t src = (uint32_t)clampi(song.trk[T_BRK].p[PB_SRC], 0, BRK_N - 1), k, rf = row_frames();
    for (k = 0; k < 4u; k++) {
        uint32_t h = BRK_HIT[src][slice % 16u][k], lane = h & 7u, m = h >> 7;
        int32_t v = (int32_t)((h >> 3) & 15u) * vel / 15;
        if (!h) break;
        if (m == 3u) {                          /* a flam: a soft grace note just before */
            xs_hit(XJ_BRK, XS_LANE_NOTE[lane], v * 4 / 10);
            pend_add((int32_t)rf / 12, T_BRK, XS_LANE_NOTE[lane], v, 0, 0, 0, 0, 4);
            continue;
        }
        xs_hit(XJ_BRK, XS_LANE_NOTE[lane], m == 2u ? v * 3 / 4 : v);
        if (m == 1u) pend_add((int32_t)rf / 2, T_BRK, XS_LANE_NOTE[lane], v * 8 / 10, 0, 0, 0, 0, 4);
        if (m == 2u) { pend_add((int32_t)rf / 3, T_BRK, XS_LANE_NOTE[lane], v * 85 / 100, 0, 0, 0, 0, 4);
                       pend_add((int32_t)rf * 2 / 3, T_BRK, XS_LANE_NOTE[lane], v, 0, 0, 0, 0, 4); }
    }
}
static void trigger(uint32_t t, uint32_t n, int32_t vel, int32_t gate, int rev, int32_t pitch, int retrig)
{
    const int16_t *p = song.trk[t].p;
    (void)rev; (void)pitch; (void)retrig;
    trig_ms[t] = 1u;
    switch (TRK_TYPE[t]) {
    case TY_BREAK:
        brk_slice(n, vel);
        break;
    case TY_KICK:
        xs_hit(XJ_DRM, 36, vel);
        break;
    case TY_DRUMS:                              /* the note says which drum: snare, clap, closed or open hat */
        xs_hit(XJ_DRM, XS_DRM_NOTE[drm_kind(n)], drm_kind(n) == DK_HAT || drm_kind(n) == DK_OPEN ? (vel * 24000) >> 15 : vel);   /* (the hats under the snare) */
        break;
    default: {
        int32_t notes[5];
        uint32_t k, cnt, m = (uint32_t)clampi((int32_t)n + 12 * p[PY_OCT], 12, 120);
        cnt = chord_notes((int32_t)m, (uint32_t)clampi(p[PY_CHORD], 0, WB_NCHORD - 1), notes);
        if (t == T_BAS) xj_release(XJ_BAS);
        for (k = 0; k < cnt; k++)
            xs_note(XS_JT[t], (uint32_t)clampi(notes[k], 12, 120), cnt == 1u ? vel : vel * 3 / 4, gate);
        if (t == T_KEY && p[PY_CTR]) counter_voice(t, m, vel, gate);
    }
    }
}
static void counter_voice(uint32_t t, uint32_t n, int32_t vel, int32_t gate)
{
    const int16_t *p = song.trk[t].p;
    int32_t d = deg_at((int32_t)n), sh = (int32_t)n - deg_midi(d), m = (int32_t)n, wait = 0, g = gate, rf = (int32_t)row_frames();
    int32_t lvl = clampi(p[PY_CTRL], 0, 100) * 327;
    switch (p[PY_CTR]) {
    case 1: m = deg_midi(d + 2) + sh; break;
    case 2: m = deg_midi(d - 2) + sh; break;
    case 3: m = deg_midi(d - 5) + sh; break;
    case 4: m = (int32_t)n + 12; break;
    case 5: m = deg_midi(d + 2) + sh; wait = 3 * rf; g = gate < 2 * rf ? gate : 2 * rf; lvl = lvl * 8 / 10; break;
    case 6: m = (int32_t)n + 12; wait = 2 * rf; g = gate < 3 * rf ? gate : 3 * rf; lvl = lvl * 7 / 10; break;
    }
    if (wait) { pend_add(wait, t, (uint32_t)clampi(m, 12, 120), (vel * lvl) >> 15, g, 0, 0, 0, 1); return; }
    xs_note(XJ_CTR, (uint32_t)clampi(m, 12, 120), (vel * lvl) >> 15, g);
}

/* ---- the sequencer */
static uint32_t sq_frame, sq_rowlen, sq_pos;    /* frames into the row, the row's length; the real row (punch-ins map it) */
static uint8_t loop_start, rev_at;
static uint32_t sq_racc;                        /* the rows' lengths, exact on average: 661500 / BPM has a remainder */
static uint32_t exp_frames0, exp_frames, exp_mark;   /* (the emulator's export: where the pattern came round) */
static uint32_t next_rowlen(void)
{
    uint32_t b = song.bpm ? song.bpm : 100u, f = row_frames();
    sq_racc += 661500u % b;
    if (sq_racc >= b) { sq_racc -= b; f++; }
    return f;
}
static uint32_t gate_rows(const pat_t *p, uint32_t t, uint32_t r)
{
    uint32_t k, L = p->len ? p->len : 16u;
    for (k = r + 1u; k < L; k++) if (p->c[t][k].n) return k - r;
    return L - r;
}
/* the meter: sq_pos counts the steps played; a bar of 16 rows plays METER_STEPS of them (fewer: its end is left
 * out; more: its last beat again) */
static uint32_t meter_steps(void) { return METER_STEPS[(punch_meter ? punch_meter : song.meter) % NMETER]; }
static uint32_t steps_of(uint32_t L)            /* the steps a pattern of L rows takes */
{
    uint32_t B = meter_steps(), rest = L % 16u;
    return B == 16u ? L : (L / 16u) * B + (rest < B ? rest : B);
}
static uint32_t row_of(uint32_t pos)            /* the row a step plays */
{
    uint32_t B = meter_steps(), k;
    if (B == 16u) return pos;
    k = pos % B;
    if (k >= 16u) k = 12u + (k - 16u) % 4u;
    return (pos / B) * 16u + k;
}
static uint32_t play_row(uint32_t L)
{
    uint32_t n = steps_of(L), vp = sq_pos % n;
    if (punch_rev) vp = (uint32_t)((((int32_t)rev_at * 2 - (int32_t)sq_pos) % (int32_t)n + (int32_t)n) % (int32_t)n);
    if (punch_loop) vp = (loop_start + ((sq_pos - loop_start) % punch_loop)) % n;
    return row_of(vp) % L;
}
static void row_fire(void)
{
    const pat_t *p = &song.pat[sq_pi % NPAT];
    uint32_t L = p->len ? p->len : 16u, r = play_row(L), t, rf = sq_rowlen, solo = 0;
    int32_t sw = (r & 1u) ? (int32_t)(rf * (uint32_t)clampi(song.swing - 50, 0, 25) * 2u / 100u) : 0;
    sq_row = (uint8_t)r;
    sq_rowtick++;
    for (t = 0; t < NT; t++) if (song.trk[t].solo) solo = 1;
    for (t = 0; t < NT; t++) {
        const cell_t *c = &p->c[t][r];
        const track_t *tr = &song.trk[t];
        uint32_t n = c->n, hits, h;
        int32_t vel, wait, prob;
        if ((rpt_held & 0x80u) && rpt_trk == t) {   /* note repeat: the held pad, every row */
            pend_add(sw, t, rpt_note + (TRK_TYPE[t] == TY_BREAK ? 1u : 0u), 26000, (int32_t)rf * 9 / 10, 0, 0, 1, 0);
            if (rec_on) { cell_t *w = &song.pat[sq_pi % NPAT].c[t][r]; w->n = (uint8_t)(rpt_note + (TRK_TYPE[t] == TY_BREAK ? 1u : 0u)); w->v = 12; w->fx = 0; w->fv = 0; }
        }
        if (!n) continue;
        if (n == NOTE_OFF) { if (TRK_TYPE[t] == TY_SYNTH) xj_release(XS_JT[t]); continue; }
        if (tr->mute || (solo && !tr->solo)) continue;
        if (!song.loop && (song.x.smute[sq_chainpos & 15u] >> t) & 1u) continue;   /* (XENOISE: SEQ's matrix) */
        prob = tr->chance;
        if (c->fx == FX_G) prob = prob * (c->fv + 1) / 16;
        if (prob < 100 && (int32_t)((xr(&rnd_isr) >> 8) % 100u) >= prob) continue;
        if (TRK_TYPE[t] == TY_BREAK && punch_scat) { n = 1u + (xr(&rnd_isr) >> 8) % 16u; }
        wait = sw + (c->fx == FX_D ? (int32_t)(rf * c->fv / 16u) : 0);
        hits = (c->fx == FX_R && c->fv > 1u ? c->fv : 1u) * (punch_roll ? 2u : 1u);
        if (TRK_TYPE[t] == TY_BREAK && punch_scat && (xr(&rnd_isr) & 3u) == 0u) hits = 2u + (xr(&rnd_isr) >> 8) % 3u;
        vel = (int32_t)c->v * 2184;
        for (h = 0; h < hits; h++) {
            int32_t hv = hits > 1u ? vel - (int32_t)(vel * 4 / 10 * h / hits) : vel;
            int32_t g = hits > 1u ? (int32_t)(rf / hits) : (int32_t)(gate_rows(p, t, r) * rf);
            pend_add(wait + (int32_t)(rf * h / hits), t, n, hv, g, c->fx == FX_B || punch_rev, c->fx == FX_P ? (int32_t)c->fv - 8 : 0, hits > 1u, 0);
        }
    }
}
static void seq_chunk(void)
{
    uint32_t i;
    if (sq_req == 1u) {                         /* PLAY */
        sq_req = 0;
        sq_play = 1;
        sq_pos = 0;
        sq_frame = 0;
        sq_chainpos = (uint8_t)(sq_start < song.nchain ? sq_start : 0u);
        sq_pi = song.nchain > 1u && !song.loop ? song.chain[sq_chainpos] : song.cur;
        sq_jump = 0xFF;
        sq_rowlen = next_rowlen();
        row_fire();
    } else if (sq_req == 2u) {
        sq_req = 0;
        sq_play = 0;
        xs_stop();
        for (i = 0; i < NPEND; i++) pend[i].note = 0, pend[i].ctr = 0;
    }
    for (i = 0; i < NPEND; i++) {               /* what is due in this chunk */
        pend_t *e = &pend[i];
        if (!e->note && !e->ctr) continue;
        e->wait -= (int32_t)CH;
        if (e->wait > 0) continue;
        if (e->ctr & 4u)                        /* a break's roll, drag or flam: its lane again */
            xs_hit(XJ_BRK, e->note, e->vel);
        else if (e->ctr & 1u)
            xs_note(XJ_CTR, e->note, e->vel, e->gate);
        else
            trigger(e->trk, (e->ctr & 2u) ? 0u : (TRK_TYPE[e->trk] == TY_BREAK ? e->note - 1u : e->note), e->vel, e->gate, e->rev, e->pitch, e->retrig);
        e->note = 0;
        e->ctr = 0;
    }
    if (!sq_play) return;
    sq_frame += CH;
    if (sq_frame >= sq_rowlen) {
        const pat_t *p = &song.pat[sq_pi % NPAT];
        uint32_t L = p->len ? p->len : 16u;
        sq_frame -= sq_rowlen;
        sq_rowlen = next_rowlen();
        if (++sq_pos >= steps_of(L) || (sq_jump < 16u && sq_pos % meter_steps() == 0u)) {   /* the pattern's end, or a */
            sq_pos = 0;                                                                      /* bar line with a jump due */
            sq_end_pi = sq_pi;
            if (!exp_mark) exp_mark = exp_frames + CH;   /* (its first row sounds at the next chunk) */
            if (sq_jump < 16u) {                /* live: the part tapped on ARR, from its start */
                uint32_t k = sq_jump % (song.nchain ? song.nchain : 1u);
                sq_jump = 0xFF;
                sq_cue = 0xFF;
                sq_chainpos = (uint8_t)k;
                sq_pi = song.chain[k] % NPAT;
            } else if (sq_cue < NPAT) { sq_pi = sq_cue; sq_cue = 0xFF; }
            else if (song.nchain > 1u && !song.loop) { sq_chainpos = (uint8_t)((sq_chainpos + 1u) % song.nchain); sq_pi = song.chain[sq_chainpos] % NPAT; }
            if (punch_rev) rev_at = 0;
        }
        row_fire();
    }
}


/* ---- the block: JIANT's audio interrupt, between two rendered blocks (fx.c events_block) */
static uint32_t wb_blk_carry;
static void wb_block(uint32_t n)
{
    uint32_t k;
    while (evq_r != evq_w) {                    /* the pads */
        ev_t *e = &evq[evq_r % EVQ];
        if (e->type == EV_ON) trigger(e->trk, e->note, e->vel * 2184, TRK_TYPE[e->trk] == TY_SYNTH ? 44100 * 8 : 22050, 0, 0, 0);
        else if (e->type == EV_OFF) { if (TRK_TYPE[e->trk] == TY_SYNTH) xj_release(XS_JT[e->trk]); }
        else if (e->type == EV_ALLOFF) { for (k = 0; k < 6u; k++) xj_release(k); }
        evq_r++;
    }
    for (wb_blk_carry += n; wb_blk_carry >= CH; wb_blk_carry -= CH) {
        exp_frames += CH;
        seq_chunk();
        xs_gates(CH);
    }
}
