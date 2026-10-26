/* SPDX-License-Identifier: GPL-3.0-only */
/* Wonderblue-OS: writing music. A genre's roll (tempo, swing, key, progression, breaks, drum lanes, the parts' styles
 * and sounds), its eight sections A..H (XENOISE: a song's parts, intro, verse, chorus, second verse, bridge, build, drop,
 * outro; WND-1's: main, variation, breakdown, build, other groove, half-time, chopped, outro) and the form they play in,
 * FILL and DICE on one track, the chord progressions (moving bass, keys and synth to new chords, keeping their rhythm),
 * and SIT (the machine plays itself). The same rules as the browser version (z-noise.html), in integers. */

static const wb_genre_t *genre(void) { return &WB_GENRES[song.genre % WB_NGENRE]; }
static uint32_t pick_u8(const uint8_t *a, uint32_t n) { return a[rnd(n ? n : 1u)]; }

static void cell_set(cell_t *c, uint32_t n, uint32_t v, uint32_t fx, uint32_t fv)
{
    c->n = (uint8_t)n;
    c->v = (uint8_t)clampi((int32_t)v, 0, 15);
    c->fx = (uint8_t)fx;
    c->fv = (uint8_t)fv;
}
static void pat_blank(pat_t *p, uint32_t len)
{
    memset(p, 0, sizeof *p);
    p->len = (uint8_t)len;
}
static uint32_t prog_len(void) { return (uint32_t)clampi((int32_t)song.ndeg * song.rows, 16, (int32_t)NROW); }

/* the song's drum lanes: the main set or the other one */
static void song_lanes(int alt, const char **k, const char **s, const char **h)
{
    const wb_genre_t *g = genre();
    *k = g->k[song.roll[alt ? RL_K2 : RL_K] % (g->nk ? g->nk : 1u)];
    *s = g->s[song.roll[alt ? RL_S2 : RL_S] % (g->ns ? g->ns : 1u)];
    *h = g->h[song.roll[alt ? RL_H2 : RL_H] % (g->nh ? g->nh : 1u)];
}
static uint32_t default_mode(uint32_t t)
{
    int c = genre()->chords;
    switch (t) {
    case T_BRK: return WM_STRAIGHT;
    case T_BAS: return WL_ROOTS;
    case T_KEY: return c ? WK_PADS : WK_MOTIF;
    case T_SYN: return c ? WK_MOTIF : WK_PADS;
    default: return 1;                          /* (drums: their lanes; 0 would be off) */
    }
}
static uint32_t song_mode(uint32_t t, int alt)
{
    switch (t) {
    case T_BRK: return song.roll[alt ? RL_BRK2 : RL_BRK];
    case T_BAS: return song.roll[alt ? RL_BAS2 : RL_BAS];
    case T_KEY: return song.roll[alt ? RL_KEY2 : RL_KEY];
    case T_SYN: return song.roll[alt ? RL_SYN2 : RL_SYN];
    default: return 1;
    }
}
static void roll_new(void)
{
    const wb_genre_t *g = genre();
    uint32_t a;
    const uint8_t *L[4] = {g->brkm, g->basm, g->keym, g->synm};
    for (a = 0; a < 4u; a++) {
        uint32_t n = g->nm[a] ? g->nm[a] : 1u, m = L[a][rnd(n)], o = m;
        if (n > 1u) { uint32_t k = 0; do o = L[a][rnd(n)]; while (o == m && ++k < 16u); }   /* (XENOISE: bounded: a list of one kind ends) */
        song.roll[RL_BRK + a] = (uint8_t)m;
        song.roll[RL_BRK2 + a] = (uint8_t)o;
    }
    {
        uint32_t nk = g->nk ? g->nk : 1u, ns = g->ns ? g->ns : 1u, nh = g->nh ? g->nh : 1u;
        song.roll[RL_K] = (uint8_t)rnd(nk); song.roll[RL_K2] = (uint8_t)((song.roll[RL_K] + (nk > 1u ? 1u + rnd(nk - 1u) : 0u)) % nk);
        song.roll[RL_S] = (uint8_t)rnd(ns); song.roll[RL_S2] = (uint8_t)((song.roll[RL_S] + (ns > 1u ? 1u + rnd(ns - 1u) : 0u)) % ns);
        song.roll[RL_H] = (uint8_t)rnd(nh); song.roll[RL_H2] = (uint8_t)((song.roll[RL_H] + (nh > 1u ? 1u + rnd(nh - 1u) : 0u)) % nh);
    }
}

/* ---- one track of one pattern */
static void gen_track(uint32_t pi, uint32_t t, uint32_t mode, uint32_t dens, const char *kl, const char *sl, const char *hl)
{
    pat_t *p = &song.pat[pi];
    uint32_t L = p->len, bars = L / 16u, b, i, k;
    static const uint8_t ORDER[7][4] = {{0, 1, 2, 3}, {0, 1, 2, 3}, {0, 1, 0, 3}, {0, 2, 1, 3}, {0, 1, 3, 2}, {0, 3, 2, 1}, {0, 1, 2, 1}};
    static const uint8_t RH[6][6] = {{0, 3, 6, 10, 99, 99}, {0, 2, 4, 7, 10, 12}, {0, 6, 8, 14, 99, 99}, {2, 5, 8, 11, 14, 99},
                                     {0, 3, 7, 10, 13, 99}, {0, 4, 6, 8, 12, 99}};
    static const int8_t ARP[4][8] = {{0, 2, 4, 7, 4, 2, 0, 2}, {0, 4, 2, 7, 0, 4, 2, 9}, {7, 4, 2, 0, 2, 4, 7, 9}, {0, 2, 4, 2, 0, 4, 7, 4}};
    const uint8_t *rh = RH[rnd(6)];
    const int8_t *ap = ARP[rnd(4)];
    int8_t md[6], ml[6];
    int32_t d0;
    for (i = 0; i < NROW; i++) memset(&p->c[t][i], 0, sizeof(cell_t));
    if (mode == WK_OFF && TRK_TYPE[t] == TY_SYNTH) return;
    if (t == T_BRK && mode == WM_OFF) return;
    {   /* the motif: one bar's melody, every bar on that bar's chord. (XENOISE) A shape, not a random walk: it rises,
         * arches, falls or waves from a chord tone; on the strong beats a chord tone (0 2 4: the root, third, fifth over
         * the bar's chord), between them steps. The bars then say it as a phrase (below: call, answer, call, home) */
        static const int8_t SHAPE[4][6] = {{0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 2, 1}, {4, 3, 2, 1, 0, -1}, {0, 2, 1, 3, 2, 4}};
        static const int8_t TONES[6] = {-3, 0, 2, 4, 7, 9};
        uint32_t sh = rnd(4);
        d0 = (const int8_t[]){0, 2, 4}[rnd(3)];
        if (sh == 2u) d0 = 2 + (int32_t)rnd(2) * 2;   /* (a falling one from higher up) */
        for (i = 0; i < 6u; i++) {
            int32_t v = d0 + SHAPE[sh][i] - SHAPE[sh][0];
            if (rh[i] < 16u && rh[i] % 4u == 0u) {   /* a strong beat: the nearest chord tone */
                uint32_t q, bq = 0;
                for (q = 1; q < 6u; q++) if (clampi(v - TONES[q], -20, 20) * clampi(v - TONES[q], -20, 20) < clampi(v - TONES[bq], -20, 20) * clampi(v - TONES[bq], -20, 20)) bq = q;
                v = TONES[bq];
            }
            md[i] = (int8_t)clampi(v, -3, 9);
            ml[i] = (int8_t)(2 + rnd(3));
        }
    }
    for (b = 0; b < bars; b++) {
        uint32_t o = b * 16u, last = b + 1u == bars;
        cell_t *c = &p->c[t][o];
#define CH_AT(i) ((o + (i)) == 0u || prog_deg(o + (i)) != prog_deg(o + (i) - 1u))
        switch (TRK_TYPE[t]) {
        case TY_BREAK:
            if (mode == WM_STRAIGHT || mode == WM_CHOP) {
                const uint8_t *ord = ORDER[rnd(7)];
                for (i = 0; i < 16u; i++) {
                    uint32_t beat = i / 4u, from = ord[beat];
                    if (beat && rnd(100) < (mode == WM_CHOP ? 35u : dens * 20u / 100u)) from = rnd(4);
                    if (mode == WM_CHOP && (i & 1u) && rnd(100) < 18u) continue;
                    cell_set(&c[i], from * 4u + (i & 3u) + 1u, (i & 3u) == 0u ? 15u : (i & 1u) ? 10u : 13u, 0, 0);
                }
                if (last && rnd(100) < 40u + dens / 2u) { cell_set(&c[14], rnd(16) + 1u, 12, FX_R, 2u + rnd(3)); cell_set(&c[15], rnd(16) + 1u, 11, FX_R, rnd(2) ? 2u : 4u); }
            } else if (mode == WM_HALF) {
                for (i = 0; i < 16u; i += 2u) cell_set(&c[i], (b & 1u) * 8u + i / 2u + 1u, i % 8u ? 11u : 15u, 0, 0);
            } else if (mode == WM_STORM) {
                for (i = 0; i < 16u; i++) {
                    uint32_t r = rnd(100), fx = 0, fv = 0;
                    if (r < 8u) continue;
                    r = rnd(100);
                    if (r < 12u + dens * 15u / 100u) { fx = FX_R; fv = (uint32_t)(int32_t)(const uint8_t[]){2, 3, 4, 6, 8}[rnd(5)]; }
                    else if (r < 20u + dens / 10u) fx = FX_B;
                    else if (r < 24u + dens * 8u / 100u) { fx = FX_P; fv = (const uint8_t[]){12, 13, 2, 4}[rnd(4)]; }
                    cell_set(&c[i], (rnd(100) < 45u ? i : rnd(16)) + 1u, i % 4u ? 9u + rnd(5) : 15u, fx, fv);
                }
            } else if (mode == WM_SPARSE) {
                for (i = 0; i < 16u; i++) if (i % 8u == 0u || rnd(100) < dens / 5u) cell_set(&c[i], i + 1u, 8u + rnd(5), 0, 0);
            } else if (mode == WM_GHOST) {
                for (i = 0; i < 16u; i++) if ((i % 4u == 2u && rnd(100) < 55u) || ((i & 1u) && rnd(100) < dens * 15u / 100u)) cell_set(&c[i], rnd(16) + 1u, 5u + rnd(4), 0, 0);
            } else if (mode == WM_FILLS) {
                if (last) for (i = 12; i < 16u; i++) if (rnd(100) < 75u) cell_set(&c[i], rnd(16) + 1u, 10u + rnd(4), FX_R, 2u + rnd(3));
            } else if (mode == WM_STORMFILL) {
                for (i = 8; i < 16u; i++) if (rnd(100) < 35u + dens * 40u / 100u) cell_set(&c[i], rnd(16) + 1u, 10u + rnd(5), FX_R, (const uint8_t[]){2, 4, 6, 8, 12}[rnd(5)]);
            }
            break;
        case TY_KICK:
            if (!kl || !kl[0] || mode == WM_OFF) break;
            for (i = 0; i < 16u; i++) {
                char ch = kl[i];
                if (ch == 'x') cell_set(&c[i], 60, 13u + rnd(3), 0, 0);
                else if (ch == 'g') cell_set(&c[i], 60, 4u + rnd(3), 0, 0);
                else if (!(i & 1u) && rnd(100) < dens / 10u) cell_set(&c[i], 60, 8u + rnd(4), 0, 0);
            }
            break;
        case TY_DRUMS:                          /* the hats first, then the snare over them (it wins a shared row) */
            if (mode == WM_OFF) break;
            if (hl && hl[0])
                for (i = 0; i < 16u; i++) {
                    char ch = hl[i];
                    if (ch == 'x') cell_set(&c[i], DN_HAT, i % 4u == 0u ? 12u : (i & 1u) ? 7u : 10u, 0, 0);
                    else if (ch == 'g') cell_set(&c[i], DN_HAT, 5u + rnd(3), 0, 0);
                    else if (ch == 'o') cell_set(&c[i], DN_OPEN, 10, 0, 0);
                    else if (rnd(100) < dens * 6u / 100u) cell_set(&c[i], DN_HAT, 6, FX_R, 3);
                }
            if (sl && sl[0])
                for (i = 0; i < 16u; i++) {
                    char ch = sl[i];
                    if (ch == 'x') cell_set(&c[i], DN_SNR, 13u + rnd(3), 0, 0);
                    else if (ch == 'g') cell_set(&c[i], DN_SNR, 4u + rnd(3), 0, 0);
                    else if (ch == 'o') cell_set(&c[i], DN_CLAP, 12, 0, 0);
                    else if (!c[i].n && rnd(100) < dens / 20u) cell_set(&c[i], DN_SNR, 4, 0, 0);
                }
            if (sl && sl[0] && last && rnd(100) < dens / 2u) cell_set(&c[15], DN_SNR, 9, FX_R, 2u + rnd(2));
            break;
        default: {
            int32_t r0;
#define N(dd, i) ((uint32_t)fold_oct(deg_midi(r0 + prog_deg(o + (i)) + (dd))))
            if (t == T_BAS) {
                static const int8_t A[8] = {0, 0, 0, 7, 2, 4, -7, 1};
                r0 = deg_at(36 + song.key);
                switch (mode) {
                case WL_ROOTS:
                    for (i = 0; i < 16u; i++)
                        if ((kl && kl[i] == 'x') || CH_AT(i)) {
                            cell_set(&c[i], N(i && rnd(5) == 0u ? 4 : 0, i), 12, 0, 0);
                            if (i + 3u < 16u && kl && kl[i + 1u] != 'x' && kl[i + 2u] != 'x' && kl[i + 3u] != 'x' && rnd(100) < 60u) cell_set(&c[i + 3u], NOTE_OFF, 0, 0, 0);
                        }
                    break;
                case WL_WALK: {
                    static const uint8_t ROW[5] = {0, 6, 8, 11, 14};
                    int8_t ds[5] = {0, (int8_t)(rnd(2) ? 2 : 4), 4, (int8_t)(const int8_t[]){5, 6, 3}[rnd(3)], (int8_t)(const int8_t[]){1, -1, 7}[rnd(3)]};
                    for (k = 0; k < 5u; k++) if (!k || rnd(100) < 55u + dens * 30u / 100u) cell_set(&c[ROW[k]], N(ds[k], ROW[k]), k ? 10u : 13u, 0, 0);
                    for (i = 1; i < 16u; i++) if (CH_AT(i) && !c[i].n) cell_set(&c[i], N(0, i), 12, 0, 0);
                    break;
                }
                case WL_SUB:
                    for (i = 0; i < 16u; i++) if (CH_AT(i)) cell_set(&c[i], N(0, i), 13, 0, 0);
                    if (!c[10].n && rnd(2)) cell_set(&c[10], N(rnd(2) ? 4 : 0, 10), 10, 0, 0);
                    break;
                case WL_OFFBEAT:
                    for (i = 2; i < 16u; i += 4u) cell_set(&c[i], N(rnd(5) == 0u ? 7 : 0, i), 12, 0, 0);
                    break;
                case WL_EIGHTHS:
                    for (i = 0; i < 16u; i += 2u) cell_set(&c[i], N(i == 12u && rnd(5) < 2u ? 4 : 0, i), i % 4u ? 10u : 13u, 0, 0);
                    break;
                default:                        /* ACID */
                    for (i = 0; i < 16u; i++) {
                        if (i && rnd(100) >= 55u + dens * 30u / 100u) { if (rnd(100) < 30u) cell_set(&c[i], NOTE_OFF, 0, 0, 0); continue; }
                        cell_set(&c[i], N(A[rnd(8)], i), rnd(100) < 30u ? 15u : 9u, 0, 0);
                    }
                }
            } else if (mode >= WK_PADS && mode != WK_OFF) {   /* KEY / SYN: chords */
                r0 = deg_at(55 + song.key);
                if (mode == WK_PADS) { for (i = 0; i < 16u; i++) if (CH_AT(i)) cell_set(&c[i], N(0, i), 9, 0, 0); }
                else if (mode == WK_SWELL) { for (i = 0; i < 16u; i++) if (CH_AT(i)) cell_set(&c[i], N(0, i), 9, 0, 0); if (!c[13].n) cell_set(&c[13], NOTE_OFF, 0, 0, 0); }
                else if (mode == WK_STABS) {
                    static const uint8_t R[5][4] = {{0, 10, 99, 99}, {3, 10, 99, 99}, {0, 6, 12, 99}, {2, 8, 99, 99}, {2, 6, 10, 14}};
                    const uint8_t *r = R[rnd(5)];
                    for (k = 0; k < 4u && r[k] < 16u; k++) { cell_set(&c[r[k]], N(0, r[k]), 11, 0, 0); if (r[k] + 2u < 16u && !c[r[k] + 2u].n) cell_set(&c[r[k] + 2u], NOTE_OFF, 0, 0, 0); }
                } else {                        /* JAZZ */
                    static const uint8_t R[4][3] = {{0, 7, 10}, {0, 3, 10}, {2, 7, 14}, {0, 10, 99}};
                    const uint8_t *r = R[rnd(4)];
                    for (k = 0; k < 3u && r[k] < 16u; k++) { cell_set(&c[r[k]], N(k && rnd(3) == 0u ? 2 : 0, r[k]), 9u + rnd(4), 0, 0); if (r[k] + 3u < 16u && !c[r[k] + 3u].n) cell_set(&c[r[k] + 3u], NOTE_OFF, 0, 0, 0); }
                }
            } else {                            /* KEY / SYN: a melody */
                r0 = deg_at((t == T_SYN ? 72 : 60) + song.key);
                /* (XENOISE) the phrase: each bar a role in four. 0 the call (it ends, a breath after it), 1 the answer
                 * (its second half a step lower, ending on the fifth, held: a question), 2 the call again, higher in its
                 * middle (the climb), 3 home (ending on the root or its octave, held). Two bars: call, home; one: home */
                uint32_t role = bars <= 1u ? 3u : bars == 2u ? (b ? 3u : 0u) : (b % 4u == 3u || last) ? 3u : b % 4u;
                if (mode == WK_MOTIF) {
                    for (k = 0; k < 6u && rh[k] < 16u; k++) {
                        int32_t dd = md[k];
                        int endk = k == 5u || rh[k + 1u] >= 16u, hold = endk && (role == 1u || role == 3u);
                        if (role == 1u && rh[k] >= 8u) dd -= 1;
                        if (role == 2u && rh[k] >= 4u && rh[k] < 12u) dd += 2;
                        if (endk && role == 1u) dd = 4;
                        if (endk && role == 3u) dd = rnd(3) ? 0 : 7;
                        cell_set(&c[rh[k]], N(dd, rh[k]), k ? 9u + rnd(3) + (role == 2u ? 1u : 0u) : 12u, 0, 0);
                        if (hold) continue;             /* (held: it rings into the next bar's first note) */
                        if ((uint32_t)(rh[k] + ml[k]) < 16u && !c[rh[k] + ml[k]].n) {
                            uint32_t j, free = 1;
                            for (j = k + 1u; j < 6u && rh[j] < 16u; j++) if (rh[j] == rh[k] + ml[k]) free = 0;
                            if (free) cell_set(&c[rh[k] + ml[k]], NOTE_OFF, 0, 0, 0);
                        }
                    }
                } else if (mode == WK_LEAD) {       /* (XENOISE) long notes saying the motif's shape, in the same roles */
                    static const uint8_t R[5][3] = {{0, 8, 99}, {0, 6, 12}, {4, 12, 99}, {0, 10, 99}, {2, 8, 14}};
                    const uint8_t *r = R[(pi + t) % 5u];   /* (one rhythm a pattern: a line, not a scatter) */
                    for (k = 0; k < 3u && r[k] < 16u; k++) {
                        int endk = k == 2u || r[k + 1u] >= 16u;
                        int32_t dd = md[k * 2u] + (role == 2u ? 2 : 0);
                        if (endk && role == 1u) dd = 4;
                        if (endk && role == 3u) dd = rnd(3) ? 0 : 7;
                        cell_set(&c[r[k]], N(dd, r[k]), 9u + rnd(3) + (role == 2u ? 2u : 0u), 0, 0);
                    }
                    if (role == 0u) cell_set(&c[15], NOTE_OFF, 0, 0, 0);
                } else {                        /* ARP */
                    for (i = 0; i < 16u; i += 2u) if (!i || rnd(100) < 70u + dens * 30u / 100u) cell_set(&c[i], N(ap[i / 2u], i), i % 4u ? 8u : 11u, 0, 0);
                }
            }
#undef N
        }
        }
#undef CH_AT
    }
}

/* ---- DICE: change a track a little (in the scale, around the anchors) */
static pat_t seed_pat;                          /* RESTORE: the current pattern before FILL / DICE / CLEAR */
static uint8_t seed_of = 0xFF;
static void seed_take(uint32_t pi)
{
    if (seed_of == pi) return;
    seed_pat = song.pat[pi];
    seed_of = (uint8_t)pi;
}
static void mutate_track(uint32_t pi, uint32_t t, uint32_t a)   /* a: 0..100 */
{
    pat_t *p = &song.pat[pi];
    uint32_t r;
    for (r = 0; r < p->len; r++) {
        cell_t *c = &p->c[t][r];
        if (rnd(1000) >= a * 3u) continue;
        switch (TRK_TYPE[t]) {
        case TY_BREAK:
            if (c->n && c->n != NOTE_OFF) {
                uint32_t q = rnd(100);
                if (q < 45u) c->n = (uint8_t)(rnd(16) + 1u);
                else if (q < 65u) { c->fx = FX_R; c->fv = (uint8_t)(const uint8_t[]){2, 3, 4, 6}[rnd(4)]; }
                else if (q < 72u) { c->fx = FX_B; c->fv = 0; }
                else if (q < 80u && r % 16u) memset(c, 0, sizeof *c);
                else { c->fx = 0; c->fv = 0; }
            } else if (!c->n && rnd(10) < 4u) cell_set(c, r % 16u + 1u, 7u + rnd(5), 0, 0);
            break;
        case TY_KICK: case TY_DRUMS: {          /* the strong hits stay; ghost notes and hats come and go */
            int hat = t == T_DRM && (!c->n || drm_kind(c->n) == DK_HAT), keep = c->n && c->n != NOTE_OFF && !hat && c->v >= 12u && r % 4u == 0u;
            if (c->n && !keep && rnd(10) < 6u) memset(c, 0, sizeof *c);
            else if (!c->n) {
                int roll = hat && rnd(5) == 0u;
                cell_set(c, t == T_KIK ? 60u : rnd(3) ? DN_HAT : DN_SNR, hat ? 5u + rnd(5) : 4u + rnd(6), roll ? FX_R : 0u, roll ? 2u : 0u);
            }
            break;
        }
        default:
            if (c->n && c->n != NOTE_OFF) {
                int32_t d = deg_at(c->n);
                c->n = (uint8_t)fold_oct(rnd(10) < 7u ? deg_midi(d + (const int8_t[]){-2, -1, 1, 2}[rnd(4)]) : deg_midi(d + (rnd(2) ? 7 : -7)));
            } else if (!c->n && rnd(4) == 0u) {
                int32_t k;
                for (k = (int32_t)r - 1; k >= 0; k--)
                    if (p->c[t][k].n && p->c[t][k].n != NOTE_OFF) { cell_set(c, (uint32_t)fold_oct(deg_midi(deg_at(p->c[t][k].n) + (const int8_t[]){0, 2, 4}[rnd(3)])), 7u + rnd(4), 0, 0); break; }
            }
        }
    }
}

/* ---- (XENOISE) the song's form. The eight patterns are its parts; the chain plays them as a song (SEQ: FORM picks
 * one of four, SHUFFLE writes another that still makes sense: an intro first, verses before choruses, a build before
 * every drop, an outro last; or a jam's loop of verses and choruses) */
enum { S_INTRO, S_VERSE, S_CHORUS, S_VERSE2, S_BRIDGE, S_BUILD, S_DROP, S_OUTRO };
#define WB_NFORM 4u
static const char *const FORM_NAME[WB_NFORM] = {"SONG", "CLUB", "JAM", "EPIC"};
static const uint8_t FORM_LEN[WB_NFORM] = {10, 8, 6, 12};
static const uint8_t FORM[WB_NFORM][16] = {
    {S_INTRO, S_VERSE, S_CHORUS, S_VERSE2, S_CHORUS, S_BRIDGE, S_BUILD, S_DROP, S_CHORUS, S_OUTRO},
    {S_INTRO, S_BUILD, S_DROP, S_VERSE, S_BRIDGE, S_BUILD, S_DROP, S_OUTRO},
    {S_VERSE, S_CHORUS, S_VERSE2, S_CHORUS, S_BRIDGE, S_CHORUS},
    {S_INTRO, S_VERSE, S_VERSE2, S_CHORUS, S_BRIDGE, S_VERSE, S_CHORUS, S_BUILD, S_DROP, S_DROP, S_CHORUS, S_OUTRO},
};
static void form_set(uint32_t f)
{
    uint32_t i;
    f %= WB_NFORM;
    for (i = 0; i < FORM_LEN[f]; i++) song.chain[i] = FORM[f][i];
    song.nchain = FORM_LEN[f];
}
static int form_of(void)                        /* the form the chain is (-1: the user's own) */
{
    uint32_t f, i;
    for (f = 0; f < WB_NFORM; f++) {
        if (song.nchain != FORM_LEN[f]) continue;
        for (i = 0; i < FORM_LEN[f] && song.chain[i] == FORM[f][i]; i++) {}
        if (i == FORM_LEN[f]) return (int)f;
    }
    return -1;
}
static void form_shuffle(void)
{
    uint8_t c[24];
    uint32_t n, k, tries, cycles, drop_at, bridge_at, i;
    for (tries = 0; tries < 8u; tries++) {
        n = 0;
        cycles = 2u + rnd(2);
        drop_at = rnd(cycles);
        bridge_at = rnd(cycles);
        if (rnd(4)) c[n++] = S_INTRO;
        for (k = 0; k < cycles; k++) {
            if (k == 0u || rnd(3)) c[n++] = (uint8_t)(rnd(2) ? S_VERSE : S_VERSE2);
            if (!rnd(4)) { uint8_t pv = c[n - 1u]; c[n++] = (uint8_t)(pv == S_VERSE ? S_VERSE2 : S_VERSE); }
            c[n++] = S_CHORUS;
            if (k + 1u == cycles && rnd(2)) c[n++] = S_CHORUS;
            if (k == bridge_at && k + 1u < cycles) c[n++] = S_BRIDGE;
            if (k == drop_at) {
                c[n++] = S_BUILD;
                c[n++] = S_DROP;
                if (!rnd(3)) c[n++] = S_DROP;
            }
        }
        if (rnd(4)) c[n++] = S_OUTRO;
        if (n > 16u) { c[15] = c[n - 1u]; n = 16u; }   /* (the end kept: an outro stays the last) */
        if (n != song.nchain) break;
        for (i = 0; i < n && c[i] == song.chain[i]; i++) {}
        if (i < n) break;                       /* (another than the one there) */
    }
    for (i = 0; i < n; i++) song.chain[i] = c[i];
    song.nchain = (uint8_t)n;
}
/* the recipe each part is written by (gen_pattern's v): the bridge a breakdown or half-time, the song decides */
static uint32_t sect_v(uint32_t pi)
{
    static const uint8_t V[NPAT] = {8, 0, 9, 4, 2, 3, 6, 7};
    pi %= NPAT;
    return pi == S_BRIDGE ? ((song.bpm + song.genre) & 1u ? 5u : 2u) : V[pi];
}

/* ---- a section A..H into pattern pi */
static void gen_pattern(uint32_t pi, uint32_t v)
{
    const wb_genre_t *g = genre();
    const char *k1, *s1, *h1, *k2, *s2, *h2;
    uint32_t t;
    int chd = g->chords;
    song_lanes(0, &k1, &s1, &h1);
    song_lanes(1, &k2, &s2, &h2);
    pat_blank(&song.pat[pi], prog_len());
    for (t = 0; t < NT; t++) {
        uint32_t mode = song_mode(t, 0), dens = song.trk[t].density;
        int drum = t == T_KIK || t == T_DRM, brk_on = !(t == T_BRK && mode == WM_OFF);
        const char *kl = k1, *sl = s1, *hl = h1;
        switch (v) {
        case 1: dens = dens + 20u > 100u ? 100u : dens + 20u; break;
        case 2:
            if (drum) mode = WM_OFF;
            if (t == T_BRK && brk_on) mode = WM_SPARSE;
            if (t == T_BAS) mode = WL_SUB;
            if (t == T_KEY) mode = chd ? WK_PADS : song_mode(t, 1);
            if (t == T_SYN) mode = chd ? WK_OFF : WK_PADS;
            break;
        case 3:
            dens = dens + 35u > 100u ? 100u : dens + 35u;
            if (t == T_BRK && brk_on && g->storm) mode = WM_STORM;
            if (t == T_DRM) dens = 100;
            break;
        case 4: kl = k2; sl = s2; hl = h2; if (!drum) mode = song_mode(t, 1); break;
        case 5:
            kl = WB_HALF_LANE[0]; sl = WB_HALF_LANE[1]; hl = WB_HALF_LANE[2];
            if (t == T_BRK && brk_on) mode = WM_HALF;
            if (t == T_KEY && !chd) mode = WK_LEAD;
            if (t == T_SYN && chd) mode = WK_LEAD;
            break;
        case 6:
            dens = dens + 25u > 100u ? 100u : dens + 25u;
            if (t == T_BRK && brk_on) mode = g->storm ? WM_STORM : WM_CHOP;
            break;
        case 8:                                 /* (XENOISE) INTRO: no kick, the hats alone, the break thin, pads */
            if (t == T_KIK) mode = WM_OFF;
            if (t == T_DRM) sl = "";
            if (t == T_BRK && brk_on) mode = WM_SPARSE;
            if (t == T_BAS) mode = WL_SUB;
            if (t == T_KEY) mode = chd ? WK_PADS : song_mode(t, 1);
            if (t == T_SYN) mode = WK_OFF;
            dens = dens > 20u ? dens - 20u : 0u;
            break;
        case 9:                                 /* (XENOISE) CHORUS: the main groove fuller, a lead over it */
            dens = dens + 20u > 100u ? 100u : dens + 20u;
            if (t == T_KEY && !chd) mode = WK_LEAD;
            if (t == T_SYN && chd) mode = WK_LEAD;
            break;
        case 7:
            if (t == T_KIK) mode = WM_OFF;
            if (t == T_DRM) { sl = ""; hl = WB_HALF_LANE[2]; }
            if (t == T_BRK && brk_on) mode = WM_SPARSE;
            if (t == T_BAS) mode = WL_SUB;
            if (t == T_KEY) mode = chd ? WK_SWELL : song_mode(t, 1);
            if (t == T_SYN) mode = chd ? song_mode(t, 1) : WK_SWELL;
            break;
        }
        gen_track(pi, t, mode, dens, kl, sl, hl);
    }
    if (v == 1u) for (t = 0; t < NT; t++) mutate_track(pi, t, 25);
    if (v == 6u) mutate_track(pi, T_BRK, 35);
}

/* ---- sounds */
static void sound_patch(uint32_t t, uint32_t k)   /* a factory sound onto a track */
{
    int16_t *p = song.trk[t].p;
    uint32_t i;
    switch (TRK_TYPE[t]) {
    case TY_KICK: k %= WB_NKICK; for (i = 0; i < 4u; i++) p[i] = WB_KICK[k][i]; break;
    case TY_DRUMS: k %= WB_NSNARE; for (i = 0; i < 5u; i++) p[i] = WB_SNARE[k][i]; break;
    case TY_SYNTH: k %= WB_NSYN; for (i = 0; i < 16u; i++) p[i] = WB_SYN[k][i]; break;
    default: return;
    }
    song.trk[t].patch = (uint8_t)k;
}
static void hat_patch(uint32_t t, uint32_t k)     /* DRM's hats */
{
    uint32_t i;
    k %= WB_NHAT;
    for (i = 0; i < 4u; i++) song.trk[t].p[PH_TUNE + i] = WB_HAT[k][i];
    song.trk[t].patch2 = (uint8_t)k;
}
static uint32_t patch_count(uint32_t t)
{
    static const uint8_t N[4] = {0, WB_NKICK, WB_NSNARE, WB_NSYN};
    return N[TRK_TYPE[t]];
}
static const char *patch_name(uint32_t t, uint32_t k)
{
    switch (TRK_TYPE[t]) {
    case TY_KICK: return WB_KICK_NAME[k % WB_NKICK];
    case TY_DRUMS: return WB_SNARE_NAME[k % WB_NSNARE];
    case TY_SYNTH: return WB_SYN_NAME[k % WB_NSYN];
    default: return "";
    }
}
static void track_defaults(uint32_t t)
{
    static const uint8_t VOL[NT] = {60, 50, 44, 66, 58, 48}, REV[NT] = {6, 0, 12, 0, 30, 40}, DLY[NT] = {0, 0, 0, 0, 20, 12};
    track_t *tr = &song.trk[t];
    memset(tr, 0, sizeof *tr);
    tr->vol = VOL[t];
    tr->rev = REV[t];
    tr->dly = DLY[t];
    tr->chance = 100;
    tr->density = 50;
    tr->patch = 0;
    switch (TRK_TYPE[t]) {
    case TY_BREAK: tr->p[PB_SRC] = 0; tr->p[PB_GATE] = 100; tr->p[PB_TONE] = 100; break;
    case TY_SYNTH: sound_patch(t, t == T_BAS ? 0u : t == T_KEY ? 3u : 4u); tr->p[PY_CTRP] = 6; tr->p[PY_CTRL] = 45; break;
    case TY_DRUMS: sound_patch(t, 0); hat_patch(t, 0); break;
    default: sound_patch(t, 0); break;
    }
}

/* ---- a new song from a genre: the roll, the sounds, A..H, the chain */
static void song_name(void)
{
    const char *a = genre()->name;
    uint32_t n = 0, k = 10u + rnd(90);
    memset(song.name, 0, sizeof song.name);
    while (*a && *a != ' ' && *a != '-' && n < 10u) song.name[n++] = *a++;
    song.name[n++] = ' ';
    a = NOTE_NM[song.key % 12u];
    while (*a) song.name[n++] = *a++;
    song.name[n++] = ' ';
    song.name[n++] = (char)('0' + k / 10u);
    song.name[n++] = (char)('0' + k % 10u);
}
static void song_new(uint32_t gi)
{
    const wb_genre_t *g;
    uint32_t t, i, pid;
    memset(&song, 0, sizeof song);
    song.magic = SONG_MAGIC;
    song.version = SONG_VERSION;
    song.x.magic = SONG_EXT_MAGIC;              /* (XENOISE: the extension, nothing set) */
    song.genre = (uint8_t)(gi % WB_NGENRE);
    g = genre();
    pid = g->nprog ? g->prog[rnd(g->nprog)] : 0u;
    song.prog = (uint8_t)pid;
    if (pid == 0u || WB_PROGS[pid].scale == 255u) {   /* VIBE: the genre's own */
        song.scale = g->scale[rnd(g->nscale ? g->nscale : 1u)];
        song.ndeg = g->ndeg;
        for (i = 0; i < 8u; i++) song.deg[i] = g->deg[i];
    } else {
        song.scale = WB_PROGS[pid].scale;
        song.ndeg = WB_PROGS[pid].n;
        for (i = 0; i < 8u; i++) song.deg[i] = WB_PROGS[pid].deg[i];
    }
    if (!song.ndeg) { song.ndeg = 1; song.deg[0] = 0; }
    song.rows = song.ndeg > 4u ? 8u : 16u;
    song.key = (uint8_t)(const uint8_t[]){0, 2, 4, 5, 7, 9, 10, 11, 1, 3}[rnd(10)];
    song.bpm = (uint8_t)(g->bpm[0] + rnd((uint32_t)(g->bpm[1] - g->bpm[0]) + 1u));
    song.swing = (uint8_t)(g->swing[0] + rnd((uint32_t)(g->swing[1] - g->swing[0]) + 1u));
    song.age = 0;                               /* clean: tape AGE and CRUSH are there to dial in (MST) */
    song.crush = 0;
    song.space = g->space;
    song.pump = g->pump;
    song.mvol = 100;                            /* (XENOISE: VOLUME is the master's now, 100 % its 0 dB) */
    for (t = 0; t < NT; t++) track_defaults(t);
    {                                           /* the sounds from the genre's pools: kick, snare, hats, bass, keys */
        static const uint8_t TO[5] = {T_KIK, T_DRM, T_DRM, T_BAS, T_KEY};
        for (t = 0; t < 5u; t++) {
            uint32_t n = g->nsnd[t];
            if (!n) continue;
            if (t == 2u) hat_patch(T_DRM, g->snd[t][rnd(n)]);
            else sound_patch(TO[t], g->snd[t][rnd(n)]);
        }
    }
    if (g->nsyn) sound_patch(T_SYN, g->syn[rnd(g->nsyn)]);
    if (g->space >= 85u) {                      /* (XENOISE) the most spacious genres: their tracks deep in the reverb and the
                                                 * delay (a dreamscape: the bell's echoes, the pads' wash; the drums further off) */
        static const uint8_t REV[NT] = {35, 20, 40, 12, 60, 75}, DLY[NT] = {10, 0, 15, 0, 40, 25};
        for (t = 0; t < NT; t++) { song.trk[t].rev = REV[t]; song.trk[t].dly = DLY[t]; }
    }
    song.trk[T_BRK].p[PB_SRC] = g->nbrk ? g->brk[rnd(g->nbrk)] : 0;
    song.trk[T_KEY].p[PY_CHORD] = g->chords ? (int16_t)(3 + rnd(2)) : 0;
    song.trk[T_SYN].p[PY_CHORD] = g->chords ? 0 : (int16_t)(2 + rnd(3));
    song.trk[T_KEY].p[PY_CTR] = g->nctr ? g->ctr[rnd(g->nctr)] : 0;
    {   /* (XENOISE) the counter voice: a sound of the genre's own keys, not KEY's (WND-1: one of six bright ones at random,
         * over any genre: it grated) */
        uint32_t n = g->nsnd[4], k = n ? g->snd[4][rnd(n)] : 6u, tries;
        for (tries = 0; tries < 6u && n > 1u && k == song.trk[T_KEY].patch; tries++) k = g->snd[4][rnd(n)];
        song.trk[T_KEY].p[PY_CTRP] = (int16_t)k;
        song.trk[T_KEY].p[PY_CTRL] = 35;        /* (under the keys: an answer, not a second lead) */
    }
    roll_new();
    song_name();
    for (i = 0; i < NPAT; i++) gen_pattern(i, sect_v(i));
    form_set(0);                                /* (XENOISE) SONG: intro, verse, chorus .. outro */
    song.cur = 0;
    seed_of = 0xFF;
}

static void song_empty(void)                    /* a blank page: the sounds and tempo of the genre it had, no notes, A alone */
{
    uint32_t i;
    song_new(song.genre % WB_NGENRE);
    for (i = 0; i < NPAT; i++) pat_blank(&song.pat[i], 16);
    song.nchain = 1;
    song.chain[0] = 0;
    song.cur = 0;
    song.loop = 0;
    memset(song.name, 0, sizeof song.name);
    memcpy(song.name, "EMPTY SONG", 10);
}

/* ---- chord progressions: every bass, keys and synth note moves from its row's old chord to its new one */
static int8_t undo_deg[8];
static uint8_t undo_n, undo_rows, undo_scale, undo_prog, undo_ok;
static cell_t undo_cells[NPAT][3][NROW];
static uint8_t undo_len[NPAT];
static void prog_undo_take(void)
{
    uint32_t p, t;
    for (p = 0; p < NPAT; p++) {
        undo_len[p] = song.pat[p].len;
        for (t = 0; t < 3u; t++) memcpy(undo_cells[p][t], song.pat[p].c[T_BAS + t], sizeof undo_cells[p][t]);
    }
    memcpy(undo_deg, song.deg, 8);
    undo_n = song.ndeg; undo_rows = song.rows; undo_scale = song.scale; undo_prog = song.prog; undo_ok = 1;
}
static int prog_undo(void)
{
    uint32_t p, t;
    if (!undo_ok) return 0;
    for (p = 0; p < NPAT; p++) {
        song.pat[p].len = undo_len[p];
        for (t = 0; t < 3u; t++) memcpy(song.pat[p].c[T_BAS + t], undo_cells[p][t], sizeof undo_cells[p][t]);
    }
    memcpy(song.deg, undo_deg, 8);
    song.ndeg = undo_n; song.rows = undo_rows; song.scale = undo_scale; song.prog = undo_prog; undo_ok = 0;
    return 1;
}
static void prog_set(const int8_t *deg, uint32_t n, uint32_t rows, uint32_t scale, uint32_t pid)
{
    uint32_t p, t, r;
    uint32_t osc = song.scale, nsc = scale < WB_NSCALE ? scale : song.scale, newlen = (uint32_t)clampi((int32_t)n * rows, 16, (int32_t)NROW);
    prog_undo_take();
    for (p = 0; p < NPAT; p++) {
        pat_t *pt = &song.pat[p];
        uint32_t L0 = pt->len ? pt->len : 16u, L = L0 > newlen ? L0 : newlen, any = 0;
        static cell_t tmp[NROW];
        for (t = 0; t < NT && !any; t++) for (r = 0; r < L0; r++) if (pt->c[t][r].n) { any = 1; break; }
        if (!any) continue;
        for (t = 0; t < NT; t++) {
            for (r = 0; r < L; r++) {
                cell_t c = pt->c[t][r % L0];
                if (t >= T_BAS && c.n && c.n != NOTE_OFF) {
                    int32_t d = deg_at_s(c.n, osc), chrom = (int32_t)c.n - deg_midi_s(d, osc);
                    c.n = (uint8_t)fold_oct(deg_midi_s(d - prog_deg_of(song.deg, song.ndeg, song.rows, r % L0) + prog_deg_of(deg, n, rows, r), nsc) + chrom);
                }
                tmp[r] = c;
            }
            memcpy(pt->c[t], tmp, L * sizeof(cell_t));
        }
        pt->len = (uint8_t)L;
    }
    song.scale = (uint8_t)nsc;
    for (p = 0; p < 8u; p++) song.deg[p] = p < n ? deg[p] : 0;
    song.ndeg = (uint8_t)n;
    song.rows = (uint8_t)rows;
    song.prog = (uint8_t)pid;
    seed_of = 0xFF;
}
static void prog_pick(uint32_t pid)             /* a preset (0: the genre's own) */
{
    const wb_genre_t *g = genre();
    if (pid >= WB_NPROG - 1u) return;           /* (the last one is CUSTOM: the pads make it) */
    if (pid == 0u || WB_PROGS[pid].scale == 255u) prog_set(g->deg, g->ndeg ? g->ndeg : 1u, song.rows, g->scale[0], 0);
    else prog_set(WB_PROGS[pid].deg, WB_PROGS[pid].n, song.rows, WB_PROGS[pid].scale, pid);
}
static void transpose_to(uint32_t k)
{
    int32_t d = (int32_t)k - (int32_t)song.key;
    uint32_t p, t, r;
    if (d > 6) d -= 12;
    if (d < -6) d += 12;
    if (!d) return;
    for (p = 0; p < NPAT; p++)
        for (t = T_BAS; t < NT; t++)
            for (r = 0; r < NROW; r++) {
                cell_t *c = &song.pat[p].c[t][r];
                if (c->n && c->n != NOTE_OFF) c->n = (uint8_t)clampi((int32_t)c->n + d, 24, 108);
            }
    song.key = (uint8_t)(k % 12u);
}

/* ---- SIT: sections that leave tracks out, small mutations, another pattern now and then */
static const uint8_t SIT_SECT[6] = {0x07, 0x26, 0x00, 0x15, 0x16, 0x0F};   /* DRIFT DUST GROOVE PULSE BREAKS FLOAT (bit: muted) */
static const char *const SIT_NAME[6] = {"DRIFT", "DUST", "GROOVE", "PULSE", "BREAKS", "FLOAT"};
static uint8_t sit_on, sit_sect = 2, sit_left = 4;
static void sit_step(uint32_t pi)               /* at the end of each pattern while SIT plays */
{
    mutate_track(pi, rnd(NT), 12u + rnd(15));
    if (sit_left) sit_left--;
    if (!sit_left) {
        uint32_t s;
        do s = rnd(6); while (s == sit_sect);
        sit_sect = (uint8_t)s;
        sit_left = (uint8_t)(const uint8_t[]){2, 3, 4, 4, 6}[rnd(5)];
        sit_mute = SIT_SECT[s];
        if (rnd(10) < 6u) {
            uint32_t q = rnd(NPAT);
            sq_cue = (uint8_t)q;
        }
    }
}
