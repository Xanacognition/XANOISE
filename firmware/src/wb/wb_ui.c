/* SPDX-License-Identifier: GPL-3.0-only */
/* Wonderblue-OS: the panel and the screen, as the browser version laid out for the FM-1.
 *
 *   SCREEN         the tracker grid (or the page's own view), the status line, the four knobs, F1..F4
 *   KNOB 1..4      the page's knobs          SELECT  the cursor row      ALGORITHM  the track
 *   PRESETS        SND: the sound, SNG: the genre, CHD: the progression, else the pattern
 *   WHITE KEYS     the 16 pads (slices, drums, notes in the scale; chords on CHD; patterns on ARR)
 *   BLACK 1..6     the track: BRK KIK DRM BAS KEY SYN            BLACK 8..11  F1..F4
 *                  (XENOISE: on HOM they play, a ghost of the drum pad on their left or a synth's passing note;
 *                  HOME held, or SETTINGS > BLACK TRACKS: the tracks and F1..F4 there too)
 *   ALGORITHM      (XENOISE) the track, BRK .. SYN; on HOM past SYN: the visualizer (xn_vis.c)
 *   SAV            (XENOISE) K1 slot, K2 genre, K3 save, K4 load (OCT+ yes, OCT- no); OCT- a tap: delete (again: yes);
 *                  OCT+ a tap: a new empty song, four: a new song of the genre (OCT+ yes)
 *   HOME on HOM    (XENOISE) the tracker alone (the knobs rest, the grid longer); again: NOTE VEL FX AMT on the knobs
 *   GLO            (XENOISE) MST, the settings: again, its next page (TEMPO, TONE, GROOVE, SYSTEM: the theme, the black keys)
 *   SEQ            (XENOISE) the sequence's slots: K1 copy, K2 paste, K3 insert, K4 outsert, OCT+ add, OCT- delete the last,
 *                  PRESETS the slot's pattern; SEQ again: LIVE, the white keys queue patterns 1..8
 *   SAVE SNG  HOME TRK  EDIT SND  SCL CHD  LFO GEN  ENV MIX  GLO MST  SEQ (a tap) ARR (again: the page's next knobs)
 *   FX             held: PERFORM (white: punch-ins, black: mute / solo); held then pressed again: it stays
 *   SEQ + white    pattern 1..8 (9: loop / chain)     ARP + pad  note repeat (ARP held then pressed again: it stays)
 *   PLAY, REC, HOME + PLAY: SIT      OCT- / OCT+  the pads' octave
 * The store: sixteen slots of 16 KiB from STORE_LO, slot 0 the autosave. */

/* (XENOISE) the tabs in the panel's order: HOME, SAVE, SCL, then ENV (MIX) left of LFO (GEN) as on the FM-1, SEQ, EDIT,
 * GLO; renamed for what they hold (WND-1: TRK SNG CHD MIX GEN ARR SND MST) */
/* (XENOISE) the tabs: ENV (the envelope over the sound) and LFO where WND-1 had MIX and GEN; past them pages of their
 * own: GEN (LFO again), MIX (GLO again: the mixer), ARP (its button) */
enum { PG_TRK, PG_SNG, PG_CHD, PG_ENV, PG_LFO, PG_ARR, PG_SND, PG_MST, PG_N, PG_GEN = PG_N, PG_MIX, PG_ARP, PG_ALL };
static const char *const PG_NAME[PG_ALL] = {"HOM", "SAV", "CHD", "ENV", "LFO", "SEQ", "EDT", "MST", "GEN", "MIX", "ARP"};
static const char *const PG_HINT[PG_ALL] = {"SELECT ROW, PLAY PADS", "", "WHITE KEYS: CHORDS",
                                            "ENV AGAIN: PAGE 2", "LFO AGAIN: PAGE 2", "SEQ AGAIN: LIVE",
                                            "EDIT THE SOUND", "PRESETS GROUP, ALG PARAM", "HOLD SAVE: BACK",
                                            "ALG TRACK  GLO: BACK", "ARP AGAIN: PAGE 2"};
static uint32_t tab_of(uint32_t pg) { return pg == PG_GEN ? PG_SNG : pg == PG_MIX ? PG_MST : pg == PG_ARP ? 0xFFu : pg; }
#define MST_NSUB 4u                             /* (XENOISE) MST's pages: TEMPO, TONE, GROOVE, SYSTEM (GLO steps through them) */
static const char *const MST_SUB_NAME[MST_NSUB] = {"TEMPO", "TONE", "GROOVE", "SYSTEM"};
static const uint8_t PG_BTN[PG_N] = {BTN_HOME, BTN_SAVE, BTN_SEL, BTN_ENV, BTN_LFO, BTN_SEQ, BTN_EDIT, BTN_GLO};
static const char *const SECT_NAME[NPAT] = {"INTRO", "VERSE", "CHORUS", "VERSE2", "BRIDGE", "BUILD", "DROP", "OUTRO"};   /* (XENOISE: a song's parts) */

/* ---- the colours: the theme picked (xn_themes.h, the boot's splash shows it too) */
static const wb_theme_t *TH = &WB_THEMES[0];
#define C_BG (TH->bg)
#define C_ROW (TH->row)
#define C_BEAT (TH->beat)
#define C_PANEL (TH->panel)
#define C_INSET (TH->inset)
#define C_TXT (TH->txt)
#define C_NOTE (TH->note)
#define C_MUTED (TH->muted)
#define C_FAINT (TH->faint)
#define C_ORANGE (TH->play)                     /* (what plays: WND-1's orange, COMBAT's red) */
#define C_BLUE (TH->sel)                        /* (what is picked) */
#define C_CYAN (TH->chord)
#define C_REDC (TH->rec)
#define TRK_COL (TH->tcol)
#define KNOB_C (TH->kcol)

/* ---- the keys: white and black, in order */
#define KEY0 53u                                /* key 0 = F3 */
static uint8_t KEY_OF_WHITE[16], KEY_OF_BLACK[11];
static int8_t WHITE_OF_KEY[27], BLACK_OF_KEY[27];
static void keys_init(void)
{
    uint32_t k, w = 0, b = 0;
    for (k = 0; k < 27u; k++) {
        WHITE_OF_KEY[k] = BLACK_OF_KEY[k] = -1;
        if ((0x54Au >> ((KEY0 + k) % 12u)) & 1u) { BLACK_OF_KEY[k] = (int8_t)b; KEY_OF_BLACK[b++] = (uint8_t)k; }
        else { WHITE_OF_KEY[k] = (int8_t)w; KEY_OF_WHITE[w++] = (uint8_t)k; }
    }
}
#define BLK_F1 7u                               /* black keys 8..11: F1..F4 (7: none) */

/* ---- the UI's state */
static uint8_t page = PG_TRK, prev_page = PG_TRK, sub, cur_row, chd_slot, slot_sel = 1, genre_pick, prog_sel, arr_pos;
static uint8_t last_note[NT] = {0, 60, DN_SNR, 36, 60, 60};
static uint8_t arm_f = 0xFF;                    /* the F button waiting for its second press (SNG) */
static uint32_t arm_ms, toast_ms, frame_ms, dirty_ms, prog_ms, aud_off_ms, flash_ms[NT];
static uint8_t dirty, prog_due, aud_off_t = 0xFF;
static char toast_s[32];
static char knob_lab[4][16];
static uint32_t prev_keys, punch_keys, pad_keys;
static uint32_t taps[4], ntap;
static uint8_t led_want[41];
/* the held buttons that can stay: FX (PERFORM) and ARP (note repeat). Held, then pressed again quickly: latched */
static uint8_t fx_latch, arp_latch, perf_solo, seq_used;
static uint32_t page_ms;                        /* when the page opened (its F keys blink a moment) */
static uint8_t vis_on;                          /* (XENOISE) HOM: the visualizer (xn_vis.c) in place of the tracker */
static uint8_t vis_ar;                          /* (xn_vis.c: ALL RANGE, its start, where it looks) */
static uint32_t vis_ar_ms;
static uint16_t vis_yaw;
static uint8_t pilot_col;                       /* (XENOISE) the PILOT SYSTEM: the knob ALGORITHM is on (SELECT turns it) on
                                                 * pages with no tracks (MST, SAV, GEN) */
/* (XENOISE) SAV's actions: a knob's turn asks, OCT+ does it, OCT- does not (0..3: the F buttons' too) */
enum { SA_SAVE, SA_NEW, SA_LOAD, SA_DELETE, SA_COPY, SA_PASTE, SA_INSERT, SA_OUTSERT };
static uint8_t sng_clip = 0xFF;                 /* COPY's slot (PASTE and INSERT write it) */
static uint8_t arm_oct;                         /* DELETE asked by OCT- (OCT- again: yes, OCT+: no); a knob's: OCT+ yes */
static uint8_t fx_row, fx_col;                  /* (XENOISE) PERFORM's PILOT: the row (the punch-ins, the tracks), the column */
static uint8_t vis_arp_held, arp_used;          /* (ARP's tap or hold) */
static uint32_t arp_down_ms;                    /* (XENOISE) ARP down on the visualizer (held: ALL RANGE) */
static uint8_t save_used;                       /* SAVE held: GEN (its release then does nothing) */
static uint32_t save_down_ms;
#define ARR_ASK 0x10u                           /* (XENOISE) arm_f: SEQ's K1..K4 asked */
static void live_queue(uint32_t w);
static uint32_t live_n(void);
static uint8_t wb_resound;                      /* (XENOISE) tracks whose sound xs_sound loads again (LFO RESET) */
static int env_knob(uint32_t t, uint32_t i, int32_t dd, char *lab, char *val);   /* (xn_pages.c) */
static int lfo_knob(uint32_t t, uint32_t i, int32_t dd, char *lab, char *val);
static int arp_knob(uint32_t t, uint32_t i, int32_t dd, char *lab, char *val);
static int fx_knob(uint32_t i, int32_t dd, char *lab, char *val);
static uint32_t env_note(uint32_t t);
static uint32_t env_npages(uint32_t t);
static void fx_reset(void);
static uint8_t lfo_pg;
static uint8_t hom_edit, home_used;             /* (XENOISE) HOM: HOME again shows NOTE VEL FX AMT; HOME used with a key */
static uint8_t arr_live, arr_clip = 0xFF;       /* (XENOISE) SEQ: live (SEQ again), the copied slot's pattern */
static void vis_open(void);
static void vis_step(void);
static uint8_t sng_taps, sng_fx;                /* (XENOISE) SAV: OCT+ taps counted; the store's last action (its animation) */
static uint32_t sng_tap_ms, sng_fx_ms;
static int jam_nav(void) { return (xj_pref() >> 4) & 1u; }   /* SETTINGS: HOM's black keys the tracks and F1..F4 */
static void theme_apply(void) { TH = xn_theme_ptr(xn_theme_index()); g_force = 1; }
static uint32_t fx_rel_ms, arp_rel_ms, seq_down_ms;
#define LATCH_MS 400u
#define ARM_MS 8000u                            /* (XENOISE: a question waits 8 s, was 3) */
static int fx_on(void) { return fx_latch; }       /* (XENOISE: FX and ARP a tap on, a tap off; WND-1: held, or latched) */
static int arp_on(void) { return arp_latch; }

static void toast(const char *s)
{
    uint32_t i = 0;
    while (s[i] && i < 29u) { toast_s[i] = s[i]; i++; }
    toast_s[i] = 0;
    toast_ms = fm1_ms ? fm1_ms : 1u;
}
static void touch(void) { dirty = 1; dirty_ms = fm1_ms; }
static pat_t *P(void) { return &song.pat[song.cur % NPAT]; }
static uint32_t plen(void) { uint32_t l = P()->len; return l && l <= NROW ? l : 16u; }
static uint32_t trk(void) { return sel_trk % NT; }
static int quiet(void)
{
    return !xj_busy();                          /* (XENOISE: JIANT's voices) */
}
static void audition(uint32_t t, uint32_t n)    /* a short note on a track (the knobs, HEAR) */
{
    ev_push(EV_ON, t, n, 12);
    aud_off_t = (uint8_t)t;
    aud_off_ms = fm1_ms + (TRK_TYPE[t] == TY_SYNTH ? 450u : 0u);
}
static void drm_name(uint32_t n, char *o)       /* "SNR +2", "HHO -1" */
{
    static const char *const K[4] = {"SNR", "HHC", "HHO", "CLP"};
    char *e = str_cat((o[0] = 0, o), K[drm_kind(n)]);
    *e++ = ' ';
    *e = 0;
    fmt_s(e, drm_tune(n));
}

/* ================================================================ the store */
#define SLOT_SIZE 0x4000u
#define NSLOT 15u                               /* slot 0 the autosave, 14 for songs */
#define UNDO_SLOT NSLOT                         /* (XENOISE) one more: UNDO's (what the last change took) */
#define NSTORE (NSLOT + 1u)
typedef char song_fits_a_slot[sizeof(song_t) <= SLOT_SIZE ? 1 : -1];
static char slot_name[NSTORE][16];
static uint8_t slot_ok[NSTORE];
#if defined(DINGHY_HOST) || defined(KAWA_WASM) || defined(XN_WB_RAMSTORE)   /* (XENOISE: its emulator and tests) */
static uint8_t host_flash[NSTORE][SLOT_SIZE];
static uint32_t store_gen;
static int store_read(uint32_t k, uint32_t off, void *p, uint32_t n) { memcpy(p, host_flash[k] + off, n); return 0; }
static int store_write(uint32_t k, const void *p, uint32_t n) { memcpy(host_flash[k], p, n); store_gen++; return 0; }
static int store_erase(uint32_t k) { memset(host_flash[k], 0xFF, SLOT_SIZE); store_gen++; return 0; }
#else
static uint32_t slot_at(uint32_t k) { return STORE_LO + k * SLOT_SIZE; }
static int store_read(uint32_t k, uint32_t off, void *p, uint32_t n) { return st_read(slot_at(k) + off, p, n); }
static int store_erase(uint32_t k)             /* (XENOISE: JIANT's st_erase / st_prog, storage_hw.c; the emulator's NOR) */
{
    uint32_t s;
    int rc;
    for (s = 0; s < SLOT_SIZE; s += 0x1000u)
        if ((rc = st_erase(slot_at(k) + s)) != 0) return rc;
    return 0;
}
static int store_write(uint32_t k, const void *p, uint32_t n)
{
    int rc;
    if ((rc = store_erase(k)) != 0) return rc;
    if ((rc = st_prog(slot_at(k) + 8u, (const uint8_t *)p + 8u, n - 8u)) != 0)   /* the magic last: a cut save stays */
        return rc;                                                               /* invalid */
    return st_prog(slot_at(k), (const uint8_t *)p, 8u);
}
#endif
typedef char store_fits[STORE_LO + NSTORE * SLOT_SIZE <= STORE_HI ? 1 : -1];
static uint32_t crc_step(uint32_t c, const uint8_t *p, uint32_t n)
{
    uint32_t k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++) c = c & 1u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return c;
}
#define SONG_BODY ((uint32_t)((uint8_t *)&song.crc - (uint8_t *)&song))
static int slot_valid(uint32_t k, char *name)   /* the slot holds a song (its CRC checked a piece at a time) */
{
    uint8_t buf[256];
    uint32_t h[2], c = 0xFFFFFFFFu, off, crc;
    if (store_read(k, 0, h, 8) || h[0] != SONG_MAGIC || h[1] != SONG_VERSION) return 0;
    for (off = 0; off < SONG_BODY; off += sizeof buf) {
        uint32_t n = SONG_BODY - off < sizeof buf ? SONG_BODY - off : sizeof buf;
        if (store_read(k, off, buf, n)) return 0;
        c = crc_step(c, buf, n);
        if (off == 0u && name) { memcpy(name, buf + 8, 15); name[15] = 0; }
    }
    if (store_read(k, SONG_BODY, &crc, 4)) return 0;
    return ~c == crc;
}
static void slots_scan(void)
{
    uint32_t k;
    for (k = 0; k < NSTORE; k++) {
        slot_ok[k] = (uint8_t)slot_valid(k, slot_name[k]);
        if (!slot_ok[k]) slot_name[k][0] = 0;
    }
}
static int song_save(uint32_t k)
{
    song.magic = SONG_MAGIC;
    song.version = SONG_VERSION;
    song.crc = ~crc_step(0xFFFFFFFFu, (const uint8_t *)&song, SONG_BODY);
    if (store_write(k, &song, sizeof song)) return -1;
    slot_ok[k] = 1;
    memcpy(slot_name[k], song.name, 16);
    slot_name[k][15] = 0;
    return 0;
}
static int slot_copy(uint32_t d, uint32_t s)    /* (XENOISE) a slot's song to another, a piece at a time (no song in RAM) */
{
    if (d == s || (!slot_ok[s] && !slot_ok[d])) return 0;
    if (!slot_ok[s]) { slot_ok[d] = 0; slot_name[d][0] = 0; return store_erase(d); }
#if defined(DINGHY_HOST) || defined(KAWA_WASM) || defined(XN_WB_RAMSTORE)
    memcpy(host_flash[d], host_flash[s], SLOT_SIZE);
    store_gen++;
#else
    {
        uint8_t buf[256];
        uint32_t off, n;
        int rc;
        if ((rc = store_erase(d)) != 0) return rc;
        for (off = 8; off < sizeof(song_t); off += n) {
            n = sizeof(song_t) - off < sizeof buf ? sizeof(song_t) - off : sizeof buf;
            if ((rc = store_read(s, off, buf, n)) != 0 || (rc = st_prog(slot_at(d) + off, buf, n)) != 0) return rc;
        }
        if ((rc = store_read(s, 0, buf, 8)) != 0 || (rc = st_prog(slot_at(d), buf, 8)) != 0) return rc;   /* (the magic last) */
    }
#endif
    slot_ok[d] = 1;
    memcpy(slot_name[d], slot_name[s], 16);
    return 0;
}
static int song_load(uint32_t k)
{
    if (!slot_valid(k, 0)) return -1;
    sq_req = 2;
    if (store_read(k, 0, &song, sizeof song)) return -1;
    if (song.x.magic != SONG_EXT_MAGIC) { memset(&song.x, 0, sizeof song.x); song.x.magic = SONG_EXT_MAGIC; }   /* (XENOISE: saved before it) */
    seed_of = 0xFF;
    undo_ok = 0;
    sit_mute = 0;
    return 0;
}

/* (XENOISE) the CUSTOM theme (xn_themes.h): the Dock's THEME LAB sends it, the sector after the store keeps it
 * (0xD7000: free in JIANT's map, storage.c). Written stopped only (an erase stalls the sound): one that comes while
 * the sequence plays shows at once and is saved at the next stop.
 * On the wire (editor 99 XN_THEME_PUT, 100 XN_THEME_GET; the emulator's web_xn_theme_*): the name in 11 bytes
 * (ASCII, 0-padded), then the 34 colours (RGB565, xn_themes.h's order) as 3 bytes each, 7 + 7 + 2 bits */
#define THEME_MAGIC 0x4D454854u                 /* "THEM" */
#define THEME_WIRE (11u + XN_TH_NCOL * 3u)
typedef struct { uint32_t magic; char name[12]; uint16_t c[XN_TH_NCOL]; uint32_t crc; } xn_theme_rec_t;
static uint8_t theme_dirty;
#if defined(DINGHY_HOST) || defined(KAWA_WASM) || defined(XN_WB_RAMSTORE)
static xn_theme_rec_t host_theme;
static int theme_rec_read(xn_theme_rec_t *r) { *r = host_theme; return 0; }
static int theme_rec_write(const xn_theme_rec_t *r) { host_theme = *r; return 0; }
#else
#define THEME_AT (STORE_LO + NSTORE * SLOT_SIZE)
typedef char theme_fits[THEME_AT + 0x1000u <= STORE_HI ? 1 : -1];
static int theme_rec_read(xn_theme_rec_t *r) { return st_read(THEME_AT, r, sizeof *r); }
static int theme_rec_write(const xn_theme_rec_t *r)
{
    int rc;
    if ((rc = st_erase(THEME_AT)) != 0) return rc;
    if ((rc = st_prog(THEME_AT + 4u, (const uint8_t *)r + 4u, sizeof *r - 4u)) != 0) return rc;
    return st_prog(THEME_AT, r, 4u);            /* the magic last: a cut write stays invalid */
}
#endif
static uint32_t theme_rec_crc(const xn_theme_rec_t *r)
{
    return ~crc_step(0xFFFFFFFFu, (const uint8_t *)r->name, (uint32_t)(sizeof r->name + sizeof r->c));
}
static void theme_take(const char *name, const uint16_t *c)     /* into CUSTOM */
{
    uint32_t i;
    xn_theme_ptr(WB_NTHEME);
    memcpy(xn_theme_cols(&xn_custom), c, XN_TH_NCOL * 2u);
    for (i = 0; i < 11u && name[i]; i++)
        xn_custom_name[i] = name[i] >= 'a' && name[i] <= 'z' ? (char)(name[i] - 32) : name[i] >= ' ' && name[i] < 127 ? name[i] : ' ';
    xn_custom_name[i] = 0;
    if (!i) str_cat((xn_custom_name[0] = 0, xn_custom_name), "CUSTOM");
}
static void theme_load(void)                    /* at start: CUSTOM as it was saved */
{
    xn_theme_rec_t r;
    if (!theme_rec_read(&r) && r.magic == THEME_MAGIC && theme_rec_crc(&r) == r.crc) {
        r.name[11] = 0;
        theme_take(r.name, r.c);
    }
}
static int theme_save(void)
{
    xn_theme_rec_t r;
    memset(&r, 0, sizeof r);
    r.magic = THEME_MAGIC;
    memcpy(r.name, xn_custom_name, 12);
    memcpy(r.c, xn_theme_cols(&xn_custom), sizeof r.c);
    r.crc = theme_rec_crc(&r);
    return theme_rec_write(&r);
}
static void theme_poll(void) { if (theme_dirty && !sq_play) { theme_dirty = 0; theme_save(); } }
static uint32_t theme_wire_put(const uint8_t *a)   /* a theme from the wire: CUSTOM, picked; 0 saved, 1 saved at the stop */
{
    uint16_t c[XN_TH_NCOL];
    char name[12];
    uint32_t i;
    for (i = 0; i < 11u; i++) name[i] = (char)(a[i] & 127u);
    name[11] = 0;
    for (i = 0; i < XN_TH_NCOL; i++)
        c[i] = (uint16_t)(a[11u + i * 3u] | (uint32_t)a[12u + i * 3u] << 7 | (uint32_t)(a[13u + i * 3u] & 3u) << 14);
    theme_take(name, c);
    xj_pref_set((xj_pref() & ~15u) | WB_NTHEME);
    theme_apply();
    theme_dirty = 1;
    theme_poll();
    return theme_dirty;
}
static uint32_t theme_wire_get(uint32_t k, uint8_t *o)   /* theme k (127: the one shown) to the wire; -> its index */
{
    const wb_theme_t *t;
    const uint16_t *c;
    uint32_t i;
    if (k >= XN_NTHEME) k = xn_theme_index();
    t = xn_theme_ptr(k);
    c = &t->bg;
    for (i = 0; i < 11u; i++) o[i] = 0;
    for (i = 0; i < 11u && t->tname[i]; i++) o[i] = (uint8_t)(t->tname[i] & 127);
    for (i = 0; i < XN_TH_NCOL; i++) {
        o[11u + i * 3u] = (uint8_t)(c[i] & 127u);
        o[12u + i * 3u] = (uint8_t)((c[i] >> 7) & 127u);
        o[13u + i * 3u] = (uint8_t)(c[i] >> 14);
    }
    return k;
}

/* (XENOISE) the Dock's EXPORT (editor SysEx, editor.c): the songs leave over USB-MIDI to be rendered, stemmed or
 * turned into MIDI in the browser.
 *   96 XN_LIST                -> the slots: their count, then each: held (0/1), its name (0-terminated)
 *   97 XN_GET slot chunk(14b) -> slot, chunk, the song's size (21 bits), the bytes here (14 bits), each byte as two
 *                                nibbles; slot 127: the song in memory now. 256 bytes a chunk */
#ifdef ED_HDR0                                  /* (editor.c in this build: the FM-1's) */
static int xn_ed_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t k, i;
    if (cmd == 96u) {
        slots_scan();
        ed_b(NSLOT);
        for (k = 0; k < NSLOT; k++) { ed_b(slot_ok[k] ? 1u : 0u); ed_str(slot_name[k], 15); }
        return 1;
    }
    if (cmd == 97u && na == 3u) {
        static uint8_t buf[256];
        uint32_t c = a[1] | (uint32_t)a[2] << 7, off = c * 256u, size = sizeof(song_t), n = 0;
        k = a[0];
        ed_b(k); ed_b(a[1]); ed_b(a[2]);
        ed_b(size); ed_b(size >> 7); ed_b(size >> 14);
        if (off < size && (k == 127u || (k < NSLOT && slot_ok[k]))) {
            n = size - off < 256u ? size - off : 256u;
            if (k == 127u) memcpy(buf, (const uint8_t *)&song + off, n);
            else if (store_read(k, off, buf, n)) n = 0;
        }
        ed_b(n); ed_b(n >> 7);
        for (i = 0; i < n; i++) { ed_b(buf[i] >> 4); ed_b(buf[i] & 15u); }
        return 1;
    }
    if (cmd == 99u && na == THEME_WIRE) {       /* 99 XN_THEME_PUT: CUSTOM, picked -> 0 saved, 1 at the next stop */
        ed_b(theme_wire_put(a));
        return 1;
    }
    if (cmd == 100u && na == 1u) {              /* 100 XN_THEME_GET k (127: the one shown) -> k, the count, the theme */
        uint8_t o[THEME_WIRE];
        k = theme_wire_get(a[0], o);
        ed_b(k); ed_b(XN_NTHEME);
        for (i = 0; i < THEME_WIRE; i++) ed_b(o[i]);
        return 1;
    }
    return 0;
}
#endif
/* (XENOISE) UNDO: one change back. What the change took goes to UNDO's slot (a song, or a slot's song) or, for the
 * sequence, a copy here; SEQ held + OCT- (anywhere), or UNDO's F button, puts it back. The store is written stopped only
 * (an erase stalls the sound): a song replaced while it plays has no undo */
enum { U_NONE, U_SONG, U_SLOT, U_INSERT, U_OUTSERT, U_CHAIN };
static uint8_t xu_kind, xu_at, xu_chain[16], xu_nchain, xu_smute[16];
static void xu_slot(uint32_t k)               /* slot k as it is, before a change to it */
{
    if (slot_ok[k]) slot_copy(UNDO_SLOT, k);
    else if (slot_ok[UNDO_SLOT]) { store_erase(UNDO_SLOT); slot_ok[UNDO_SLOT] = 0; }
    xu_kind = U_SLOT;
    xu_at = (uint8_t)k;
}
static int xu_song(void)                      /* the song in memory, before it is replaced */
{
    uint8_t keep = song.loop;
    if (sq_play) { xu_kind = U_NONE; return 0; }
    if (song_save(UNDO_SLOT)) { xu_kind = U_NONE; return 0; }
    song.loop = keep;
    xu_kind = U_SONG;
    return 1;
}
static void xu_chain_take(void)
{
    memcpy(xu_chain, song.chain, 16);
    memcpy(xu_smute, song.x.smute, 16);
    xu_nchain = song.nchain;
    xu_kind = U_CHAIN;
}
/* ================================================================ the knobs
 * a sound parameter: its place in p[], its range and how it reads */
enum { F_INT, F_ST, F_MS, F_PCT, F_X10, F_X100, F_CHORD, F_RATIO, F_WAVE, F_UNI, F_CT, F_VOWEL, F_CTR, F_SYN, F_BRK, F_HZ,
       F_SOUND, F_OCT, F_SOUND2 };
typedef struct {
    const char *lab;
    int8_t idx;                                 /* -1: the factory sound */
    int16_t lo, hi;
    uint8_t fmt, rnd;                           /* rnd: RAND may change it */
} pd_t;
#define PD_END {0, 0, 0, 0, 0, 0}
static const pd_t SP_KICK[2][4] = {{{"SOUND", -1, 0, WB_NKICK - 1, F_SOUND, 0}, {"TUNE", PK_TUNE, -12, 12, F_ST, 1}, {"DECAY", PK_DECAY, 100, 1500, F_MS, 1}, {"PUNCH", PK_PUNCH, 0, 100, F_PCT, 1}},
                                   {{"DRIVE", PK_DRIVE, 0, 100, F_PCT, 1}, PD_END, PD_END, PD_END}};
static const pd_t SP_DRM[4][4] = {          /* the snare, then the hats (idx -2: the hats' factory sound) */
    {{"SNARE", -1, 0, WB_NSNARE - 1, F_SOUND, 0}, {"TUNE", PS_TUNE, -12, 12, F_ST, 1}, {"DECAY", PS_DECAY, 50, 600, F_MS, 1}, {"TONE", PS_TONE, 0, 100, F_PCT, 1}},
    {{"SNAP", PS_SNAP, 0, 100, F_PCT, 1}, {"CLAP", PS_CLAP, 0, 100, F_PCT, 1}, PD_END, PD_END},
    {{"HATS", -2, 0, WB_NHAT - 1, F_SOUND2, 0}, {"TUNE", PH_TUNE, -12, 12, F_ST, 1}, {"DECAY", PH_DECAY, 20, 400, F_MS, 1}, {"TONE", PH_TONE, 0, 100, F_PCT, 1}},
    {{"METAL", PH_METAL, 0, 100, F_PCT, 1}, PD_END, PD_END, PD_END}};
static const pd_t SP_BRK[1][4] = {{{"BREAK", PB_SRC, 0, BRK_N - 1, F_BRK, 0}, {"PITCH", PB_PITCH, -12, 12, F_ST, 1}, {"GATE", PB_GATE, 10, 200, F_PCT, 1}, {"TONE", PB_TONE, 0, 100, F_HZ, 1}}};
static const pd_t SP_FM[3][4] = {
    {{"SOUND", -1, 0, WB_NSYN - 1, F_SOUND, 0}, {"BRIGHT", PY_INDEX, 0, 80, F_X10, 1}, {"DECAY", PY_DEC, 50, 4000, F_MS, 1}, {"CHORD", PY_CHORD, 0, WB_NCHORD - 1, F_CHORD, 0}},
    {{"RATIO", PY_RATIO, 0, WB_NRATIO - 1, F_RATIO, 1}, {"ATTACK", PY_ATT, 2, 3000, F_MS, 1}, {"REL", PY_REL, 20, 4000, F_MS, 1}, {"DETUNE", PY_DET, 0, 30, F_CT, 1}},
    {{"TINE", PY_TINE, 0, 100, F_PCT, 1}, {"SUS", PY_SUS, 0, 100, F_PCT, 1}, {"OCTAVE", PY_OCT, -2, 2, F_OCT, 0}, PD_END}};
static const pd_t SP_VA[4][4] = {
    {{"SOUND", -1, 0, WB_NSYN - 1, F_SOUND, 0}, {"BRIGHT", PY_INDEX, 0, 80, F_X10, 1}, {"DECAY", PY_DEC, 50, 4000, F_MS, 1}, {"CHORD", PY_CHORD, 0, WB_NCHORD - 1, F_CHORD, 0}},
    {{"ATTACK", PY_ATT, 2, 3000, F_MS, 1}, {"REL", PY_REL, 20, 4000, F_MS, 1}, {"DETUNE", PY_DET, 0, 40, F_CT, 1}, {"RESO", PY_RESO, 5, 180, F_X10, 1}},
    {{"F.ENV", PY_FENV, 0, 400, F_X100, 1}, {"UNISON", PY_UNI, 1, 3, F_UNI, 1}, {"WAVE", PY_WAVE, 0, 3, F_WAVE, 1}, PD_END},
    {{"SWOOP", PY_GLIDE, -12, 12, F_ST, 1}, {"SUS", PY_SUS, 0, 100, F_PCT, 1}, {"OCTAVE", PY_OCT, -2, 2, F_OCT, 0}, {"VIB", PY_VIB, 0, 60, F_CT, 1}}};
static const pd_t SP_VOX[3][4] = {
    {{"SOUND", -1, 0, WB_NSYN - 1, F_SOUND, 0}, {"LEVEL", PY_INDEX, 0, 80, F_X10, 1}, {"DECAY", PY_DEC, 50, 4000, F_MS, 1}, {"CHORD", PY_CHORD, 0, WB_NCHORD - 1, F_CHORD, 0}},
    {{"ATTACK", PY_ATT, 2, 3000, F_MS, 1}, {"REL", PY_REL, 20, 4000, F_MS, 1}, {"DETUNE", PY_DET, 0, 40, F_CT, 1}, {"VOWEL", PY_VOWEL, 0, 4, F_VOWEL, 1}},
    {{"SWOOP", PY_GLIDE, -12, 12, F_ST, 1}, {"SUS", PY_SUS, 0, 100, F_PCT, 1}, {"OCTAVE", PY_OCT, -2, 2, F_OCT, 0}, {"VIB", PY_VIB, 0, 60, F_CT, 1}}};
static const pd_t SP_CTR[4] = {{"CTR", PY_CTR, 0, WB_NCTR - 1, F_CTR, 0}, {"C.SND", PY_CTRP, 0, WB_NSYN - 1, F_SYN, 0}, {"C.LVL", PY_CTRL, 0, 100, F_PCT, 0}, PD_END};

static uint32_t snd_pages(uint32_t t, const pd_t (**pg)[4])   /* the track's knob pages (and the counter voice's on KEY) */
{
    switch (TRK_TYPE[t]) {
    case TY_BREAK: *pg = SP_BRK; return 1;
    case TY_KICK: *pg = SP_KICK; return 2;
    case TY_DRUMS: *pg = SP_DRM; return 4;
    default:
        switch (song.trk[t].p[PY_ENG]) {
        case 1: *pg = SP_VA; return 4;
        case 2: *pg = SP_VOX; return 3;
        default: *pg = SP_FM; return 3;
        }
    }
}
static uint32_t snd_npages(uint32_t t) { const pd_t (*pg)[4]; return snd_pages(t, &pg) + (t == T_KEY ? 1u : 0u); }
static const pd_t *snd_pd(uint32_t t, uint32_t s, uint32_t i)
{
    const pd_t (*pg)[4];
    uint32_t n = snd_pages(t, &pg);
    s %= snd_npages(t);
    if (s >= n) return SP_CTR[i].lab ? &SP_CTR[i] : 0;
    return pg[s][i].lab ? &pg[s][i] : 0;
}

static void pd_fmt(char *o, const pd_t *d, int32_t v, uint32_t t)
{
    char *e;
    o[0] = 0;
    switch (d->fmt) {
    case F_ST: e = fmt_s(o, v); str_cat(e, " ST"); break;
    case F_MS:
        if (v < 1000) { e = fmt_u(o, (uint32_t)v, 0); str_cat(e, "MS"); }
        else { e = fmt_u(o, (uint32_t)v / 1000u, 0); *e++ = '.'; e = fmt_u(e, (uint32_t)v % 1000u / 10u, 2); str_cat(e, "S"); }
        break;
    case F_PCT: e = fmt_u(o, (uint32_t)v, 0); str_cat(e, "%"); break;
    case F_X10: e = fmt_u(o, (uint32_t)v / 10u, 0); *e++ = '.'; fmt_u(e, (uint32_t)v % 10u, 1); break;
    case F_X100: e = fmt_u(o, (uint32_t)v / 100u, 0); *e++ = '.'; fmt_u(e, (uint32_t)v % 100u, 2); break;
    case F_CHORD: str_cat(o, WB_CHORD_NAME[clampi(v, 0, WB_NCHORD - 1)]); break;
    case F_RATIO: {
        uint32_t r = (uint32_t)v;
        e = fmt_u(o, r / 100u, 0);
        if (r % 100u) { *e++ = '.'; e = fmt_u(e, r % 100u, r % 10u ? 2u : 0u); if (!(r % 10u)) { e[-1] = 0; e = fmt_u(e - 1, r % 100u / 10u, 0); } }
        str_cat(e, ":1");
        break;
    }
    case F_WAVE: str_cat(o, (const char *const[]){"SAW", "SQUARE", "TRI", "SINE"}[clampi(v, 0, 3)]); break;
    case F_UNI: e = fmt_u(o, (uint32_t)v, 0); str_cat(e, "X"); break;
    case F_CT: e = fmt_u(o, (uint32_t)v, 0); str_cat(e, " CT"); break;
    case F_VOWEL: str_cat(o, (const char *const[]){"A", "E", "I", "O", "U"}[clampi(v, 0, 4)]); break;
    case F_CTR: str_cat(o, WB_CTR_NAME[clampi(v, 0, WB_NCTR - 1)]); break;
    case F_SYN: str_cat(o, WB_SYN_NAME[clampi(v, 0, WB_NSYN - 1)]); break;
    case F_BRK: str_cat(o, BRK_NAME[clampi(v, 0, BRK_N - 1)]); break;
    case F_HZ: { uint32_t f = 180u + (uint32_t)(v * v) * 2u; if (v >= 98) str_cat(o, "OPEN"); else { e = fmt_u(o, f / 100u * 100u, 0); str_cat(e, "HZ"); } break; }
    case F_SOUND: str_cat(o, patch_name(t, (uint32_t)v)); break;
    case F_SOUND2: str_cat(o, WB_HAT_NAME[(uint32_t)v % WB_NHAT]); break;
    case F_OCT: fmt_s(o, v); break;
    default: fmt_s(o, v); break;
    }
}
static int32_t pd_get(const pd_t *d, uint32_t t)
{
    const int16_t *p = song.trk[t].p;
    if (d->idx == -2) return song.trk[t].patch2;
    if (d->idx < 0) return song.trk[t].patch;
    if (d->fmt == F_RATIO) {                    /* the nearest ratio of the list */
        uint32_t i, b = 0;
        for (i = 0; i < WB_NRATIO; i++)
            if (clampi(p[PY_RATIO] - (int32_t)WB_RATIO[i], -9999, 9999) * clampi(p[PY_RATIO] - (int32_t)WB_RATIO[i], -9999, 9999) <
                clampi(p[PY_RATIO] - (int32_t)WB_RATIO[b], -9999, 9999) * clampi(p[PY_RATIO] - (int32_t)WB_RATIO[b], -9999, 9999)) b = i;
        return (int32_t)b;
    }
    return p[d->idx];
}
static void pd_set(const pd_t *d, uint32_t t, int32_t v)
{
    v = clampi(v, d->lo, d->hi);
    if (d->idx == -2) { hat_patch(t, (uint32_t)v); return; }
    if (d->idx < 0) { sound_patch(t, (uint32_t)v); return; }
    song.trk[t].p[d->idx] = (int16_t)(d->fmt == F_RATIO ? (int32_t)WB_RATIO[v] : v);
}
static int32_t step_of(const pd_t *d, int32_t v, int32_t dd)   /* times move faster when long */
{
    if (d->fmt == F_MS) { int32_t s = v / 16; return v + dd * (s < 1 ? 1 : s); }
    if (d->hi - d->lo >= 100) return v + dd * (dd > 2 || dd < -2 ? 2 : 1);
    return v + dd;
}

/* ---- the page's knob i: turned by dd (0: only its label and value) */
static const uint8_t RATES[4] = {4, 8, 16, 32};
static uint32_t chord_trk(void) { return genre()->chords ? T_KEY : T_SYN; }
static void cell_def(cell_t *c, uint32_t t)
{
    if (!c->n) { c->n = (uint8_t)(TRK_TYPE[t] == TY_BREAK ? last_note[t] + 1u : last_note[t]); c->v = 12; c->fx = 0; c->fv = 0; }
}
static void sng_ask(uint32_t a)                 /* (XENOISE) SAV: the question (OCT+ yes, OCT- no) */
{
    static const char *const Q[8] = {"SAVE TO ", "NEW SONG", "LOAD ", "DELETE ", "COPY ", "PASTE TO ", "INSERT AT ", "OUTSERT "};
    char buf[32], *e;
    int write = a != SA_LOAD && a != SA_COPY && a != SA_NEW;
    if (write && sq_play) { toast("STOP FIRST (THE STORE)"); return; }
    if ((a == SA_LOAD || a == SA_COPY || a == SA_DELETE) && !slot_ok[slot_sel]) { toast("NOTHING IN THAT SLOT"); return; }
    if ((a == SA_PASTE || a == SA_INSERT) && (sng_clip == 0xFF || !slot_ok[sng_clip])) { toast("COPY A SONG FIRST (K3)"); return; }
    if (a == SA_INSERT && slot_ok[NSLOT - 1u]) { toast("THE LAST SLOT IS FULL"); return; }
    arm_f = (uint8_t)a;
    arm_ms = fm1_ms;
    arm_oct = 0;
    buf[0] = 0;
    e = str_cat(buf, Q[a % 8u]);
    if (a != SA_NEW) e = fmt_u(e, slot_sel, 2);
    str_cat(e, "? OCT+");
    toast(buf);
}
static int ngen_knob(uint32_t i, int32_t dd, char *lab, char *val)   /* (XENOISE) GEN: the next song */
{
    static const char *const L[2][4] = {{"GENRE", "PROG", "CHORDS", "MELODY"}, {"BPM", "KEY", "FORM", "BASS"}};
    static const char *const CS[4] = {"AUTO", "1/2 BAR", "1 BAR", "2 BARS"}, *const MS[3] = {"NORMAL", "AIRY", "SPARSE"};
    static const char *const BS[4] = {"AUTO", "WARM", "SINE", "GENRE"};
    str_cat(lab, L[sub & 1u][i]);
    switch ((sub & 1u) * 4u + i) {
    case 0:
        if (dd) genre_pick = (uint8_t)(((int32_t)genre_pick + dd % (int32_t)(WB_NGENRE + 1u) + (int32_t)WB_NGENRE + 1) % (int32_t)(WB_NGENRE + 1u));
        if (genre_pick >= WB_NGENRE) str_cat(val, "EMPTY"); else fmt_u(val, genre_pick + 1u, 0);
        break;
    case 1:
        if (dd) ng_prog = (uint8_t)clampi(ng_prog + dd, 0, WB_NPROG - 1);
        str_cat(val, ng_prog ? WB_PROGS[ng_prog - 1u].name : "AUTO");
        break;
    case 2: if (dd) ng_cspace = (uint8_t)clampi(ng_cspace + dd, 0, 3); str_cat(val, CS[ng_cspace & 3u]); break;
    case 3: if (dd) ng_mspace = (uint8_t)clampi(ng_mspace + dd, 0, 2); str_cat(val, MS[ng_mspace % 3u]); break;
    case 4:
        if (dd) { int32_t v = (ng_bpm ? ng_bpm : 59) + dd; ng_bpm = (uint8_t)(v < 60 ? 0 : v > 180 ? 180 : v); }
        if (ng_bpm) fmt_u(val, ng_bpm, 0); else str_cat(val, "AUTO");
        break;
    case 5: if (dd) ng_key = (uint8_t)clampi(ng_key + dd, 0, 12); str_cat(val, ng_key ? NOTE_NM[(ng_key - 1u) % 12u] : "AUTO"); break;
    case 6: if (dd) ng_form = (uint8_t)clampi(ng_form + dd, 0, WB_NFORM - 1); str_cat(val, FORM_NAME[ng_form % WB_NFORM]); break;
    default: if (dd) ng_bass = (uint8_t)clampi(ng_bass + dd, 0, 3); str_cat(val, BS[ng_bass & 3u]); break;
    }
    return 1;
}
static void ngen_ask(void)
{
    char buf[32];
    arm_f = SA_NEW;
    arm_ms = fm1_ms;
    buf[0] = 0;
    str_cat(str_cat(buf, genre_pick >= WB_NGENRE ? "EMPTY SONG" : WB_GENRES[genre_pick].name), "? OCT+");
    buf[31] = 0;
    toast(buf);
}
static int knob(uint32_t i, int32_t dd, char *lab, char *val)
{
    uint32_t t = trk();
    track_t *tr = &song.trk[t];
    char *e;
    lab[0] = val[0] = 0;
    if (fx_on()) return fx_knob(i, dd, lab, val);   /* (XENOISE: PERFORM's own knobs) */
    if (page == PG_TRK && (vis_on || !hom_edit)) return 0;   /* (XENOISE: the visualizer, or HOM plain: the knobs rest) */
    switch (page) {
    case PG_ENV: return env_knob(t, i, dd, lab, val);
    case PG_LFO: return lfo_knob(t, i, dd, lab, val);
    case PG_ARP: return arp_knob(t, i, dd, lab, val);
    case PG_TRK: {
        cell_t *c = &P()->c[t][cur_row % plen()];
        int brk = TRK_TYPE[t] == TY_BREAK, drm = TRK_TYPE[t] == TY_DRUMS;
        if (i == 0u) {
            int32_t cur = c->n ? (c->n == NOTE_OFF ? -1 : (int32_t)c->n - (brk ? 1 : 0)) : (int32_t)last_note[t];
            str_cat(lab, drm ? "DRUM" : "NOTE");
            if (dd) {
                cur = clampi(cur + dd, -1, brk ? 15 : drm ? 127 : 108);
                if (drm && cur >= 0 && cur < 48) cur = dd < 0 ? -1 : 48;
                cell_def(c, t);
                if (cur < 0) c->n = NOTE_OFF;
                else { c->n = (uint8_t)(brk ? cur + 1 : cur); last_note[t] = (uint8_t)cur; if (!sq_play) audition(t, (uint32_t)cur); }
                touch();
            }
            if (cur < 0) str_cat(val, "OFF");
            else if (brk) { e = str_cat(val, "SLC "); fmt_u(e, (uint32_t)cur + 1u, 2); }
            else if (drm) drm_name((uint32_t)cur, val);
            else note_name((uint32_t)cur, val);
        } else if (i == 1u) {
            str_cat(lab, "VEL");
            if (dd) { cell_def(c, t); c->v = (uint8_t)clampi((int32_t)c->v + dd, 0, 15); touch(); }
            fmt_u(val, c->n ? c->v : 12u, 0);
        } else if (i == 2u) {
            str_cat(lab, "FX");
            if (dd) {
                cell_def(c, t);
                c->fx = (uint8_t)clampi((int32_t)c->fx + dd, 0, FX_N - 1);
                if (c->fx == FX_R && c->fv < 2u) c->fv = 2;
                if (c->fx == FX_G && !c->fv) c->fv = 7;
                if (c->fx == FX_P && !c->fv) c->fv = 8;
                touch();
            }
            str_cat(val, FX_NAME[c->n ? c->fx % FX_N : 0]);
        } else {
            uint32_t fv = c->n ? c->fv : 0u, fx = c->n ? c->fx : 0u;
            str_cat(lab, "AMT");
            if (dd) { cell_def(c, t); c->fv = (uint8_t)clampi((int32_t)c->fv + dd, 0, 15); fv = c->fv; fx = c->fx; touch(); }
            if (fx == FX_R) { e = str_cat(val, "X"); fmt_u(e, fv, 0); }
            else if (fx == FX_G) { e = fmt_u(val, (fv + 1u) * 100u / 16u, 0); str_cat(e, "%"); }
            else if (fx == FX_D) { e = fmt_u(val, fv, 0); str_cat(e, "/16"); }
            else if (fx == FX_P) { e = fmt_s(val, (int32_t)fv - 8); str_cat(e, " ST"); }
            else fmt_u(val, fv, 0);
        }
        return 1;
    }
    case PG_SND: {
        const pd_t *d = snd_pd(t, sub, i);
        int32_t v;
        if (!d) return 0;
        str_cat(lab, d->lab);
        v = pd_get(d, t);
        if (dd) {
            v = d->fmt == F_SOUND || d->fmt == F_RATIO || d->fmt == F_BRK || d->fmt == F_SYN || d->fmt == F_CHORD || d->fmt == F_CTR ? v + dd : step_of(d, v, dd);
            pd_set(d, t, v);
            v = pd_get(d, t);
            touch();
            if (d->fmt == F_SOUND && !sq_play) audition(t, TRK_TYPE[t] == TY_DRUMS ? DN_SNR : last_note[t]);
            if (d->fmt == F_SOUND2 && !sq_play) audition(t, DN_HAT);
        }
        pd_fmt(val, d, d->fmt == F_RATIO ? (int32_t)WB_RATIO[clampi(v, 0, WB_NRATIO - 1)] : v, t);
        return 1;
    }
    case PG_SNG: {                              /* (XENOISE) K1 SAVE K2 LOAD K3 COPY K4 PASTE; SAVE again: INSERT OUTSERT NEW DELETE */
        static const char *const L[2][4] = {{"SAVE", "LOAD", "COPY", "PASTE"}, {"INSERT", "OUTSRT", "NEW", "DELETE"}};
        static const uint8_t A[2][4] = {{SA_SAVE, SA_LOAD, SA_COPY, SA_PASTE}, {SA_INSERT, SA_OUTSERT, SA_NEW, SA_DELETE}};
        uint32_t a = A[sub & 1u][i];
        str_cat(lab, L[sub & 1u][i]);
        if (a == SA_NEW) str_cat(val, "EMPTY");
        else if (a == SA_PASTE) { if (sng_clip == 0xFF) str_cat(val, "-"); else { e = fmt_u(val, sng_clip, 2); e = str_cat(e, ">"); fmt_u(e, slot_sel, 2); } }
        else { e = str_cat(val, a == SA_SAVE ? "TO " : a == SA_LOAD ? "FROM " : a == SA_INSERT ? "AT " : ""); fmt_u(e, slot_sel, 2); }
        if (dd) sng_ask(a);
        return 1;
    }
    case PG_MST + 100u: {                       /* (XENOISE) MST's SYSTEM page (below) */
        uint32_t v = xj_pref();
        if (i == 0u) {
            str_cat(lab, "THEME");
            if (dd) { v = (v & ~15u) | (uint32_t)(((int32_t)(v & 15u) % (int32_t)XN_NTHEME + dd % (int32_t)XN_NTHEME + (int32_t)XN_NTHEME) % (int32_t)XN_NTHEME); xj_pref_set(v); theme_apply(); }   /* (round and round) */
            str_cat(val, TH->tname);
        } else if (i == 1u) {
            str_cat(lab, "BLACK");
            if (dd) { v = dd > 0 ? v | 16u : v & ~16u; xj_pref_set(v); }
            str_cat(val, jam_nav() ? "TRACKS" : "PLAY");
        } else if (i == 2u) {                   /* (XENOISE) the MIDI clock: INT, OUT, IN */
            uint32_t c = (v >> 5) & 3u;
            str_cat(lab, "CLOCK");
            if (dd) { c = (uint32_t)clampi((int32_t)c + (dd > 0 ? 1 : -1), 0, 2); xj_pref_set((v & ~0x60u) | c << 5); }
            str_cat(val, (const char *const[]){"INT", "OUT", "IN", "INT"}[c]);
        } else return 0;
        return 1;
    }
    case PG_CHD:
        if (i == 0u) {
            str_cat(lab, "PROG");
            if (dd) { prog_sel = (uint8_t)clampi((int32_t)prog_sel + dd, 0, WB_NPROG - 1); prog_due = 1; prog_ms = fm1_ms; }
            str_cat(val, WB_PROGS[(prog_due ? prog_sel : song.prog) % WB_NPROG].name);
        } else if (i == 1u) {
            uint32_t k = 0;
            str_cat(lab, "LENGTH");
            while (k < 3u && RATES[k] < song.rows) k++;
            if (dd) {
                k = (uint32_t)clampi((int32_t)k + (dd > 0 ? 1 : -1), 0, 3);
                if (RATES[k] != song.rows) { prog_set(song.deg, song.ndeg, RATES[k], song.scale, song.prog); touch(); }
            }
            e = fmt_u(val, song.rows, 0);
            str_cat(e, " ROWS");
        } else if (i == 2u) {
            int16_t *ch = &song.trk[chord_trk()].p[PY_CHORD];
            str_cat(lab, "VOICE");
            if (dd) { *ch = (int16_t)clampi(*ch + dd, 0, WB_NCHORD - 1); touch(); }
            str_cat(val, WB_CHORD_NAME[clampi(*ch, 0, WB_NCHORD - 1)]);
        } else {
            str_cat(lab, "KEY");
            if (dd) { transpose_to((uint32_t)((song.key + 12 + (dd > 0 ? 1 : -1)) % 12)); touch(); }
            str_cat(val, NOTE_NM[song.key % 12u]);
        }
        return 1;
    case PG_ARR: {                              /* (XENOISE) the slot picked (PRESETS): K1 copy, K2 paste, K3 insert, K4 outsert;
                                                 * a turn asks, OCT+ does it, OCT- does not (the PILOT's way, as SAV) */
        static const char *const L[4] = {"COPY", "PASTE", "INSERT", "OUTSRT"};
        uint32_t n = song.nchain, at = arr_pos < n ? arr_pos : n ? n - 1u : 0u;
        char *q;
        if (arr_live) {                         /* LIVE: K1 queues a pattern (as the white keys), K2 the loop */
            if (i == 0u) {
                uint32_t cur = sq_play && sq_cue < NPAT ? sq_cue : song.cur % NPAT;
                str_cat(lab, "QUEUE");
                if (dd) live_queue((uint32_t)clampi((int32_t)cur + (dd > 0 ? 1 : -1), 0, (int32_t)live_n() - 1));
                q = str_cat(val, "P");
                q[0] = (char)('1' + (sq_play && sq_cue < NPAT ? sq_cue : song.cur % NPAT));
                q[1] = 0;
                return 1;
            }
            if (i == 1u) {
                str_cat(lab, "LOOP");
                if (dd) { song.loop = dd > 0; touch(); }
                str_cat(val, song.loop ? "PATTERN" : "SEQ");
                return 1;
            }
            return 0;
        }
        str_cat(lab, L[i]);
        if (i == 0u) { q = str_cat(val, "P"); q[0] = (char)('1' + song.chain[at] % NPAT); q[1] = 0; }
        else if (arr_clip == 0xFF) str_cat(val, "-");
        else { q = str_cat(val, "P"); q[0] = (char)('1' + arr_clip % NPAT); q[1] = 0; }
        if (i == 3u) { val[0] = 0; q = fmt_u(str_cat(val, "SLOT "), at + 1u, 0); (void)q; }
        if (!dd) return 1;
        if (i != 0u && arr_clip == 0xFF && i != 3u) { toast("COPY A SLOT FIRST (K1)"); return 1; }
        if (i == 2u && n >= 16u) { toast("16 SLOTS AT MOST"); return 1; }
        if (i == 3u && n <= 1u) { toast("ONE SLOT LEFT"); return 1; }
        if (arm_f != ARR_ASK + i) {
            char buf[32];
            arm_f = (uint8_t)(ARR_ASK + i);
            arm_ms = fm1_ms;
            buf[0] = 0;
            q = fmt_u(str_cat(str_cat(buf, (const char *const[]){"COPY ", "PASTE TO ", "INSERT AT ", "OUTSERT "}[i]), "SLOT "), at + 1u, 0);
            str_cat(q, "? OCT+");
            toast(buf);
        }
        return 1;
    }
    case PG_ARR + 100u:                         /* (WND-1's ARR knobs, kept for the PRESETS knob: the slot's pattern) */
        if (i == 0u) {
            str_cat(lab, "PART");
            if (dd) arr_pos = (uint8_t)clampi((int32_t)arr_pos + dd, 0, song.nchain < 16u ? song.nchain : 15);
            fmt_u(val, arr_pos + 1u, 0);
        } else if (i == 1u) {
            str_cat(lab, "PATTERN");
            if (arr_pos >= song.nchain) { str_cat(val, "-"); if (!dd || song.nchain >= 16u) return 1; song.chain[song.nchain] = song.chain[song.nchain - 1u]; song.nchain++; arr_pos = (uint8_t)(song.nchain - 1u); }
            if (dd) { song.chain[arr_pos] = (uint8_t)clampi((int32_t)song.chain[arr_pos] + dd, 0, NPAT - 1); touch(); }
            str_cat(val, SECT_NAME[song.chain[arr_pos] % NPAT]);   /* (XENOISE: its name; its letter on the tile) */
        } else if (i == 2u) {
            str_cat(lab, "PARTS");
            if (dd) {
                int32_t n = clampi((int32_t)song.nchain + dd, 1, 16);
                while ((int32_t)song.nchain < n) { song.chain[song.nchain] = song.chain[song.nchain - 1u]; song.nchain++; }
                song.nchain = (uint8_t)n;
                if (sq_chainpos >= song.nchain) sq_chainpos = 0;
                touch();
            }
            fmt_u(val, song.nchain, 0);
        } else {                                /* (XENOISE) FORM: the chain as one of the forms (BPM: on MST) */
            int f = form_of();
            str_cat(lab, "FORM");
            if (dd) {
                f = f < 0 ? 0 : clampi(f + (dd > 0 ? 1 : -1), 0, (int32_t)WB_NFORM - 1);
                form_set((uint32_t)f);
                if (sq_chainpos >= song.nchain) sq_chainpos = 0;
                if (arr_pos > song.nchain) arr_pos = song.nchain;
                touch();
            }
            str_cat(val, f < 0 ? "CUSTOM" : FORM_NAME[f]);
        }
        return 1;
    case PG_GEN: {
        static const char *const L[4] = {"CHANCE", "MUTATE", "DRIFT", "DENS"};
        if (sub < 2u) return ngen_knob(i, dd, lab, val);   /* (XENOISE) the next song's options */
        uint8_t *f[4] = {&tr->chance, &tr->mutate, &tr->drift, &tr->density};
        str_cat(lab, L[i]);
        if (dd) { *f[i] = (uint8_t)clampi((int32_t)*f[i] + dd, 0, 100); touch(); }
        e = fmt_u(val, *f[i], 0);
        str_cat(e, "%");
        return 1;
    }
    case PG_MIX:
        if (i == 1u) {
            str_cat(lab, "PAN");
            if (dd) { tr->pan = (int8_t)clampi(tr->pan + dd, -50, 50); touch(); }
            if (!tr->pan) str_cat(val, "C");
            else { e = str_cat(val, tr->pan < 0 ? "L" : "R"); fmt_u(e, (uint32_t)(tr->pan < 0 ? -tr->pan : tr->pan), 0); }
        } else {
            static const char *const L[4] = {"VOL", "", "VERB", "DELAY"};
            uint8_t *f = i == 0u ? &tr->vol : i == 2u ? &tr->rev : &tr->dly;
            str_cat(lab, L[i]);
            if (dd) { *f = (uint8_t)clampi((int32_t)*f + dd, 0, 100); touch(); }
            e = fmt_u(val, *f, 0);
            str_cat(e, "%");
        }
        return 1;
    case PG_MST:
        if (sub % MST_NSUB == 3u) {             /* (XENOISE) SYSTEM: the theme, HOM's black keys */
            uint8_t pg = page;
            int r;
            page = (uint8_t)(PG_MST + 100u);
            r = knob(i, dd, lab, val);
            page = pg;
            return r;
        }
        if (sub % MST_NSUB == 2u) {
            if (i == 1u) {
                str_cat(lab, "METER");
                if (dd) { song.meter = (uint8_t)clampi(song.meter + (dd > 0 ? 1 : -1), 0, NMETER - 1); touch(); }
                str_cat(val, METER_NAME[song.meter % NMETER]);
                return 1;
            }
            if (i) return 0;
            str_cat(lab, "PUMP");
            if (dd) { song.pump = (uint8_t)clampi(song.pump + dd, 0, 80); touch(); }
            e = fmt_u(val, song.pump, 0);
            str_cat(e, "%");
        } else if (sub % MST_NSUB == 0u) {
            if (i == 0u) {
                str_cat(lab, "BPM");
                if (dd) { song.bpm = (uint8_t)clampi(song.bpm + dd, 50, 200); touch(); }
                fmt_u(val, song.bpm, 0);
            } else if (i == 1u) {
                str_cat(lab, "SWING");
                if (dd) { song.swing = (uint8_t)clampi(song.swing + dd, 50, 75); touch(); }
                e = fmt_u(val, song.swing, 0);
                str_cat(e, "%");
            } else if (i == 2u) {
                str_cat(lab, "AGE");
                if (dd) { song.age = (uint8_t)clampi(song.age + dd, 0, 100); touch(); }
                e = fmt_u(val, song.age, 0);
                str_cat(e, "%");
            } else {
                str_cat(lab, "CRUSH");
                if (dd) { song.crush = (uint8_t)clampi(song.crush + dd, 0, 100); touch(); }
                if (song.crush < 6u) str_cat(val, "CLEAN");
                else { e = fmt_u(val, 16u - song.crush / 10u, 0); str_cat(e, " BIT"); }
            }
        } else {
            if (i == 0u) {
                str_cat(lab, "KEY");
                if (dd) { transpose_to((uint32_t)((song.key + 12 + (dd > 0 ? 1 : -1)) % 12)); touch(); }
                str_cat(val, NOTE_NM[song.key % 12u]);
            } else if (i == 1u) {
                str_cat(lab, "SCALE");
                if (dd) { song.scale = (uint8_t)clampi(song.scale + dd, 0, WB_NSCALE - 1); touch(); }
                str_cat(val, WB_SCALE_NAME[song.scale % WB_NSCALE]);
            } else if (i == 2u) {
                str_cat(lab, "SPACE");
                if (dd) { song.space = (uint8_t)clampi(song.space + dd, 0, 100); touch(); }
                e = fmt_u(val, song.space, 0);
                str_cat(e, "%");
            } else {
                str_cat(lab, "VOLUME");
                if (dd) { song.mvol = (uint8_t)clampi(song.mvol + dd, 0, 100); touch(); }
                e = fmt_u(val, song.mvol, 0);
                str_cat(e, "%");
            }
        }
        return 1;
    }
    return 0;
}

/* ================================================================ the F buttons */
static const char *fn_label(uint32_t i, char *buf)
{
    uint32_t t = trk();
    char *e;
    buf[0] = 0;
    switch (page) {
    case PG_ENV:
        if (i == 0u && env_npages(t) > 1u) { e = str_cat(buf, "PG "); e = fmt_u(e, sub % 2u + 1u, 0); str_cat(e, "/2"); return buf; }
        return (const char *const[]){"", "PLAY", "", ""}[i];
    case PG_LFO:
        if (i == 0u) { e = str_cat(buf, "PG "); e = fmt_u(e, (lfo_pg & 1u) + 1u, 0); str_cat(e, "/2"); return buf; }
        return (const char *const[]){"", "", "", "RESET"}[i];
    case PG_ARP:
        if (i == 0u) { e = str_cat(buf, "PG "); e = fmt_u(e, (sub & 1u) + 1u, 0); str_cat(e, "/2"); return buf; }
        if (i == 1u) return song.x.arp[t].hold ? "HOLD ON" : "HOLD";
        return (const char *const[]){"", "", "", "OFF"}[i];
    case PG_TRK:
        if (i == 2u) { e = str_cat(buf, "LEN "); fmt_u(e, plen(), 0); return buf; }
        return (const char *const[]){"CLR", "OFF", "", "DUP"}[i];
    case PG_SND:
        if (i == 0u) { e = str_cat(buf, "PG "); e = fmt_u(e, sub % snd_npages(t) + 1u, 0); *e++ = '/'; fmt_u(e, snd_npages(t), 0); return buf; }
        return (const char *const[]){"", "PLAY", "RAND", "INIT"}[i];
    case PG_SNG:
        if (arm_f == i) return "SURE?";
        if (sub & 1u) return (const char *const[]){"UNDO", "", "", ""}[i];   /* (XENOISE: page 2) */
        return (const char *const[]){"SAVE", "NEWSONG", "LOAD", "DELETE"}[i];
    case PG_CHD: return (const char *const[]){"WRITE", "NEWPROG", "UNDO", "HEAR"}[i];
    case PG_GEN:
        if (sub < 2u) {
            if (i == 0u) { e = str_cat(buf, "PG "); e = fmt_u(e, sub + 1u, 0); str_cat(e, "/3"); return buf; }
            if (i == 1u) return arm_f == SA_NEW ? "SURE?" : "CREATE";
            return (const char *const[]){"", "", "UNDO", "AUTO"}[i];
        }
        return (const char *const[]){"FILL", "DICE", "RESTORE", "FILLALL"}[i];
    case PG_MIX:
        if (i == 0u) return song.trk[t].mute ? "UNMUTE" : "MUTE";
        if (i == 1u) return song.trk[t].solo ? "UNSOLO" : "SOLO";
        return (const char *const[]){"", "", "ALL ON", "CLR TRK"}[i];
    case PG_MST:
        if (i == 0u) { e = str_cat(buf, "PG "); e = fmt_u(e, sub % MST_NSUB + 1u, 0); str_cat(e, "/4"); return buf; }
        if (i == 2u) return sit_on ? "SIT OFF" : "SIT";
        return (const char *const[]){"", "TAP", "", "CLR PAT"}[i];
    }
    return "";
}

static void sit_set(int on)
{
    sit_on = (uint8_t)on;
    sit_mute = 0;
    if (on) {
        sit_sect = 2;
        sit_left = 4;
        if (!sq_play) sq_req = 1;
        toast("SIT - LET IT PLAY");
    } else toast("SIT OFF");
}
static uint32_t fill_mode(uint32_t t, int alt)
{
    uint32_t m = song_mode(t, alt);
    if ((t == T_BRK && m == WM_OFF) || (TRK_TYPE[t] == TY_SYNTH && m == WK_OFF)) m = default_mode(t);
    return m;
}
static void fill_track(uint32_t pi, uint32_t t)
{
    const char *k, *s, *h;
    song_lanes(rnd(10) < 4u, &k, &s, &h);
    gen_track(pi, t, fill_mode(t, rnd(10) < 4u), song.trk[t].density, k, s, h);
}
static void hear_chord(void)
{
    uint32_t t = chord_trk();
    int32_t m;
    if (!song.ndeg) return;
    m = deg_midi(song.deg[(chd_slot < song.ndeg ? chd_slot : 0u) % 8u]);
    while (m < 55) m += 12;
    while (m > 67) m -= 12;
    ev_push(EV_OFF, t, 0, 0);
    audition(t, (uint32_t)m);
    aud_off_ms = fm1_ms + 900u;
}
static void prog_text(char *o)                  /* "Am F C G" */
{
    uint32_t k;
    char nm[8];
    o[0] = 0;
    for (k = 0; k < song.ndeg && k < 8u; k++) {
        chord_name(song.deg[k], nm, 0);
        if (k) str_cat(o, " ");
        str_cat(o, nm);
    }
}
static uint8_t sng_fresh_empty;                 /* (XENOISE) the next NEWSONG an empty one (OCT+ a tap on SAV) */
static void xu_undo(void)                       /* (XENOISE) UNDO: the last change back */
{
    uint32_t k, at = xu_at;
    if (xu_kind == U_NONE) { toast("NOTHING TO UNDO"); return; }
    if (xu_kind != U_CHAIN && sq_play) { toast("STOP FIRST TO UNDO"); return; }
    switch (xu_kind) {
    case U_SONG:
        if (song_load(UNDO_SLOT)) { toast("NOTHING TO UNDO"); xu_kind = U_NONE; return; }
        genre_pick = song.genre;
        prog_sel = song.prog;
        cur_row = 0;
        break;
    case U_SLOT: slot_copy(at, UNDO_SLOT); break;   /* (an empty UNDO: the slot empty again) */
    case U_INSERT:                              /* the inserted one out again */
        for (k = at; k + 1u < NSLOT; k++) slot_copy(k, k + 1u);
        store_erase(NSLOT - 1u);
        slot_ok[NSLOT - 1u] = 0;
        slot_name[NSLOT - 1u][0] = 0;
        break;
    case U_OUTSERT:                             /* the one taken out back in its place */
        for (k = NSLOT - 1u; k > at; k--) slot_copy(k, k - 1u);
        slot_copy(at, UNDO_SLOT);
        break;
    default:
        memcpy(song.chain, xu_chain, 16);
        memcpy(song.x.smute, xu_smute, 16);
        song.nchain = xu_nchain;
        if (sq_chainpos >= song.nchain) sq_chainpos = 0;
        if (arr_pos >= song.nchain) arr_pos = (uint8_t)(song.nchain - 1u);
        break;
    }
    xu_kind = U_NONE;
    toast("UNDONE");
    touch();
}
static void sng_do(uint32_t a)                  /* (XENOISE) SAV's actions, asked already */
{
    char buf[40];
    arm_f = 0xFF;
    if (a == SA_SAVE) {
        if (sq_play) { toast("STOP FIRST TO SAVE"); return; }
        xu_slot(slot_sel);                    /* (UNDO: what the slot held) */
        song.loop = 0;
        toast(song_save(slot_sel) ? "SAVE FAILED" : "SAVED");
        sng_fx = 1;
        sng_fx_ms = fm1_ms;
    } else if (a == SA_NEW) {
        int was = sq_play;
        xu_song();                            /* (UNDO: the song before; stopped only) */
        if (genre_pick >= WB_NGENRE || sng_fresh_empty) song_empty();   /* EMPTY: the sounds of the genre it had, no notes */
        else song_new(genre_pick);
        sng_fresh_empty = 0;
        sng_fx = 4;
        sng_fx_ms = fm1_ms;
        cur_row = 0;
        chd_slot = 0;
        prog_sel = song.prog;
        sit_mute = 0;
        if (was) sq_req = 1;
        buf[0] = 0;
        str_cat(str_cat(buf, "NEW SONG "), song.name);
        toast(buf);
        touch();
    } else if (a == SA_LOAD) {
        xu_song();
        if (song_load(slot_sel)) { toast("NOTHING TO LOAD"); return; }
        genre_pick = song.genre;
        prog_sel = song.prog;
        cur_row = 0;
        toast("LOADED");
        sng_fx = 2;
        sng_fx_ms = fm1_ms;
        touch();
    } else if (a == SA_DELETE) {
        if (sq_play) { toast("STOP FIRST TO DELETE"); return; }
        xu_slot(slot_sel);
        store_erase(slot_sel);
        slot_ok[slot_sel] = 0;
        slot_name[slot_sel][0] = 0;
        toast("DELETED");
        sng_fx = 3;
        sng_fx_ms = fm1_ms;
    } else if (a == SA_COPY) {
        sng_clip = slot_sel;
        buf[0] = 0;
        fmt_u(str_cat(buf, "COPIED "), slot_sel, 2);
        toast(buf);
        sng_fx = 5;
    } else if (a == SA_PASTE) {
        if (sng_clip == slot_sel) return;
        xu_slot(slot_sel);
        toast(slot_copy(slot_sel, sng_clip) ? "PASTE FAILED" : "PASTED");
        sng_fx = 1;
    } else if (a == SA_INSERT) {            /* the songs from here on one slot later, the copied one here */
        uint32_t k, src = sng_clip;
        for (k = NSLOT - 1u; k > slot_sel; k--) slot_copy(k, k - 1u);
        if (src >= slot_sel) src++;
        slot_copy(slot_sel, src);
        sng_clip = (uint8_t)src;
        xu_kind = U_INSERT;
        xu_at = slot_sel;
        toast("INSERTED");
        sng_fx = 6;
    } else {                                /* OUTSERT: the slot gone, the songs after it one earlier */
        uint32_t k;
        xu_slot(slot_sel);
        xu_kind = U_OUTSERT;
        for (k = slot_sel; k + 1u < NSLOT; k++) slot_copy(k, k + 1u);
        store_erase(NSLOT - 1u);
        slot_ok[NSLOT - 1u] = 0;
        slot_name[NSLOT - 1u][0] = 0;
        if (sng_clip == slot_sel) sng_clip = 0xFF;
        else if (sng_clip != 0xFF && sng_clip > slot_sel) sng_clip--;
        toast("OUTSERTED");
        sng_fx = 7;
    }
    sng_fx_ms = fm1_ms;
}
static void fn_press(uint32_t i)
{
    uint32_t t = trk(), pi = song.cur % NPAT, r;
    pat_t *p = P();
    char buf[40];
    switch (page) {
    case PG_ENV:
        if (i == 1u) audition(t, env_note(t));
        break;
    case PG_LFO:
        if (i == 0u) lfo_pg ^= 1u;
        else if (i == 3u) { song.x.lfo[t].set = 0; wb_resound |= (uint8_t)(1u << t); toast("LFO: THE SOUND'S OWN"); touch(); }
        break;
    case PG_ARP:
        if (i == 0u) sub ^= 1u;
        else if (i == 1u) { wb_arp_t *a = &song.x.arp[t]; if (!a->set) { a->set = 1; a->mode = 1; a->rate = 2; a->oct = 1; a->gate = 64; a->prob = 127; } a->hold ^= 1u; touch(); }
        else if (i == 3u) { song.x.arp[t].mode = 0; toast("ARP OFF"); touch(); }
        break;
    case PG_TRK:
        if (i == 0u) { memset(&p->c[t][cur_row % plen()], 0, sizeof(cell_t)); touch(); }
        else if (i == 1u) { cell_set(&p->c[t][cur_row % plen()], NOTE_OFF, 0, 0, 0); cur_row = (uint8_t)((cur_row + 1u) % plen()); touch(); }
        else if (i == 2u) {
            uint32_t L = plen(), n = L == 16u ? 32u : L == 32u ? 64u : 16u, k;
            for (k = 0; k < NT; k++) for (r = L; r < n; r++) p->c[k][r] = p->c[k][r % L];
            p->len = (uint8_t)n;
            if (cur_row >= n) cur_row = 0;
            buf[0] = 0;
            fmt_u(str_cat(buf, "PATTERN LENGTH "), n, 0);
            toast(buf);
            touch();
        } else {
            uint32_t to, k, any;
            for (to = 0; to < NPAT; to++) {
                if (to == pi) continue;
                for (k = 0, any = 0; k < NT && !any; k++) for (r = 0; r < NROW; r++) if (song.pat[to].c[k][r].n) { any = 1; break; }
                if (!any) break;
            }
            if (to >= NPAT) to = (pi + 1u) % NPAT;
            song.pat[to] = *p;
            if (sq_play) sq_cue = (uint8_t)to;
            else song.cur = (uint8_t)to;
            buf[0] = 0;
            {
                char *e = str_cat(buf, "COPIED TO PATTERN ");
                e[0] = (char)('1' + to);
                e[1] = 0;
            }
            toast(buf);
            touch();
        }
        break;
    case PG_SND:
        if (i == 0u) sub = (uint8_t)((sub + 1u) % snd_npages(t));
        else if (i == 1u) audition(t, last_note[t]);
        else if (i == 2u) {
            uint32_t s, k, n = snd_npages(t);
            for (s = 0; s < n; s++)
                for (k = 0; k < 4u; k++) {
                    const pd_t *d = snd_pd(t, s, k);
                    if (d && d->rnd) pd_set(d, t, d->lo + (int32_t)rnd((uint32_t)(d->hi - d->lo) + 1u));
                }
            buf[0] = 0;
            str_cat(str_cat(buf, "RANDOM "), TRK_ID[t]);
            toast(buf);
            touch();
        } else {
            track_t keep = song.trk[t];
            track_defaults(t);
            song.trk[t].vol = keep.vol; song.trk[t].pan = keep.pan; song.trk[t].rev = keep.rev; song.trk[t].dly = keep.dly;
            song.trk[t].mute = keep.mute; song.trk[t].solo = keep.solo; song.trk[t].chance = keep.chance;
            song.trk[t].mutate = keep.mutate; song.trk[t].drift = keep.drift; song.trk[t].density = keep.density;
            toast("SOUND RESET");
            touch();
        }
        break;
    case PG_SNG:
        if (sub & 1u) { if (i == 0u) xu_undo(); break; }   /* (XENOISE: page 2, F1 UNDO) */
        if (arm_f != i || fm1_ms - arm_ms > ARM_MS) {
            if (i == 0u && !slot_ok[slot_sel]) goto do_it;   /* (an empty slot: no question) */
            arm_f = (uint8_t)i;
            arm_ms = fm1_ms;
            toast(i == 1u ? (genre_pick >= WB_NGENRE ? "AGAIN: AN EMPTY SONG" : "AGAIN: NEW SONG ON 1-8") : i == 0u ? "AGAIN: OVERWRITE SLOT" : i == 2u ? "AGAIN: LOAD THE SLOT" : "AGAIN: DELETE THE SLOT");
            return;
        }
    do_it:
        sng_do(i);
        break;
    case PG_CHD:
        if (i == 0u) {
            seed_take(pi);
            for (t = T_BAS; t < NT; t++) fill_track(pi, t);
            prog_text(buf);
            toast(buf);
            touch();
        } else if (i == 1u) {
            int8_t d[8];
            uint32_t n = (const uint8_t[]){2, 4, 4, 4}[rnd(4)], k;
            d[0] = 0;
            for (k = 1; k < n; k++) { int8_t x; do x = (int8_t)rnd(7); while (x == d[k - 1u]); d[k] = x; }
            prog_set(d, n, song.rows, song.scale, WB_NPROG - 1u);
            prog_sel = WB_NPROG - 1u;
            prog_text(buf);
            toast(buf);
            touch();
        } else if (i == 2u) toast(prog_undo() ? "PROGRESSION BACK" : "NOTHING TO UNDO");
        else hear_chord();
        break;
    case PG_GEN:
        if (sub < 2u) {                         /* (XENOISE) the next song: F1 its next page, F2 GENERATE (asked), F4 AUTO */
            if (i == 0u) sub = (uint8_t)((sub + 1u) % 3u);
            else if (i == 1u) { if (arm_f == SA_NEW && fm1_ms - arm_ms <= ARM_MS) sng_do(SA_NEW); else ngen_ask(); }
            else if (i == 2u) xu_undo();
            else if (i == 3u) { ng_prog = ng_cspace = ng_mspace = ng_bpm = ng_key = ng_form = ng_bass = 0; toast("ALL AUTO"); }
            break;
        }
        if (i == 0u) { seed_take(pi); fill_track(pi, t); buf[0] = 0; str_cat(str_cat(buf, "FILLED "), TRK_ID[t]); toast(buf); }
        else if (i == 1u) { seed_take(pi); mutate_track(pi, t, 60); buf[0] = 0; str_cat(str_cat(buf, "MUTATED "), TRK_ID[t]); toast(buf); }
        else if (i == 2u) {
            if (seed_of != pi) { toast("NOTHING TO RESTORE"); return; }
            *p = seed_pat;
            seed_of = 0xFF;
            toast("PATTERN RESTORED");
        } else { seed_take(pi); gen_pattern(pi, sect_v(pi)); toast("NEW PATTERN - RESTORE UNDOES"); }
        touch();
        break;
    case PG_MIX:
        if (i == 0u) song.trk[t].mute ^= 1u;
        else if (i == 1u) song.trk[t].solo ^= 1u;
        else if (i == 2u) { for (r = 0; r < NT; r++) song.trk[r].mute = song.trk[r].solo = 0; }
        else { seed_take(pi); memset(p->c[t], 0, sizeof p->c[t]); toast("TRACK CLEARED - RESTORE ON GEN"); }
        touch();
        break;
    case PG_MST:
        if (i == 0u) sub = (uint8_t)((sub + 1u) % MST_NSUB);
        else if (i == 1u) {
            if (ntap && fm1_ms - taps[(ntap - 1u) % 4u] > 2500u) ntap = 0;
            taps[ntap % 4u] = fm1_ms;
            ntap++;
            if (ntap >= 3u) {
                uint32_t n = ntap > 4u ? 4u : ntap, d = (fm1_ms - taps[(ntap - n) % 4u]) / (n - 1u);
                if (d) song.bpm = (uint8_t)clampi((int32_t)(60000u / d), 50, 200);
                touch();
            }
            buf[0] = 0;
            fmt_u(str_cat(buf, "TAP "), ntap, 0);
            toast(buf);
        } else if (i == 2u) sit_set(!sit_on);
        else { seed_take(pi); for (t = 0; t < NT; t++) memset(p->c[t], 0, sizeof p->c[t]); toast("PATTERN CLEARED - RESTORE ON GEN"); touch(); }
        break;
    }
}

/* ================================================================ the pads */
static void chord_pad(uint32_t i)
{
    int8_t d[8];
    uint32_t n = song.ndeg, k;
    char buf[40];
    for (k = 0; k < 8u; k++) d[k] = song.deg[k];
    if (i >= 8u) {
        k = i - 8u;
        if (k > n || k >= 8u) return;
        chd_slot = (uint8_t)k;
        if (k < n) hear_chord();
        return;
    }
    if (i == 7u) { if (n < 2u) return; n--; if (chd_slot >= n) chd_slot = (uint8_t)(n - 1u); }
    else {
        if (chd_slot >= n) { if (n >= 8u) return; d[n++] = (int8_t)i; }
        else d[chd_slot] = (int8_t)i;
    }
    prog_set(d, n, song.rows, song.scale, WB_NPROG - 1u);
    prog_sel = WB_NPROG - 1u;
    touch();
    if (i < 7u) { hear_chord(); if (chd_slot < 7u && chd_slot + 1u == song.ndeg && song.ndeg < 8u) chd_slot++; }
    buf[0] = 0;
    prog_text(str_cat(buf, "CUSTOM "));
    toast(buf);
}
static void rec_note(uint32_t t, uint32_t n)
{
    uint32_t stored = TRK_TYPE[t] == TY_BREAK ? n + 1u : n;
    if (sq_play) {
        pat_t *p = &song.pat[sq_pi % NPAT];
        uint32_t L = p->len ? p->len : 16u, r = sq_row;
        if (sq_frame > sq_rowlen / 2u) r = (r + 1u) % L;
        cell_set(&p->c[t][r], stored, 12, 0, 0);
    } else {
        cell_set(&P()->c[t][cur_row % plen()], stored, 12, 0, 0);
        cur_row = (uint8_t)((cur_row + 1u) % plen());
    }
    touch();
}
/* ---- the punch-ins (PERFORM): white keys 1..13 */
#define NPUNCH 13u
static const char *const PUNCH_NAME[NPUNCH] = {"ROLL 4", "ROLL 8", "ROLL 16", "ROLL 32", "REVERSE", "HALF", "SCATTER",
                                               "DUB", "WASH", "FILTER", "3/4", "7/8", "5/4"};
static void punch(uint32_t i, int on)
{
    switch (i) {
    case 0: case 1: case 2: case 3:
        if (on) {
            uint32_t n = (const uint8_t[]){4, 2, 1, 1}[i];
            loop_start = (uint8_t)(sq_pos - sq_pos % n);
            punch_loop = (uint8_t)n;
            punch_roll = i == 3u;
        } else { punch_loop = 0; punch_roll = 0; }
        break;
    case 4: if (on) rev_at = (uint8_t)sq_pos; punch_rev = (uint8_t)on; break;
    case 5: punch_half = (uint8_t)on; break;
    case 6: punch_scat = (uint8_t)on; break;
    case 7: punch_dub = (uint8_t)on; break;
    case 8: punch_wash = (uint8_t)on; break;
    case 9: punch_lpf = (uint8_t)on; break;
    case 10: case 11: case 12: punch_meter = on ? (uint8_t)(const uint8_t[]){1, 4, 3}[i - 10u] : 0; break;
    }
}
static void punches_off(void)
{
    uint32_t i;
    for (i = 0; i < NPUNCH; i++) punch(i, 0);
    punch_keys = 0;
}
static void mute_key(uint32_t t)                /* PERFORM: a black key mutes (or solos) its track */
{
    track_t *tr = &song.trk[t];
    char buf[24];
    if (perf_solo) tr->solo ^= 1u;
    else tr->mute ^= 1u;
    buf[0] = 0;
    str_cat(str_cat(buf, TRK_ID[t]), perf_solo ? (tr->solo ? " SOLO" : " SOLO OFF") : (tr->mute ? " MUTED" : " ON"));
    toast(buf);
    touch();
}

static void arr_do(uint32_t i)                  /* (XENOISE) SEQ's K1..K4, asked (OCT+) */
{
    uint32_t n = song.nchain, at = arr_pos < n ? arr_pos : n ? n - 1u : 0u, k;
    arm_f = 0xFF;
    if (i == 0u) { arr_clip = song.chain[at]; toast("COPIED"); return; }
    if (arr_clip == 0xFF && i != 3u) return;
    xu_chain_take();
    if (i == 1u) { song.chain[at] = arr_clip; toast("PASTED"); }
    else if (i == 2u) {                         /* INSERT: the copy at the slot, the rest moved on */
        if (n >= 16u) return;
        for (k = n; k > arr_pos && k > 0u; k--) { song.chain[k] = song.chain[k - 1u]; song.x.smute[k] = song.x.smute[k - 1u]; }
        song.chain[arr_pos < n ? arr_pos : n] = arr_clip;
        song.x.smute[arr_pos < n ? arr_pos : n] = 0;
        song.nchain++;
        toast("INSERTED");
    } else {                                    /* OUTSERT: the slot gone, the rest moved back */
        if (n <= 1u) return;
        for (k = at; k + 1u < n; k++) { song.chain[k] = song.chain[k + 1u]; song.x.smute[k] = song.x.smute[k + 1u]; }
        song.x.smute[n - 1u] = 0;
        song.nchain--;
        if (sq_chainpos >= song.nchain) sq_chainpos = 0;
        if (arr_pos >= song.nchain) arr_pos = (uint8_t)(song.nchain - 1u);
        toast("SLOT OUT, THE REST BACK");
    }
    touch();
}
static void fx_pilot(int32_t d)                 /* (XENOISE) PERFORM, SELECT: the one ALGORITHM is on, right on, left off */
{
    if (fx_row == 0u) {
        uint32_t w = fx_col % NPUNCH, m = 1u << KEY_OF_WHITE[w];
        if (d > 0 && !(punch_keys & m)) { punch_keys |= m; punch(w, 1); toast(PUNCH_NAME[w]); }
        else if (d < 0 && (punch_keys & m)) { punch_keys &= ~m; punch(w, 0); toast("OFF"); }
    } else {
        track_t *tr = &song.trk[fx_col % NT];
        uint8_t *f = perf_solo ? &tr->solo : &tr->mute;
        if ((d > 0) != (*f != 0)) mute_key(fx_col % NT);
    }
}
static void fn_press_now(uint32_t i)            /* (XENOISE) an F button's action, no question first (SAV: OCT+ a tap) */
{
    arm_f = (uint8_t)i;
    arm_ms = fm1_ms;
    fn_press(i);
}
static void live_queue(uint32_t w)              /* (XENOISE) LIVE: pattern w next, looped till another (a white key, K1, PRESETS) */
{
    char buf[24], *q;
    w %= NPAT;
    if (sq_play) sq_cue = (uint8_t)w;
    else song.cur = (uint8_t)w;
    song.loop = 1;
    if (cur_row >= plen()) cur_row = 0;
    buf[0] = 0;
    q = str_cat(buf, sq_play ? "NEXT: P" : "P");
    q[0] = (char)('1' + w);
    q[1] = ' ';
    q[2] = 0;
    str_cat(q, SECT_NAME[w]);
    toast(buf);
}
static uint32_t live_n(void)                    /* (XENOISE) SEQ LIVE: patterns 1..this (the highest the song plays or has notes in) */
{
    uint32_t n = 1, k, t, r;
    for (k = 0; k < song.nchain; k++) if (song.chain[k] % NPAT + 1u > n) n = song.chain[k] % NPAT + 1u;
    for (k = n; k < NPAT; k++)
        for (t = 0; t < NT; t++) for (r = 0; r < NROW; r++) if (song.pat[k].c[t][r].n) { n = k + 1u; t = NT; break; }
    return n;
}
/* ---- the arrangement: the chain of patterns the song plays */
static void arr_jump(uint32_t pos)              /* play from this part at the end of the pattern */
{
    if (!song.nchain) return;
    pos %= song.nchain;
    if (sq_play) sq_chainpos = (uint8_t)((pos + song.nchain - 1u) % song.nchain);
    else song.cur = song.chain[pos];
    song.loop = 0;
}
static void arr_set(uint32_t pat)               /* white 1..8 on ARR: this pattern at the cursor, then the next part */
{
    char buf[24], *e;
    if (arr_pos >= song.nchain) { if (song.nchain >= 16u) return; arr_pos = song.nchain++; }
    song.chain[arr_pos] = (uint8_t)pat;
    buf[0] = 0;
    e = fmt_u(str_cat(buf, "PART "), arr_pos + 1u, 0);
    e = str_cat(e, ": ");
    e[0] = (char)('1' + pat);
    e[1] = 0;
    toast(buf);
    if (arr_pos < 15u) arr_pos++;
    touch();
}
static void arr_launch(uint32_t k)              /* live: a white key on ARR goes to that part at the next bar line */
{
    char buf[28], *e;
    if (k >= song.nchain) return;
    arr_pos = (uint8_t)k;
    buf[0] = 0;
    e = fmt_u(str_cat(buf, "PART "), k + 1u, 0);
    if (sq_play) { sq_jump = (uint8_t)k; str_cat(e, ": AT THE BAR LINE"); }
    else { sq_start = (uint8_t)k; song.cur = song.chain[k] % NPAT; str_cat(e, ": PLAY STARTS HERE"); }
    toast(buf);
}
static void arr_fn(uint32_t i)
{
    uint32_t k, n = song.nchain;
    (void)k; (void)n;
    if (i == 3u) { xu_undo(); return; }         /* (XENOISE) UNDO */
    if (i != 1u) xu_chain_take();
    if (i == 0u) {                              /* (XENOISE) FORM: the next of the forms */
        int f = form_of();
        char buf[24];
        form_set(f < 0 ? 0u : ((uint32_t)f + 1u) % WB_NFORM);
        if (sq_chainpos >= song.nchain) sq_chainpos = 0;
        if (arr_pos >= song.nchain) arr_pos = (uint8_t)(song.nchain - 1u);
        buf[0] = 0;
        str_cat(str_cat(buf, "FORM: "), FORM_NAME[form_of() < 0 ? 0 : form_of()]);
        toast(buf);
    } else if (i == 1u) { song.loop ^= 1u; toast(song.loop ? "LOOP THE PATTERN" : "PLAY THE SEQUENCE"); }
    else {                                      /* (XENOISE) SHUFFLE: another form (a part goes on with a white key) */
        char buf[24];
        form_shuffle();
        if (sq_chainpos >= song.nchain) sq_chainpos = 0;
        if (arr_pos > song.nchain) arr_pos = song.nchain;
        buf[0] = 0;
        str_cat(fmt_u(str_cat(buf, "SHUFFLED: "), song.nchain, 0), " PARTS");
        toast(buf);
    }
    touch();
}

/* ================================================================ input */
static void page_go(uint32_t pg)
{
    if (pg == PG_ARR && page != PG_ARR) prev_page = page;
    page = (uint8_t)pg;
    page_ms = fm1_ms;
    sub = 0;
    arm_f = 0xFF;
    vis_on = 0;
    vis_ar = 0;
    vis_yaw = 0;
    sng_taps = 0;
    hom_edit = 0;
    pilot_col = 0;
    arr_live = 0;
}
static void ui_input(void)
{
    uint32_t rel, b = fm1_input_edges(&rel), held = fm1_in.buttons, keys = fm1_in.notes, edges = fm1_input_note_edges(), k, i;
    int32_t d;
    uint32_t t = trk();
    edges |= keys & ~prev_keys;
    if (b & (1u << BTN_HOME)) home_used = page != PG_TRK;   /* (XENOISE) HOME alone on HOM: the edit mode on, off (its release) */
    /* the pages (SEQ: a tap opens ARR, again goes back; held, it picks patterns) */
    for (i = 0; i < PG_N; i++)
        if (i != PG_ARR && i != PG_SNG && (b & (1u << PG_BTN[i]))) {
            if (fx_latch) { fx_latch = 0; punches_off(); fx_reset(); }   /* (XENOISE: a page closes PERFORM) */
            if (tab_of(page) == i) {            /* (XENOISE) a tab's button again: */
                if (i == PG_ENV) sub = (uint8_t)((sub + 1u) % env_npages(t));   /* ENV: its page 2 */
                else if (i == PG_SND) sub = (uint8_t)((sub + 1u) % snd_npages(t));
                else if (i == PG_MST) page_go(page == PG_MST ? PG_MIX : PG_MST);   /* GLO: the mixer, the settings */
                else if (i == PG_LFO) lfo_pg ^= 1u;   /* LFO: its page 2 */
            } else page_go(i);
        }
    /* (XENOISE) SAVE: a tap SAV (on SAV its page 2, on GEN GEN's next); held, GEN (on GEN, SAV) */
    if (b & (1u << BTN_SAVE)) { save_down_ms = fm1_ms; save_used = 0; if (fx_latch) { fx_latch = 0; punches_off(); fx_reset(); } }
    if ((held & (1u << BTN_SAVE)) && !save_used && fm1_ms - save_down_ms >= 450u) { save_used = 1; page_go(page == PG_GEN ? PG_SNG : PG_GEN); }
    if ((rel & (1u << BTN_SAVE)) && !save_used) {
        save_used = 1;
        if (page == PG_SNG) { sub ^= 1u; arm_f = 0xFF; }
        else if (page == PG_GEN) { sub = (uint8_t)((sub + 1u) % 3u); arm_f = 0xFF; }
        else page_go(PG_SNG);
    }
    if (b & (1u << BTN_SEQ)) { seq_down_ms = fm1_ms; seq_used = 0; }
    if ((rel & (1u << BTN_SEQ)) && !seq_used && fm1_ms - seq_down_ms < 600u) {   /* (XENOISE) SEQ, again: LIVE, again: SEQ */
        if (page == PG_ARR) { arr_live ^= 1u; toast(arr_live ? "LIVE - WHITE KEYS QUEUE" : "SEQUENCE"); }
        else { page_go(PG_ARR); arr_pos = (uint8_t)(sq_play && !song.loop ? sq_chainpos : 0u); }
    }
    if ((rel & (1u << BTN_HOME)) && !home_used && page == PG_TRK && !fx_on()) {
        hom_edit ^= 1u;
        vis_on = 0;
        toast(hom_edit ? "EDIT: NOTE VEL FX AMT" : "TRACKER");
    }
    /* (XENOISE) FX and ARP: a tap opens, a tap closes (the page under them stays as it was) */
    if (b & (1u << BTN_FX)) {
        if (fx_latch) { fx_latch = 0; punches_off(); fx_reset(); toast("PERFORM CLOSED"); }
        else { fx_latch = 1; toast("PERFORM - FX CLOSES"); }
    }
    if (vis_on && page == PG_TRK && !fx_on()) {  /* (XENOISE) the visualizer: ARP held, ALL RANGE; HOME, home again */
        if (b & (1u << BTN_ARP)) { arp_rel_ms = fm1_ms; vis_arp_held = 1; }
        if (!(held & (1u << BTN_ARP))) vis_arp_held = 0;
        if (vis_arp_held && !vis_ar && fm1_ms - arp_rel_ms >= 450u) { vis_arp_held = 0; vis_ar = 1; vis_ar_ms = fm1_ms; vis_yaw = 0; }
        if ((b & (1u << BTN_HOME)) && vis_ar == 2u) { vis_ar = 3; vis_ar_ms = fm1_ms; home_used = 1; }
        b &= ~(1u << BTN_ARP);
        arp_used = 1;
    }
    /* (XENOISE) ARP: a tap its page (the track's arp on JIANT's), again its second; held on HOM, NOTE REPEAT on, off (a pad
     * repeats at the arp's rate while it plays) */
    if (b & (1u << BTN_ARP)) { arp_down_ms = fm1_ms; arp_used = 0; }
    if ((held & (1u << BTN_ARP)) && !arp_used && page == PG_TRK && fm1_ms - arp_down_ms >= 450u) {
        arp_used = 1;
        arp_latch ^= 1u;
        rpt_held = 0;
        toast(arp_latch ? "NOTE REPEAT ON (HOLD ARP: OFF)" : "NOTE REPEAT OFF");
    }
    if ((rel & (1u << BTN_ARP)) && !arp_used) {
        arp_used = 1;
        if (fx_latch) { fx_latch = 0; punches_off(); fx_reset(); }
        if (page != PG_ARP) page_go(PG_ARP);
        else sub ^= 1u;
    }
    (void)fx_rel_ms; (void)arp_rel_ms;
    if (b & (1u << BTN_PLAY)) {
        if (held & (1u << BTN_HOME)) { home_used = 1; sit_set(!sit_on); }
        else if (sq_play) { sq_req = 2; if (sit_on) sit_set(0); }
        else sq_req = 1;
    }
    if (b & (1u << BTN_REC)) { rec_on ^= 1u; toast(rec_on ? "REC - PADS WRITE NOTES" : "REC OFF"); }
    if ((b & (1u << BTN_OCT_DOWN)) && (held & (1u << BTN_SEQ))) { seq_used = 1; xu_undo(); b &= ~(1u << BTN_OCT_DOWN); }   /* (XENOISE) SEQ held + OCT-: UNDO (the page stays) */
    if (b & ((1u << BTN_OCT_DOWN) | (1u << BTN_OCT_UP))) {
        int up = (b >> BTN_OCT_UP) & 1u;
        if (page == PG_SNG) {                   /* (XENOISE) OCT+: yes; a tap a new empty song, four a generated one. */
            char buf[32], *q;                   /* OCT-: no; a tap deletes the slot's song, asked (OCT- again) */
            if (arm_f == 3u && arm_oct) {
                if (up) { arm_f = 0xFF; toast("CANCELLED"); }
                else sng_do(SA_DELETE);
            } else if (arm_f != 0xFF) {
                if (up) sng_do(arm_f);
                else { arm_f = 0xFF; toast("CANCELLED"); }
            } else if (!up) {
                if (!slot_ok[slot_sel]) toast("NOTHING TO DELETE");
                else if (sq_play) toast("STOP FIRST TO DELETE");
                else {
                    arm_f = 3;
                    arm_oct = 1;
                    arm_ms = fm1_ms;
                    buf[0] = 0;
                    q = fmt_u(str_cat(buf, "DELETE "), slot_sel, 2);
                    str_cat(q, "? OCT- AGAIN");
                    toast(buf);
                }
            } else {
                sng_tap_ms = fm1_ms;
                if (++sng_taps >= 4u) {
                    sng_taps = 0;
                    arm_f = 1;
                    arm_ms = fm1_ms;
                    if (genre_pick >= WB_NGENRE) genre_pick = song.genre;   /* (EMPTY picked: the song's genre) */
                    buf[0] = 0;
                    q = str_cat(str_cat(buf, "GENERATE "), WB_GENRES[genre_pick % WB_NGENRE].name);
                    buf[22] = 0;
                    str_cat(buf, "? OCT+");
                    toast(buf);
                    (void)q;
                } else {
                    buf[0] = 0;
                    q = fmt_u(str_cat(buf, "OCT+ "), sng_taps, 0);
                    str_cat(q, "/4: 4 GENERATES");
                    toast(buf);
                }
            }
        } else if (page == PG_GEN && sub < 2u) {   /* (XENOISE) GEN: OCT+ generate (asked: OCT+ again), OCT- no */
            if (arm_f == SA_NEW) { if (up) sng_do(SA_NEW); else { arm_f = 0xFF; toast("CANCELLED"); } }
            else if (up) ngen_ask();
        } else if (page == PG_ARR) {            /* (XENOISE) OCT+ adds a slot (a pattern the song does not play yet, or the
                                                 * one picked again), OCT- deletes the last */
            uint32_t n = song.nchain, q, k2;
            if (arm_f >= ARR_ASK && arm_f < ARR_ASK + 4u) {   /* (a knob's question: OCT+ yes, OCT- no) */
                if (up) arr_do(arm_f - ARR_ASK);
                else { arm_f = 0xFF; toast("CANCELLED"); }
            } else if (up) {
                if (n >= 16u) toast("16 SLOTS AT MOST");
                else {
                    xu_chain_take();
                    for (q = 0; q < NPAT; q++) { for (k2 = 0; k2 < n && song.chain[k2] != q; k2++) {} if (k2 == n) break; }
                    song.chain[n] = (uint8_t)(q < NPAT ? q : song.chain[arr_pos < n ? arr_pos : n - 1u]);
                    song.nchain++;
                    arr_pos = (uint8_t)n;
                    toast("SLOT ADDED");
                    touch();
                }
            } else if (n > 1u) {
                xu_chain_take();
                song.nchain--;
                if (sq_chainpos >= song.nchain) sq_chainpos = 0;
                if (arr_pos >= song.nchain) arr_pos = (uint8_t)(song.nchain - 1u);
                toast("LAST SLOT DELETED");
                touch();
            }
        } else if (TRK_TYPE[t] == TY_SYNTH) {
            char buf[24];
            pad_oct[t] = (int8_t)clampi(pad_oct[t] + (up ? 1 : -1), -2, 2);
            buf[0] = 0;
            fmt_s(str_cat(buf, "PADS OCTAVE "), pad_oct[t]);
            toast(buf);
        }
    }
    /* the encoders */
    /* (XENOISE) the PILOT SYSTEM: PRESETS the rows (the slot, the group, the genre), ALGORITHM the columns (the track, or the
     * knob on pages with no tracks), SELECT turns what PRESETS used to (the pattern; the knob ALGORITHM is on), K1..K4
     * the page's knobs, the page's button again its next page */
    if ((d = fm1_enc_take(ENC_PRESETS)) != 0) {
        if (vis_on && vis_ar) { if (vis_ar == 2u) vis_yaw = (uint16_t)((int32_t)vis_yaw + d * 0x400); }   /* (ALL RANGE: it turns, left or right) */
        else if (fx_on()) { fx_row = (uint8_t)clampi((int32_t)fx_row + d, 0, 1); fx_col = (uint8_t)clampi(fx_col, 0, fx_row ? NT - 1 : NPUNCH - 1); }   /* (PERFORM: the punch-ins, the tracks) */
        else if (page == PG_GEN && sub < 2u) { char lab[16], val[24]; uint8_t s0 = sub; sub = 0; ngen_knob(0, d, lab, val); sub = s0; }
        else if (page == PG_SNG) slot_sel = (uint8_t)clampi((int32_t)slot_sel + d, 1, NSLOT - 1);
        else if (page == PG_CHD) chd_slot = (uint8_t)clampi((int32_t)chd_slot + d, 0, song.ndeg < 8u ? song.ndeg : 7);
        else if (page == PG_ARR && arr_live) live_queue((uint32_t)clampi((int32_t)(sq_play && sq_cue < NPAT ? sq_cue : song.cur % NPAT) + d, 0, (int32_t)live_n() - 1));
        else if (page == PG_ARR) arr_pos = (uint8_t)clampi((int32_t)arr_pos + d, 0, song.nchain < 16u ? song.nchain : 15);
        else if (page == PG_MST) { sub = (uint8_t)(((int32_t)sub + d % (int32_t)MST_NSUB + (int32_t)MST_NSUB) % (int32_t)MST_NSUB); pilot_col = 0; }   /* (XENOISE) its group */
        else cur_row = (uint8_t)(((int32_t)cur_row + d % (int32_t)plen() + (int32_t)plen()) % (int32_t)plen());
    }
    if ((d = fm1_enc_take(ENC_ALGORITHM)) != 0) {   /* (XENOISE) the tracks, BRK .. SYN (no wrap); HOM: past SYN the visualizer */
        if (vis_on && vis_ar && page == PG_TRK) { if (vis_ar == 2u) vis_yaw = (uint16_t)((int32_t)vis_yaw + d * 0x400); }   /* (ALL RANGE: as PRESETS) */
        else if (fx_on()) fx_col = (uint8_t)clampi((int32_t)fx_col + d, 0, fx_row ? NT - 1 : NPUNCH - 1);   /* (PERFORM: along the row) */
        else if (page == PG_MST || page == PG_SNG || (page == PG_GEN && sub < 2u)) {   /* (XENOISE) the knob (SELECT turns it) */
            char lab[16], val[24];
            int32_t c = clampi((int32_t)pilot_col + d, 0, 3);
            while (c > 0 && c < 4 && !knob((uint32_t)c, 0, lab, val)) c += d > 0 ? 1 : -1;
            if (c >= 0 && c < 4 && knob((uint32_t)c, 0, lab, val)) pilot_col = (uint8_t)c;
        } else if (page == PG_TRK && !fx_on() && vis_on) { if (d < 0) vis_on = 0; }
        else if (page == PG_TRK && !fx_on() && (int32_t)sel_trk + d >= (int32_t)NT) { sel_trk = NT - 1u; vis_open(); }
        else sel_trk = (uint8_t)clampi((int32_t)sel_trk + d, 0, NT - 1);
        if (page == PG_SND) sub = 0;
    }
    if ((d = fm1_enc_take(ENC_SELECT)) != 0) {
        char lab[16], val[24];
        uint32_t s0 = sub;
        if (fx_on()) fx_pilot(d);              /* (PERFORM: on, off) */
        else if (page == PG_SND) { sub = TRK_TYPE[t] == TY_DRUMS && sub >= 2u ? 2u : 0u; knob(0, d, lab, val); sub = (uint8_t)s0; }
        else if (page == PG_SNG || (page == PG_GEN && sub < 2u)) knob(pilot_col, d, lab, val);
        else if (page == PG_CHD) knob(0u, d, lab, val);
        else if (page == PG_MST) knob(pilot_col, d, lab, val);   /* (XENOISE) the parameter ALGORITHM is on */
        else if (page == PG_MIX) { song.mvol = (uint8_t)clampi(song.mvol + d, 0, 100); touch(); }   /* (the master) */
        else if (page == PG_ARR) { uint8_t pg = page; page = (uint8_t)(PG_ARR + 100u); knob(1u, d, lab, val); page = pg; }
        else {
            uint32_t q = (uint32_t)clampi((int32_t)(sq_play && sq_cue < NPAT ? sq_cue : song.cur) + d, 0, NPAT - 1);
            if (sq_play) sq_cue = (uint8_t)q;
            else song.cur = (uint8_t)q;
            if (cur_row >= plen()) cur_row = 0;
        }
    }
    for (i = 0; i < 4u; i++)
        if ((d = fm1_enc_take(ENC_KNOB1 + i)) != 0) {
            char lab[16], val[24];
            knob(i, d, lab, val);
        }
    /* the keys */
    for (k = 0; k < 27u; k++) {
        uint32_t m = 1u << k;
        int down = (edges & m) != 0, up = (prev_keys & m) && !(keys & m);
        int w = WHITE_OF_KEY[k], bl = BLACK_OF_KEY[k];
        int bpad = bl >= 0 && page == PG_TRK && !fx_on() && !jam_nav() && !(held & ((1u << BTN_HOME) | (1u << BTN_SEQ)));
        uint32_t pid = w >= 0 ? (uint32_t)w : 16u + (uint32_t)(bl >= 0 ? bl : 0);   /* (the pad's id: note repeat) */
        if (!down && !up) continue;
        if (bl >= 0 && !bpad) {
            if (!down) continue;
            if (held & (1u << BTN_HOME)) home_used = 1;
            if (fx_on()) {                      /* PERFORM: mute / solo, F1 which, F3 SIT, F4 all on */
                if (bl < (int)NT) mute_key((uint32_t)bl);
                else if (bl == (int)BLK_F1) { perf_solo ^= 1u; toast(perf_solo ? "BLACK KEYS SOLO" : "BLACK KEYS MUTE"); }
                else if (bl == (int)BLK_F1 + 2) sit_set(!sit_on);
                else if (bl == (int)BLK_F1 + 3) { for (i = 0; i < NT; i++) song.trk[i].mute = song.trk[i].solo = 0; toast("ALL TRACKS ON"); touch(); }
                continue;
            }
            if (bl < (int)NT && page == PG_ARR && !arr_live && arr_pos < song.nchain) {   /* (XENOISE) SEQ: the slot's track in, out */
                xu_chain_take();
                song.x.smute[arr_pos & 15u] ^= (uint8_t)(1u << bl);
                sel_trk = (uint8_t)bl;
                toast((song.x.smute[arr_pos & 15u] >> bl) & 1u ? "OUT OF THIS PART" : "IN THIS PART");
                touch();
                continue;
            }
            if (bl < (int)NT) { sel_trk = (uint8_t)bl; if (page == PG_SND) sub = 0; }
            else if (bl >= (int)BLK_F1 && !(page == PG_TRK && vis_on)) { if (page == PG_ARR) arr_fn((uint32_t)bl - BLK_F1); else fn_press((uint32_t)bl - BLK_F1); }
            continue;
        }
        if (w < 0 && !bpad) continue;
        if (up) {
            if (w >= 0 && (punch_keys & m) && !fx_latch) { punch_keys &= ~m; punch((uint32_t)w, 0); }
            if (pad_keys & m) {
                pad_keys &= ~m;
                if (!arp_latch && rpt_held == (0x80u | pid)) rpt_held = 0;
                if (!pad_keys && TRK_TYPE[t] == TY_SYNTH) ev_push(EV_OFF, t, 0, 0);
            }
            continue;
        }
        if (bpad) {                             /* (XENOISE) a black key on HOM: the drums' pad on its left, softer (a
                                                 * ghost); a synth's, a semitone over it (the passing note) */
            uint32_t wl = (uint32_t)WHITE_OF_KEY[k - 1u], n = pad_note(t, wl), vel = 7u;
            int rep = arp_on() && sq_play;
            if (TRK_TYPE[t] == TY_SYNTH) { n = n + 1u > 120u ? 120u : n + 1u; vel = 12u; }
            pad_keys |= m;
            last_note[t] = (uint8_t)n;
            if (rep && arp_latch && rpt_held == (0x80u | pid)) { rpt_held = 0; continue; }
            if (rep) { rpt_trk = (uint8_t)t; rpt_note = (uint8_t)n; rpt_held = (uint8_t)(0x80u | pid); }
            else ev_push(EV_ON, t, n, vel);
            if (rec_on && !rep) rec_note(t, n);
            continue;
        }
        if (fx_on()) {
            if (w >= (int)NPUNCH) continue;
            if (fx_latch && (punch_keys & m)) { punch_keys &= ~m; punch((uint32_t)w, 0); continue; }   /* latched: a tap toggles */
            punch_keys |= m;
            punch((uint32_t)w, 1);
            toast(PUNCH_NAME[w]);
            continue;
        }
        if (held & (1u << BTN_SEQ)) {
            seq_used = 1;
            if (w < (int)NPAT) {
                char buf[24], *e;
                if (sq_play) sq_cue = (uint8_t)w;
                else song.cur = (uint8_t)w;
                if (cur_row >= plen()) cur_row = 0;
                buf[0] = 0;
                e = str_cat(buf, sq_play ? "NEXT: " : "PATTERN ");
                e[0] = (char)('1' + w);
                e[1] = ' ';
                e[2] = 0;
                str_cat(e, SECT_NAME[w]);
                toast(buf);
            } else if (w == 8) { song.loop ^= 1u; toast(song.loop ? "LOOP THE PATTERN" : "PLAY THE SEQUENCE"); touch(); }
            continue;
        }
        if (page == PG_CHD) { chord_pad((uint32_t)w); continue; }
        if (page == PG_ARR && arr_live) {        /* (XENOISE) LIVE: the pattern next, looped till another */
            if ((uint32_t)w < live_n()) live_queue((uint32_t)w);
            continue;
        }
        if (page == PG_ARR) { arr_launch((uint32_t)w); continue; }
        {
            uint32_t n = pad_note(t, (uint32_t)w);
            int rep = arp_on() && sq_play;
            pad_keys |= m;
            last_note[t] = (uint8_t)n;
            if (rep && arp_latch && rpt_held == (0x80u | (uint32_t)w)) { rpt_held = 0; continue; }   /* latched: again stops */
            if (rep) { rpt_trk = (uint8_t)t; rpt_note = (uint8_t)n; rpt_held = (uint8_t)(0x80u | (uint32_t)w); }
            else ev_push(EV_ON, t, n, 12);
            if (rec_on && !rep) rec_note(t, n);
        }
    }
    prev_keys = keys;
}

/* ================================================================ the LEDs */
#if defined(DINGHY_HOST) || defined(KAWA_WASM)
static void leds_flush(void) { host_leds(led_want); }
#else
static void leds_flush(void)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++) {
        uint8_t on = 0, dim = 0;
        for (r = 1; r < 5u; r++) {
            int8_t id = FM1_KEYMAP[r][p];
            if (id < 0 || id > 40) continue;
            if (led_want[id] == 2u) on |= (uint8_t)(1u << r);
            else if (led_want[id] == 1u) dim |= (uint8_t)(1u << r);
        }
        fm1_led[p] = on;
        fm1_led_dim[p] = dim;
        fm1_led_breath[p] = fm1_led_hi[p] = fm1_led_mid[p] = 0;   /* (XENOISE: JIANT's LED kinds, unused here) */
    }
}
#endif
static void led(uint32_t id, uint32_t lvl) { if (id < 41u) led_want[id] = (uint8_t)lvl; }
static const char *perf_fn(uint32_t i);
static const char *arr_fn_label(uint32_t i);
static uint32_t f_led(uint32_t f)               /* F1..F4 (black keys 8..11): off without a job, lit with one, blinking */
{                                               /* when it is the one to press */
    char fb[16];
    const char *l = fx_on() ? perf_fn(f) : page == PG_ARR ? arr_fn_label(f) : fn_label(f, fb);
    uint32_t slow = (fm1_ms / 350u) & 1u, fast = (fm1_ms / 110u) & 1u;
    if (!l[0]) return 0;
    if (page == PG_SNG && arm_f == f) return fast ? 2u : 0u;       /* SURE?: press it again */
    if (fm1_ms - page_ms < 1600u) return slow ? 2u : 1u;           /* a page opened: here are its keys */
    if (page == PG_SNG && arm_f == 0xFF && f == 1u) return slow ? 2u : 1u;   /* SNG: NEWSONG */
    return 1u;
}
static void ui_leds(void)
{
    uint32_t i, held = fm1_in.buttons, t = trk(), perf = fx_on();
    for (i = 0; i < 41u; i++) led_want[i] = 0;
    for (i = 0; i < PG_N; i++) led(PG_BTN[i], tab_of(page) == i ? 2u : 1u);
    led(BTN_PLAY, sq_play ? ((sq_row & 3u) == 0u ? 2u : 1u) : 0u);
    led(BTN_REC, rec_on ? 2u : 0u);
    led(BTN_FX, perf ? 2u : 1u);
    led(BTN_SEQ, held & (1u << BTN_SEQ) || page == PG_ARR ? 2u : 1u);
    led(BTN_ARP, page == PG_ARP || (song.x.arp[trk()].set && song.x.arp[trk()].mode) ? 2u : 1u);
    led(BTN_HOME, page == PG_TRK || sit_on ? 2u : 1u);
    led(BTN_OCT_DOWN, pad_oct[t] < 0 ? 2u : 1u);
    led(BTN_OCT_UP, pad_oct[t] > 0 ? 2u : 1u);
    for (i = 0; i < 11u; i++) {                 /* black: the tracks (PERFORM: on / muted), F1..F4 */
        uint32_t lv = i >= BLK_F1 ? f_led(i - BLK_F1) : i >= NT ? 0u : perf ? (song.trk[i].mute ? 0u : (song.trk[i].solo ? 2u : 1u)) :
                      i == t ? 2u : song.trk[i].mute ? 0u : 1u;
        led(KEY_LED(KEY_OF_BLACK[i]), lv);
    }
    for (i = 0; i < 16u; i++) {
        uint32_t lv = (fm1_in.notes >> KEY_OF_WHITE[i]) & 1u ? 2u : 0u;
        if (perf) lv = i < NPUNCH ? ((punch_keys >> KEY_OF_WHITE[i]) & 1u ? 2u : 1u) : 0u;
        else if (held & (1u << BTN_SEQ))
            lv = i < NPAT ? (i == (sq_play ? sq_pi : song.cur) ? 2u : 1u) : i == 8u ? (song.loop ? 2u : 1u) : 0u;
        else if (page == PG_ARR)                /* the parts: the one waiting for the bar line blinks, the one playing lit */
            lv = i >= song.nchain ? 0u : i == sq_jump ? ((fm1_ms / 110u) & 1u ? 2u : 0u) :
                 (sq_play ? i == sq_chainpos && !song.loop : i == sq_start) ? 2u : 1u;
        else if (page == PG_CHD) {
            int32_t pd = sq_play ? prog_deg(sq_row) : song.deg[chd_slot < song.ndeg ? chd_slot : 0u];
            int32_t n7 = ((pd % 7) + 7) % 7;
            if (i < 7u) lv = (int32_t)i == n7 ? 2u : 1u;
            else if (i == 7u) lv = 1u;
            else lv = i - 8u == chd_slot ? 2u : i - 8u < song.ndeg ? 1u : 0u;
        } else if ((rpt_held & 0x80u) && (rpt_held & 15u) == i) lv = 2u;
        else if (!lv) lv = sq_play && (sq_row % 16u) == i ? 1u : 0u;
        led(KEY_LED(KEY_OF_WHITE[i]), lv);
    }
    leds_flush();
}

/* ================================================================ drawing: flat and plain, no pictures */
#define BAR_H 20                                /* the top bar, then the pages' tabs */
#define TAB_Y 20
#define TOP_H 35
#define HEAD_Y 35
#define GRID_Y 51
#define ROW_H 14                                /* (12 px of text, a pixel of air above and below) */
#define NVIS 9
#define GUT_W 18
#define FOC_W 72
#define NAR_W 30
#define BODY_H (INFO_Y - TOP_H)
#define INFO_Y 178
#define KNOB_Y 194
static int32_t lane_x(uint32_t t)
{
    int32_t x = GUT_W;
    uint32_t k;
    for (k = 0; k < t; k++) x += k == trk() ? FOC_W : NAR_W;
    return x;
}
static int32_t lane_w(uint32_t t) { return t == trk() ? FOC_W : NAR_W; }
static void glyph_play(int32_t x, int32_t y, uint16_t c)   /* a triangle, 9 high */
{
    int32_t k;
    for (k = 0; k < 5; k++) g_rect(x + k * 2, y + k, 2, 9 - 2 * k, c);
}
static void cell_text(char *o, const cell_t *c, uint32_t t)   /* "C-4 C R3" (a note, its velocity, its effect) */
{
    static const char H[] = "0123456789ABCDEF";
    static const char *const DK[4] = {"SNR", "HHC", "HHO", "CLP"};
    if (c->n == NOTE_OFF) { o[0] = o[1] = o[2] = '='; o[3] = 0; return; }
    if (TRK_TYPE[t] == TY_BREAK) { o[0] = 'S'; o[1] = (char)('0' + c->n / 10u); o[2] = (char)('0' + c->n % 10u); }
    else if (TRK_TYPE[t] == TY_DRUMS) { o[0] = DK[drm_kind(c->n)][0]; o[1] = DK[drm_kind(c->n)][1]; o[2] = DK[drm_kind(c->n)][2]; }
    else if (TRK_TYPE[t] == TY_KICK) { o[0] = 'K'; o[1] = 'I'; o[2] = 'K'; }
    else note_name(c->n, o);
    o[3] = ' ';
    o[4] = H[c->v & 15u];
    o[5] = ' ';
    o[6] = c->fx ? FX_CH[c->fx % FX_N] : '.';
    o[7] = c->fx ? H[c->fv & 15u] : '.';
    o[8] = 0;
}
static void inset(int32_t x, int32_t y, int32_t w, int32_t h) { g_rect(x, y, w, h, C_INSET); }
/* ---- the blueprints: faint line drawings behind the pages (a mech's parts, the way an engineer's sheet draws them).
 * Lines and circles only, drawn into the band being painted: nothing stored, nothing sent unless it changes. */
#define C_BP (TH->bp)                           /* the drawing */
#define C_BP2 (TH->bp2)                         /* what it points at */
#define C_BPDOT (TH->bpdot)                     /* the sheet's dots */
static void bp_circle(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t x = r, y = 0, e = 1 - r;
    if (!g_vis(cy - r, 2 * r + 1)) return;
    while (x >= y) {
        g_px(cx + x, cy + y, c); g_px(cx - x, cy + y, c); g_px(cx + x, cy - y, c); g_px(cx - x, cy - y, c);
        g_px(cx + y, cy + x, c); g_px(cx - y, cy + x, c); g_px(cx + y, cy - x, c); g_px(cx - y, cy - x, c);
        y++;
        if (e < 0) e += 2 * y + 1;
        else { x--; e += 2 * (y - x) + 1; }
    }
}
static void bp_dash(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* 4 on, 3 off */
{
    int32_t n = (x1 - x0 < 0 ? x0 - x1 : x1 - x0) > (y1 - y0 < 0 ? y0 - y1 : y1 - y0) ? (x1 - x0 < 0 ? x0 - x1 : x1 - x0) : (y1 - y0 < 0 ? y0 - y1 : y1 - y0), k;
    if (!n) return;
    for (k = 0; k < n; k += 7) {
        int32_t k1 = k + 4 < n ? k + 4 : n;
        g_line(x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n, x0 + (x1 - x0) * k1 / n, y0 + (y1 - y0) * k1 / n, c);
    }
}
static void bp_ray(int32_t cx, int32_t cy, uint32_t ang, int32_t r0, int32_t r1, uint16_t c)   /* ang: 0..65535 a turn */
{
    int32_t co = sine(((uint32_t)ang << 16) + 0x40000000u), si = sine((uint32_t)ang << 16);
    g_line(cx + (co * r0 >> 15), cy + (si * r0 >> 15), cx + (co * r1 >> 15), cy + (si * r1 >> 15), c);
}
static void bp_gear(int32_t cx, int32_t cy, int32_t r, uint32_t teeth, uint32_t turn, uint16_t c)
{
    uint32_t k;
    if (!g_vis(cy - r - 4, 2 * r + 9)) return;
    bp_circle(cx, cy, r, c);
    bp_circle(cx, cy, r * 6 / 10, c);
    bp_circle(cx, cy, r / 5 + 1, C_BP2);
    for (k = 0; k < teeth; k++) bp_ray(cx, cy, turn + k * 65536u / teeth, r, r + 4, c);
    for (k = 0; k < 4u; k++) bp_ray(cx, cy, turn + k * 16384u + 8192u, r / 5 + 1, r * 6 / 10, c);
}
static void bp_dim(int32_t x0, int32_t x1, int32_t y, uint16_t c)   /* a dimension line: ticks and arrows */
{
    g_rect(x0, y, x1 - x0, 1, c);
    g_rect(x0, y - 3, 1, 7, c); g_rect(x1, y - 3, 1, 7, c);
    g_line(x0, y, x0 + 3, y - 2, c); g_line(x0, y, x0 + 3, y + 2, c);
    g_line(x1, y, x1 - 3, y - 2, c); g_line(x1, y, x1 - 3, y + 2, c);
}
static void bp_cross(int32_t x, int32_t y, int32_t r, uint16_t c)
{
    g_rect(x - r, y, 2 * r + 1, 1, c);
    g_rect(x, y - r, 1, 2 * r + 1, c);
}
static void bp_sheet(int32_t y0, int32_t h)     /* the paper: a dot every 12 px */
{
    int32_t x, y;
    for (y = y0 + 6; y < y0 + h; y += 12)
        if (g_vis(y, 1)) for (x = 6; x < 240; x += 12) g_px(x, y, C_BPDOT);
}
static uint32_t bp_turn(void) { return sq_play ? (sq_rowtick & 15u) * 4096u : 0u; }   /* the gears move a notch a row */
/* one drawing per page */
static void bp_song(void)                       /* a frame's joint: two gears in mesh, its axis and its sizes */
{
    bp_gear(176, 150, 34, 14, bp_turn(), C_BP);
    bp_gear(122, 168, 18, 8, 65536u - bp_turn() * 2u, C_BP);
    bp_dash(176, 100, 176, 196, C_BP);
    bp_dash(110, 150, 230, 150, C_BP);
    bp_dim(142, 210, 106, C_BP);
    bp_cross(176, 150, 6, C_BP2);
}
static void bp_sound(void)                      /* two operators and the path between them */
{
    int32_t y = TOP_H + 120;
    g_frame(150, y - 10, 34, 20, C_BP);
    g_frame(200, y - 10, 34, 20, C_BP);
    bp_dash(184, y, 200, y, C_BP2);
    g_line(196, y - 3, 200, y, C_BP2); g_line(196, y + 3, 200, y, C_BP2);
    bp_circle(167, y, 7, C_BP); bp_circle(217, y, 7, C_BP);
    bp_dim(150, 234, y + 15, C_BP);
}
static void bp_chords(void)                     /* the circle of fifths, behind the chords */
{
    uint32_t k;
    bp_circle(212, TOP_H + 118, 12, C_BP);
    bp_circle(212, TOP_H + 118, 18, C_BP);
    for (k = 0; k < 12u; k++) bp_ray(212, TOP_H + 118, k * 65536u / 12u, 18, 22, k == (uint32_t)(song.key * 7u % 12u) ? C_BP2 : C_BP);
}
static void bp_arr(void)                        /* the timeline's rails */
{
    int32_t k;
    for (k = 0; k < 4; k++) bp_dash(0, TOP_H + 47 + k * 31, 240, TOP_H + 47 + k * 31, C_BP);
}
static void bp_perform(void)                    /* a reticle */
{
    bp_circle(214, TOP_H + 111, 6, C_BP2);
    bp_cross(214, TOP_H + 111, 10, C_BP);
    bp_dim(150, 202, TOP_H + 111, C_BP);
}

static void draw_top(void)
{
    char s[40];
    uint32_t pc = sq_play ? sq_pi : song.cur;
    g_rect(0, 0, 240, BAR_H, C_PANEL);
    {                                           /* the pages: the one open in orange */
        uint32_t k;
        g_rect(0, TAB_Y, 240, TOP_H - TAB_Y, C_BG);
        for (k = 0; k < PG_N; k++) {
            int on = k == tab_of(page) && !fx_on();
            g_rect((int32_t)k * 30, TAB_Y + 1, 29, 13, on ? C_ORANGE : C_ROW);
            g_text_c((int32_t)k * 30 + 15, TAB_Y + 1, PG_NAME[k], on ? C_BG : C_MUTED, T_CROP | (on ? T_BOLD : 0));
        }
    }
    inset(3, 3, 30, 14);
    g_text_c(18, 4, fx_on() ? "FX" : PG_NAME[page], fx_on() ? C_REDC : C_ORANGE, T_CROP | T_BOLD);
    s[0] = 0;
    str_cat(s, song.name);
    s[song.x.arp[trk()].set && song.x.arp[trk()].mode ? 10 : 13] = 0;
    g_text(39, 4, s, C_TXT, T_CROP);
    if (song.x.arp[trk()].set && song.x.arp[trk()].mode) {   /* (XENOISE) the track's arp on: AR, as FX shows FX */
        inset(122, 3, 21, 14);
        g_text_c(133, 4, "AR", TH->arp, T_CROP | T_BOLD);
    }
    inset(146, 3, 32, 14);
    s[0] = (char)('1' + pc % NPAT);
    s[1] = 0;
    if (sq_play && sq_cue < NPAT) { s[1] = '>'; s[2] = (char)('1' + sq_cue); s[3] = 0; }
    g_text(150, 4, s, C_ORANGE, T_CROP | T_BOLD);
    inset(181, 3, 30, 14);
    fmt_u(s, song.bpm, 3);
    g_text(184, 4, s, C_TXT, T_CROP);
    if (sq_play) glyph_play(217, 6, C_ORANGE);
    else g_rect(217, 6, 8, 8, C_MUTED);
    if (rec_on) g_rrect(229, 6, 8, 8, C_REDC);
    else if (sit_on) g_text(229, 4, "S", C_CYAN, T_CROP | T_BOLD);
    else if (song.loop) g_text(229, 4, "L", C_MUTED, T_CROP | T_BOLD);
}
static void draw_heads(void)                     /* solid colours: the track picked full, the others softer, a hit lights it */
{
    uint32_t t, sel = trk();
    char s[16];
    g_rect(0, HEAD_Y, 240, GRID_Y - HEAD_Y, C_BG);
    for (t = 0; t < NT; t++) {
        int32_t x = lane_x(t), w = lane_w(t);
        int fl = 0, off = song.trk[t].mute;            /* (XENOISE: no flash on a hit, for now) */
        uint16_t c = off ? C_INSET : t == sel ? TRK_COL[t] : g_mix(C_BG, TRK_COL[t], 110), ink;
        if (fl && !off) c = g_mix(TRK_COL[t], RGB(255, 255, 255), 150);
        ink = off ? C_MUTED : (t == sel || fl) ? C_BG : g_mix(TRK_COL[t], RGB(255, 255, 255), 170);   /* dark on bright, light on dim */
        g_rect(x, HEAD_Y + 1, w - 2, 14, c);
        s[0] = 0;
        str_cat(s, TRK_ID[t]);
        if (t == sel && song.trk[t].solo) str_cat(s, " SOLO");
        else if (t == sel && off) str_cat(s, " OFF");
        g_text(x + 3, HEAD_Y + 2, s, ink, T_CROP | (t == sel ? T_BOLD : 0));
    }
}
static void draw_grid(void)
{
    pat_t *p = &song.pat[(sq_play ? sq_pi : song.cur) % NPAT];
    uint32_t L = p->len ? p->len : 16u, t, j, sel = trk(), B = meter_steps();
    int32_t center = sq_play ? sq_row : cur_row;
    char s[16];
    draw_heads();
    int32_t nvis = page == PG_TRK && !hom_edit && !fx_on() ? 12 : NVIS;   /* (XENOISE: HOM plain: three rows more) */
    for (j = 0; j < (uint32_t)nvis; j++) {
        int32_t top = clampi(center - nvis / 2, 0, (int32_t)L > nvis ? (int32_t)L - nvis : 0);   /* (no blank rows above 00) */
        int32_t r = top + (int32_t)j, y = GRID_Y + (int32_t)j * ROW_H;
        int play = sq_play && r == sq_row, cur = r == cur_row, skip = B < 16u && (uint32_t)(r & 15) >= B;
        if (!g_vis(y, ROW_H)) continue;
        if (r < 0 || r >= (int32_t)L) { g_rect(0, y, 240, ROW_H, C_BG); continue; }
        g_rect(0, y, 240, ROW_H, play ? C_ORANGE : skip ? C_BG : (r & 3) == 0 ? C_BEAT : C_ROW);
        if (!play && !skip) g_rect(lane_x(sel), y, FOC_W - 2, ROW_H, g_mix((r & 3) == 0 ? C_BEAT : C_ROW, C_TXT, 12));
        s[0] = 0;
        fmt_u(s, (uint32_t)r, 2);                /* (XENOISE: the rows in decimal, 00 .. 63; WND-1: hex) */
        g_text(2, y + 1, s, play ? C_BG : cur ? C_BLUE : (r & 3) == 0 ? C_TXT : C_MUTED, T_CROP);
        for (t = 0; t < NT; t++) {
            const cell_t *c = &p->c[t][r];
            int32_t x = lane_x(t);
            if (!c->n) { g_text(t == sel ? x + 3 : x + 11, y + 1, t == sel ? "-" : ".", play ? C_BG : C_FAINT, T_CROP); continue; }
            cell_text(s, c, t);
            if (t == sel) {
                char a[4] = {s[0], s[1], s[2], 0};
                uint16_t tc = play ? C_BG : skip ? C_FAINT : TRK_COL[t];   /* (XENOISE: the track's colour) */
                g_text(x + 3, y + 1, a, c->n == NOTE_OFF ? C_MUTED : tc, T_CROP | (play ? T_BOLD : 0));
                if (c->n != NOTE_OFF) {
                    char v[4];
                    v[0] = 0;
                    fmt_u(v, c->v & 15u, 2);   /* (XENOISE: the velocity in decimal, 00 .. 15) */
                    g_text(x + 32, y + 1, v, play ? C_BG : g_mix(C_BG, TRK_COL[t], 190), T_CROP);
                    if (c->fx) g_text(x + 51, y + 1, s + 6, play ? C_BG : TRK_COL[t], T_CROP);
                }
            } else {                            /* (XENOISE) narrow: Renoise's note-octave (C-4, F#2), the break's slice (S01),
                                                 * the drum (KIK SNR HHC HHO CLP), === a note off */
                cell_text(s, c, t);
                s[3] = 0;
                g_text(x + 3, y + 1, s, c->n == NOTE_OFF ? C_MUTED : song.trk[t].mute && !play ? C_FAINT : play ? C_BG : TRK_COL[t], T_CROP);
            }
        }
        if (cur && !play) g_frame(lane_x(sel), y, FOC_W - 2, ROW_H, C_BLUE);
    }
}
static void draw_perform(void)                  /* FX: the FM-1's keyboard, each punch-in wired to its white key, the tracks */
{                                               /* on the black ones */
    static const char *const SHORT[NPUNCH] = {"R4", "R8", "R16", "R32", "REV", "HLF", "SCT", "DUB", "WSH", "LPF", "3/4", "7/8", "5/4"};
    const int32_t KY = TOP_H + 52, KW = 15, WH = 42, BH = 25;
    uint32_t k;
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    g_text(6, TOP_H + 3, "PERFORM", C_ORANGE, T_CROP | T_BOLD);
    g_text(70, TOP_H + 3, "FX AGAIN: CLOSE", C_MUTED, T_CROP);
    for (k = 0; k < 16u; k++) {                 /* the white keys, and the call-outs over them */
        int32_t x = (int32_t)k * KW, cx = x + KW / 2;
        int on = k < NPUNCH && ((punch_keys >> KEY_OF_WHITE[k]) & 1u), down = (fm1_in.notes >> KEY_OF_WHITE[k]) & 1u;
        g_rect(x + 1, KY, KW - 2, WH, k < NPUNCH ? (on ? TH->wkey_on : TH->wkey) : TH->wkey_off);
        if (on || down) { g_frame(x + 1, KY, KW - 2, WH, C_BLUE); g_frame(x + 2, KY + 1, KW - 4, WH - 2, C_BLUE); }
        if (k < NPUNCH) {
            int32_t ly = TOP_H + ((k & 1u) ? 32 : 18);
            g_rect(cx, ly + 13, 1, KY - ly - 13, on ? C_BLUE : C_BP2);   /* the wire */
            int32_t bx = clampi(cx - 13, 1, 239 - 27);   /* (the label stays on the screen) */
            g_rect(bx, ly, 27, 13, on ? C_ORANGE : C_PANEL);
            g_text_c(bx + 14, ly, SHORT[k], on ? C_BG : C_TXT, T_CROP);
            if (fx_row == 0u && k == fx_col) { g_frame(bx - 1, ly - 1, 29, 15, C_TXT); g_frame(x + 1, KY, KW - 2, WH, C_TXT); }   /* (the PILOT's) */
        }
    }
    {                                           /* the black keys: the tracks (their colour, dark when muted), F1..F4 */
        uint32_t w, b = 0;
        for (w = 0; w + 1u < 16u; w++) {
            uint32_t n = (KEY0 + KEY_OF_WHITE[w]) % 12u;   /* a black key follows F, G, A, C and D */
            int32_t x = (int32_t)(w + 1u) * KW - 5;
            uint16_t c;
            if (!(n == 5u || n == 7u || n == 9u || n == 0u || n == 2u)) continue;
            c = b < NT ? (song.trk[b].mute ? C_INSET : song.trk[b].solo ? C_BLUE : TRK_COL[b]) : b >= BLK_F1 ? C_PANEL : TH->bkey_off;
            g_rect(x, KY, 10, BH, TH->bkey_case);
            g_rect(x + 1, KY, 8, BH - 1, c);
            if (b < NT && fm1_ms - flash_ms[b] < 90u && !song.trk[b].mute) g_rect(x + 2, KY + BH - 6, 6, 4, RGB(255, 255, 255));
            b++;
        }
    }
    g_text(6, TOP_H + 98, perf_solo ? "BLACK 1-6: SOLO" : "BLACK 1-6: MUTE", C_MUTED, T_CROP);
    for (k = 0; k < NT; k++) {
        int32_t x = 3 + (int32_t)k * 39, y = TOP_H + 113;
        const track_t *tr = &song.trk[k];
        int fl = fm1_ms - flash_ms[k] < 90u;
        g_rect(x, y, 36, 20, tr->solo ? C_BLUE : tr->mute ? C_INSET : C_PANEL);
        g_rect(x, y, 36, 3, tr->mute ? C_FAINT : fl ? C_TXT : TRK_COL[k]);
        g_text_c(x + 18, y + 5, TRK_ID[k], tr->mute ? C_FAINT : C_TXT, T_CROP);
        if (fx_row == 1u && k == fx_col) g_frame(x - 1, y - 1, 38, 22, C_TXT);   /* (the PILOT's) */
    }
}
static void draw_arr(void)
{
    uint32_t k, n = song.nchain;
    char s[24], *e;
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    bp_arr();
    g_text(6, TOP_H + 3, "SEQUENCE", C_ORANGE, T_CROP | T_BOLD);
    s[0] = 0;
    if (song.loop) { e = str_cat(s, "LOOP "); e[0] = (char)('1' + song.cur % NPAT); e[1] = 0; }
    else { e = fmt_u(s, n, 0); str_cat(e, " PARTS"); }
    g_text(234 - g_text_w(s), TOP_H + 3, s, song.loop ? C_CYAN : C_MUTED, T_CROP);
    for (k = 0; k < 16u; k++) {
        int32_t x = 3 + (int32_t)(k % 4u) * 59, y = TOP_H + 18 + (int32_t)(k / 4u) * 31;
        int on = k < n, play = on && sq_play && !song.loop && k == sq_chainpos, cur = k == arr_pos, wait = on && k == sq_jump;
        if (on) {
            uint32_t pt = song.chain[k] % NPAT;
            g_rect(x, y, 56, 28, play ? C_ORANGE : C_PANEL);
            s[0] = 0;
            fmt_u(s, k + 1u, 2);
            g_text(x + 3, y + 2, s, play ? C_BG : C_MUTED, T_CROP);
            s[0] = 'P';                         /* (XENOISE: the pattern, P1..P8, right of the part's number; its part's */
            s[1] = (char)('1' + pt);            /* name below, the tile's width) */
            s[2] = 0;
            g_text(x + 36, y + 2, s, play ? C_BG : C_ORANGE, T_CROP | T_BOLD);
            g_text(x + 3, y + 14, SECT_NAME[pt], play ? C_BG : C_TXT, T_CROP);
            {                                   /* (XENOISE) its tracks: lit in it, dark out of it (black keys 1-6) */
                uint32_t q;
                for (q = 0; q < NT; q++) {
                    int out = (song.x.smute[k] >> q) & 1u;
                    g_rect(x + 3 + (int32_t)q * 9, y + 25, 7, 2, out ? (play ? g_mix(C_ORANGE, C_BG, 160) : C_INSET) : TRK_COL[q]);
                }
            }
        } else {
            g_rect(x, y, 56, 28, C_ROW);
            if (k == n) g_text_c(x + 28, y + 8, "+", C_MUTED, T_CROP);
        }
        if (wait && ((fm1_ms / 110u) & 1u)) g_rect(x, y, 56, 28, C_ORANGE), g_text(x + 3, y + 14, "NEXT", C_BG, T_CROP | T_BOLD);
        else if (cur) { g_frame(x, y, 56, 28, C_BLUE); g_frame(x + 1, y + 1, 54, 26, C_BLUE); }
        if (on && !sq_play && k == sq_start && n > 1u) g_text(x + 22, y + 2, ">", C_ORANGE, T_CROP | T_BOLD);
    }
}
/* (XENOISE) SAV: an interfacing room. Above, the genre picked as a hologram (its ring turning, its blurb running by);
 * below, the data bank: a bay a slot, its light blinking as it holds a song, the one picked swept by a scan; at the
 * side the signal (the output's scope) and the memory. A question waits in a box over the bank (OCT+ yes, OCT- no);
 * what the store did plays out a moment (WRITING, READING, PURGED, NEW SONG) */
static void sng_ring(int32_t cx, int32_t cy, int32_t r, uint32_t rot, uint16_t c)
{
    uint32_t k;
    bp_circle(cx, cy, r, g_mix(C_BG, c, 90));
    bp_circle(cx, cy, r - 6, g_mix(C_BG, c, 60));
    for (k = 0; k < 6u; k++) {                  /* the ticks, turning */
        uint32_t a = rot + k * 10923u;
        int32_t s = xj_sin(a), co = xj_sin(a + 16384u);
        g_line(cx + (r - 3) * co / 32768, cy + (r - 3) * s / 32768, cx + (r + 3) * co / 32768, cy + (r + 3) * s / 32768, c);
    }
}
static void gen_holo(void)                      /* (XENOISE) GEN's hologram: the genre picked (SAV's, before) */
{
    const wb_genre_t *g = &WB_GENRES[genre_pick % WB_NGENRE];
    char s[48], *e;
    uint32_t now = fm1_ms;
    int32_t x, tw;
    int empty = genre_pick >= WB_NGENRE;
    const char *b = empty ? "A BLANK PAGE ON THE DEFAULT KIT" : g->blurb, *gname = empty ? "EMPTY SONG" : g->name;
    uint16_t holo = C_BLUE, warm = C_CYAN, hot = C_ORANGE;
    g_rect(0, TOP_H, 240, 46, g_mix(C_BG, holo, 18));
    tw = g_text_w(b);
    x = 52 - (int32_t)(now / 35u % (uint32_t)(tw + 140));
    g_text(x + 140, TOP_H + 32, b, g_mix(C_BG, C_TXT, 190), T_CROP);   /* the blurb, running by */
    g_rect(0, TOP_H, 50, 46, g_mix(C_BG, holo, 18));
    sng_ring(25, TOP_H + 23, 17, now * 40u, holo);
    g_rect(23 + (int32_t)(xj_sin(now * 70u) > 0), TOP_H + 21, 4, 4, (now / 250u) & 1u ? hot : warm);
    s[0] = 0;
    e = fmt_u(str_cat(s, "GENRE "), genre_pick + 1u, 2);
    *e++ = '/';
    fmt_u(e, WB_NGENRE + 1u, 0);
    g_text(54, TOP_H + 3, s, g_mix(C_BG, holo, 170), T_CROP);
    s[0] = 0;
    if (!empty) {
        e = fmt_u(s, g->bpm[0], 0);
        *e++ = '-';
        e = fmt_u(e, g->bpm[1], 0);
        str_cat(e, " BPM");
    }
    g_text(236 - g_text_w(s), TOP_H + 3, s, warm, T_CROP);
    g_text(54, TOP_H + 16, gname, holo, T_BOLD);
    g_rect(0, TOP_H + 45, 240, 1, g_mix(C_BG, holo, 120));
    g_rect((int32_t)(now / 5u % 280u) - 20, TOP_H + 45, 20, 1, C_TXT);   /* a pulse along it */
}
static void draw_sng(void)
{
    char s[96], *e;
    uint32_t k, now = fm1_ms, used = 0;
    int32_t y, x, tw;
    uint16_t holo = C_BLUE, warm = C_CYAN, hot = C_ORANGE;
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    {                                           /* the room: the floor's lines to the back wall, its seams coming on */
        int32_t vy = TOP_H + 52;
        for (k = 0; k < 9u; k++)
            g_line(120 + ((int32_t)k - 4) * 14, vy, 120 + ((int32_t)k - 4) * 70, INFO_Y, g_mix(C_BG, holo, 26));
        for (k = 0; k < 5u; k++) {
            uint32_t f = (k * 32u + now / 30u) % 160u;
            g_rect(0, vy + (int32_t)(f * f / 270u), 240, 1, g_mix(C_BG, holo, 20 + f / 6u));
        }
    }
    {                                           /* (XENOISE) the hologram: the song in memory, its facts running by */
        uint32_t rows = 0, sec;
        for (k = 0; k < NSLOT; k++) used += k && slot_ok[k];
        for (k = 0; k < song.nchain && k < 16u; k++) rows += song.pat[song.chain[k] % NPAT].len ? song.pat[song.chain[k] % NPAT].len : 16u;
        sec = rows * 15u / (song.bpm ? song.bpm : 120u);
        s[0] = 0;
        e = str_cat(str_cat(str_cat(s, NOTE_NM[song.key % 12u]), " "), WB_SCALE_NAME[song.scale % WB_NSCALE]);
        e = fmt_u(str_cat(e, "   "), song.bpm, 0);
        e = fmt_u(str_cat(e, " BPM   "), song.nchain, 0);
        e = str_cat(e, " PARTS   ");
        e = fmt_u(e, sec / 60u, 0);
        *e++ = ':';
        e = fmt_u(e, sec % 60u, 2);
        e = str_cat(str_cat(e, "   "), song.genre < WB_NGENRE ? WB_GENRES[song.genre].name : "");
        if (sng_clip != 0xFF) { e = fmt_u(str_cat(e, "   COPIED "), sng_clip, 2); e = str_cat(str_cat(e, " "), slot_name[sng_clip]); }
        g_rect(0, TOP_H, 240, 46, g_mix(C_BG, holo, 18));
        tw = g_text_w(s);
        x = 52 - (int32_t)(now / 35u % (uint32_t)(tw + 140));
        g_text(x + 140, TOP_H + 32, s, g_mix(C_BG, C_TXT, 190), T_CROP);
        g_rect(0, TOP_H, 50, 46, g_mix(C_BG, holo, 18));
        sng_ring(25, TOP_H + 23, 17, now * 40u, holo);
        g_rect(23 + (int32_t)(xj_sin(now * 70u) > 0), TOP_H + 21, 4, 4, (now / 250u) & 1u ? hot : warm);
        g_text(54, TOP_H + 3, sub & 1u ? "MEMORY 2/2" : "MEMORY 1/2", g_mix(C_BG, holo, 170), T_CROP);
        s[0] = 0;
        e = fmt_u(s, used, 0);
        e = fmt_u(str_cat(e, "/"), NSLOT - 1u, 0);
        str_cat(e, " SLOTS");
        g_text(236 - g_text_w(s), TOP_H + 3, s, warm, T_CROP);
        g_text(54, TOP_H + 16, song.name, holo, T_BOLD);
        g_rect(0, TOP_H + 45, 240, 1, g_mix(C_BG, holo, 120));
        g_rect((int32_t)(now / 5u % 280u) - 20, TOP_H + 45, 20, 1, C_TXT);
    }
    /* the data bank: six bays round the slot picked */
    y = TOP_H + 50;
    for (k = 1; k < NSLOT; k++) used += slot_ok[k] != 0;
    for (k = 0; k < 6u; k++) {
        int32_t sl = clampi((int32_t)slot_sel - 2, 1, (int32_t)NSLOT - 6) + (int32_t)k, yy = y + (int32_t)k * 15;
        int sel = sl == (int32_t)slot_sel, on = slot_ok[sl] != 0, cur = on && !memcmp(slot_name[sl], song.name, 15);
        uint16_t led = !on ? C_FAINT : ((now / 180u + (uint32_t)sl * 5u) % 9u) ? warm : C_TXT;
        g_rect(4, yy, 152, 13, sel ? g_mix(C_BG, holo, 60) : g_mix(C_BG, C_PANEL, 140));
        if (sel) {                              /* the scan over the picked one, its brackets breathing */
            int32_t sx = 4 + (int32_t)(now / 4u % 170u) - 18;
            g_blend(sx < 4 ? 4 : sx, yy, sx < 4 ? 18 + sx - 4 : sx + 18 > 156 ? 156 - sx : 18, 13, holo, 90);
            g_rect(1 - (int32_t)((now / 200u) & 1u), yy, 2, 13, holo);
            g_rect(157 + (int32_t)((now / 200u) & 1u), yy, 2, 13, holo);
        }
        g_rect(8, yy + 4, 5, 5, led);
        s[0] = 0;
        fmt_u(s, (uint32_t)sl, 2);
        g_text(17, yy, s, sel ? C_TXT : C_MUTED, T_CROP);
        g_text(37, yy, on ? slot_name[sl] : "-- EMPTY --", on ? (sel ? C_TXT : g_mix(C_BG, C_TXT, 200)) : C_FAINT, T_CROP);
        if (cur) g_rect(148, yy + 4, 5, 5, hot);
    }
    {                                           /* the side: the signal, the memory */
        int32_t sx0 = 164, sy0 = y, sw = 72, sh = 42, k2;
        g_rect(sx0, sy0, sw, sh, g_mix(C_BG, C_PANEL, 120));
        for (k2 = 0; k2 < sw - 2; k2 += 2) {
            int32_t a = scope[(scope_w + (uint32_t)k2 * 120u / (uint32_t)sw) % 120u];
            g_rect(sx0 + 1 + k2, sy0 + sh / 2 + clampi(a / 900, -sh / 2 + 1, sh / 2 - 2), 2, 1, holo);
        }
        g_text(sx0 + 2, sy0 + 1, "SIGNAL", g_mix(C_BG, holo, 140), T_CROP);
        sy0 += sh + 4;
        g_rect(sx0, sy0, sw, 40, g_mix(C_BG, C_PANEL, 120));
        g_text(sx0 + 2, sy0 + 1, "MEMORY", g_mix(C_BG, holo, 140), T_CROP);
        for (k2 = 0; k2 < (int32_t)NSLOT - 1; k2++) {
            int lit = k2 < (int32_t)used;
            g_rect(sx0 + 3 + (k2 % 7) * 10, sy0 + 15 + (k2 / 7) * 11, 8, 8,
                   lit ? ((now / 120u + (uint32_t)k2) % 11u ? warm : C_TXT) : g_mix(C_BG, holo, 40));
        }
    }
    if (sng_fx && fm1_ms - sng_fx_ms < 900u) {  /* what the store just did, playing out */
        static const char *const W[8] = {"", "WRITING", "READING", "PURGED", "NEW SONG", "COPIED", "SHIFTING ON", "SHIFTING BACK"};
        int32_t f = (int32_t)(fm1_ms - sng_fx_ms) * 140 / 700;
        g_rect(20, TOP_H + 78, 200, 32, C_BG);
        g_frame(20, TOP_H + 78, 200, 32, holo);
        g_text(28, TOP_H + 81, W[sng_fx % 8u], sng_fx == 3u ? hot : holo, T_BOLD);
        g_rect(28, TOP_H + 98, f > 140 ? 140 : f, 6, sng_fx == 3u ? hot : warm);
        g_text(176, TOP_H + 95, f >= 140 ? "DONE" : "", C_TXT, T_CROP);
    } else if (arm_f != 0xFF) {                 /* the question */
        static const char *const Q[8] = {"SAVE TO SLOT", "GENERATE A SONG", "LOAD FROM SLOT", "DELETE SLOT", "COPY SLOT", "PASTE TO SLOT",
                                         "INSERT AT SLOT", "OUTSERT SLOT"};
        int blink = (now / 260u) & 1u;
        uint16_t c = arm_f == 3u ? hot : holo;
        g_rect(14, TOP_H + 66, 212, 50, C_BG);
        g_frame(14, TOP_H + 66, 212, 50, blink ? c : g_mix(C_BG, c, 120));
        g_frame(15, TOP_H + 67, 210, 48, g_mix(C_BG, c, 90));
        s[0] = 0;
        e = str_cat(s, Q[arm_f % 8u]);
        if (arm_f != 1u) { *e++ = ' '; fmt_u(e, slot_sel, 2); }
        g_text_c(120, TOP_H + 71, s, c, T_BOLD);
        g_text_c(120, TOP_H + 89, arm_f == 1u ? (genre_pick < WB_NGENRE ? WB_GENRES[genre_pick].name : "") : arm_f == 0u ? song.name : (arm_f == SA_PASTE || arm_f == SA_INSERT) && sng_clip != 0xFF ? slot_name[sng_clip] : slot_name[slot_sel], C_TXT, T_CROP);
        g_text_c(120, TOP_H + 102, arm_f == 3u && arm_oct ? "OCT- AGAIN: DELETE  OCT+: NO" : "OCT+: YES   OCT-: NO", blink ? C_TXT : C_MUTED, T_CROP);
    }
}
/* (XENOISE) MST: the settings, every page of them at once (TEMPO, TONE, GROOVE, SYSTEM), the one the knobs are on lit
 * (GLO: the next): each a title, the knobs' names, their values */
static void draw_mst(void)
{
    uint32_t g, i, s0 = sub, now = fm1_ms;
    uint16_t holo = C_BLUE;
    char lab[16], val[24];
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    for (g = 0; g < MST_NSUB; g++) {
        int32_t y = TOP_H + 1 + (int32_t)g * 35;
        int on = g == s0 % MST_NSUB;
        g_rect(2, y, 236, 34, on ? g_mix(C_BG, holo, 46) : C_ROW);
        if (on) {
            g_frame(2, y, 236, 34, holo);
            g_rect(2 + (int32_t)(now / 6u % 236u), y + 1, 2, 32, g_mix(C_BG, holo, 110));   /* a scan along it */
        }
        g_text(6, y + 10, MST_SUB_NAME[g], on ? holo : C_MUTED, T_CROP | T_BOLD);   /* the title, left */
        sub = (uint8_t)g;
        {
            uint32_t nk = 0;
            int32_t cw;
            for (i = 0; i < 4u; i++) nk += (uint32_t)knob(i, 0, lab, val);
            cw = 180 / (int32_t)(nk ? nk : 1u);  /* (the cells share the width: SYSTEM's two, wide) */
            for (i = 0; i < 4u; i++) {
                int32_t x = 58 + (int32_t)i * cw;
                if (!knob(i, 0, lab, val)) continue;
                lab[cw / 8 > 15 ? 15 : cw / 8] = 0;
                val[cw / 8 > 15 ? 15 : cw / 8] = 0;
                if (on) g_rect(x, y + 4, 2, 26, KNOB_C[i]);
                if (on && i == pilot_col) g_frame(x - 1, y + 2, cw - 2, 30, C_TXT);   /* (ALGORITHM's: PRESETS turns it) */
                g_text(x + 4, y + 3, lab, C_MUTED, T_CROP);
                g_text(x + 4, y + 18, val, on ? C_TXT : g_mix(C_BG, C_TXT, 170), T_CROP);
            }
        }
        sub = (uint8_t)s0;
    }
}
/* (XENOISE) SEQ LIVE: the patterns as pads, 1 .. the last the song has; the one playing lit, the one queued blinking */
static void draw_live(void)
{
    uint32_t k, n = live_n(), cur = sq_play ? sq_pi : song.cur, now = fm1_ms;
    char s[8];
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    g_text(6, TOP_H + 3, "LIVE", C_ORANGE, T_CROP | T_BOLD);
    g_text(48, TOP_H + 3, song.loop ? "LOOPING A PATTERN" : "THE SEQUENCE", C_MUTED, T_CROP);
    for (k = 0; k < NPAT; k++) {
        int32_t x = 3 + (int32_t)(k % 4u) * 59, y = TOP_H + 18 + (int32_t)(k / 4u) * 62;
        int on = k < n, play = on && k == cur && sq_play, cue = on && sq_play && sq_cue == k && k != cur;
        uint16_t c = play ? C_ORANGE : cue && (now / 150u) & 1u ? C_BLUE : on ? C_PANEL : C_ROW;
        g_rect(x, y, 56, 58, c);
        if (!on) continue;
        if (k == cur && !sq_play) g_frame(x, y, 56, 58, C_BLUE);
        s[0] = (char)('1' + k);
        s[1] = 0;
        g_text_c(x + 28, y + 12, s, play ? C_BG : C_TXT, T_BOLD);
        g_text_c(x + 28, y + 36, SECT_NAME[k], play ? C_BG : C_MUTED, T_CROP);
        if (play) g_rect(x + 4, y + 52, (int32_t)((uint32_t)sq_row * 48u / (plen() ? plen() : 16u)), 3, C_BG);   /* how far in */
    }
}
static void draw_chd(void)
{
    uint32_t k, now = song.ndeg ? (uint32_t)((sq_play ? sq_row : cur_row) / (song.rows ? song.rows : 16u)) % song.ndeg : 0u;
    char nm[8], rom[8], s[40], *e;
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H, BODY_H);
    bp_chords();
    s[0] = 0;
    e = str_cat(s, WB_PROGS[(prog_due ? prog_sel : song.prog) % WB_NPROG].name);
    e = str_cat(e, "  ");
    e = str_cat(e, NOTE_NM[song.key % 12u]);
    e = str_cat(e, " ");
    str_cat(e, WB_SCALE_NAME[song.scale % WB_NSCALE]);
    g_text(6, TOP_H + 3, s, C_CYAN, T_CROP | T_BOLD);
    for (k = 0; k < 8u; k++) {
        int32_t x = 3 + (int32_t)(k % 4u) * 59, y = TOP_H + 18 + (int32_t)(k / 4u) * 44;
        int on = k < song.ndeg, play = on && k == now && sq_play, sel = k == chd_slot;
        g_rect(x, y, 56, 40, play ? C_ORANGE : on ? C_PANEL : C_ROW);
        if (sel) { g_frame(x, y, 56, 40, C_BLUE); g_frame(x + 1, y + 1, 54, 38, C_BLUE); }
        if (on) {
            chord_name(song.deg[k], nm, rom);
            g_text_c(x + 28, y + 5, nm, play ? C_BG : C_TXT, T_BOLD);
            g_text_c(x + 28, y + 24, rom, play ? C_BG : C_CYAN, T_CROP);
        } else if (k == song.ndeg) g_text_c(x + 28, y + 16, "+", C_MUTED, T_CROP);
    }
    g_text(6, TOP_H + 110, "WHITE 1-7 CHORD, 8 DEL", C_MUTED, T_CROP);
    g_text(6, TOP_H + 124, "WHITE 9-16 PICK A SLOT", C_MUTED, T_CROP);
}
static void draw_snd(void)
{
    uint32_t t = trk(), k;
    char s[40], *e;
    const char *eng, *nm;
    g_rect(0, TOP_H, 240, BODY_H, C_BG);
    bp_sheet(TOP_H + 42, BODY_H - 42);
    bp_sound();
    g_rect(0, TOP_H, 240, 42, C_ROW);
    g_rect(0, TOP_H, 3, 42, TRK_COL[t]);
    s[0] = 0;
    e = str_cat(s, TRK_ID[t]);
    e = str_cat(e, "  ");
    str_cat(e, TRK_FULL[t]);
    g_text(9, TOP_H + 4, s, C_TXT, T_CROP);
    eng = TRK_TYPE[t] == TY_BREAK ? "BREAK" : TRK_TYPE[t] == TY_SYNTH ? (const char *const[]){"FM", "ANALOG", "VOICE"}[clampi(song.trk[t].p[PY_ENG], 0, 2)] : "DRUM";
    g_text(234 - g_text_w(eng), TOP_H + 4, eng, C_MUTED, T_CROP);
    nm = TRK_TYPE[t] == TY_BREAK ? BRK_NAME[clampi(song.trk[t].p[PB_SRC], 0, BRK_N - 1)] :
         TRK_TYPE[t] == TY_DRUMS && sub >= 2u ? WB_HAT_NAME[song.trk[t].patch2 % WB_NHAT] : patch_name(t, song.trk[t].patch);
    g_text(9, TOP_H + 20, nm, C_ORANGE, T_BOLD);
    /* the sound as it plays: a line over a few guides */
    for (k = 1; k < 4u; k++) g_rect(0, TOP_H + 46 + (int32_t)k * 15, 240, 1, C_ROW);
    for (k = 1; k < 8u; k++) g_rect((int32_t)k * 30, TOP_H + 46, 1, 60, C_ROW);
    g_rect(0, TOP_H + 76, 240, 1, C_FAINT);
    for (k = 0; k < 119u; k++) {
        int32_t a = scope[(scope_w + k) % 120u], b2 = scope[(scope_w + k + 1u) % 120u];
        g_line((int32_t)k * 2, TOP_H + 76 - clampi(a / 1000, -28, 28), (int32_t)k * 2 + 2, TOP_H + 76 - clampi(b2 / 1000, -28, 28), TRK_COL[t]);
    }
    s[0] = 0;
    e = fmt_u(str_cat(s, "KNOBS "), sub % snd_npages(t) + 1u, 0);
    *e++ = '/';
    e = fmt_u(e, snd_npages(t), 0);
    str_cat(e, "  EDIT");
    g_text(6, TOP_H + 112, s, C_MUTED, T_CROP);
}
static int32_t info_y = INFO_Y;                 /* (XENOISE: lower on HOM plain, the grid longer) */
static void draw_info(void)
{
    char s[48], *e;
    int32_t pd;
    char nm[8], rom[8];
    const char *hint = 0;
    g_rect(0, info_y, 240, (info_y == INFO_Y ? KNOB_Y : 240) - info_y, C_BG);
    g_rect(0, info_y, 240, 1, C_FAINT);
    if (toast_ms && fm1_ms - toast_ms < 1800u) { g_text(5, info_y + 2, toast_s, C_ORANGE, T_CROP); return; }
    if (fx_on()) hint = "WHITE: PUNCH  BLACK: TRACKS";
    else if ((fm1_in.buttons >> BTN_SEQ) & 1u) hint = "WHITE 1-8: PATTERN  9: LOOP";
    else if (arp_on()) hint = "PADS REPEAT  HOLD ARP: OFF";
    if (hint) { g_text(5, info_y + 2, hint, C_CYAN, T_CROP); return; }
    s[0] = 0;
    str_cat(s, page == PG_TRK ? (!hom_edit ? "HOME: NOTE VEL FX AMT" : !jam_nav() ? "HOME+BLACK: TRACK, F" : PG_HINT[page])
                              : page == PG_ARR && arr_live ? "WHITE KEYS: QUEUE" : PG_HINT[page]);
    s[22] = 0;
    g_text(5, info_y + 2, s, C_MUTED, T_CROP);
    pd = prog_deg(sq_play ? sq_row : cur_row);
    chord_name(pd, nm, rom);
    s[0] = 0;
    e = str_cat(s, nm);
    e = str_cat(e, " ");
    str_cat(e, rom);
    g_text(236 - g_text_w(s), info_y + 2, s, C_CYAN, T_CROP | T_BOLD);
}
static const char *perf_fn(uint32_t i) { return (const char *const[]){perf_solo ? "SOLO" : "MUTE", "", sit_on ? "SIT OFF" : "SIT", "ALL ON"}[i]; }
static const char *arr_fn_label(uint32_t i) { return (const char *const[]){"FORM", song.loop ? "SEQ" : "LOOP", "SHUFFLE", "UNDO"}[i]; }
static void knob_badge(int32_t x, int32_t y, uint32_t i)   /* (XENOISE) the knob's number in a disc of its colour (12 px) */
{
    char d[2] = {(char)('1' + i), 0};
    g_disc((x + 6) * 16, (y + 6) * 16, 6 * 16, KNOB_C[i]);   /* (smooth edged) */
    g_text(x + 2, y, d, C_BG, T_CROP | T_BOLD);
}
static void draw_knobs(void)
{
    uint32_t i;
    char lab[16], val[24], fb[16];
    g_rect(0, KNOB_Y, 240, 240 - KNOB_Y, C_PANEL);
    for (i = 0; i < 4u; i++) {
        int32_t x = (int32_t)i * 60;
        const char *f;
        if (knob(i, 0, lab, val)) {
            val[7] = 0;
            knob_badge(x, KNOB_Y + 2, i);
            if (i == pilot_col && !fx_on() && (page == PG_MST || page == PG_SNG || (page == PG_GEN && sub < 2u)))
                g_frame(x, KNOB_Y, 60, 30, C_TXT);   /* (the PILOT's: SELECT turns it) */
            g_text(x + 13, KNOB_Y + 2, lab, C_TXT, T_CROP);
            inset(x + 2, KNOB_Y + 15, 56, 13);
            g_text(x + 4, KNOB_Y + 15, val, C_NOTE, T_CROP);
        }
        memcpy(knob_lab[i], lab, 16);
        f = fx_on() ? perf_fn(i) : page == PG_ARR ? arr_fn_label(i) : fn_label(i, fb);
        if (f[0]) {
            char ff[8];
            uint32_t n = 0;
            int hot = (arm_f == i && page == PG_SNG) || (fx_on() && i == 0u && perf_solo);
            while (f[n] && n < 7u) { ff[n] = f[n]; n++; }
            ff[n] = 0;
            g_rect(x + 2, KNOB_Y + 31, 56, 13, hot ? C_ORANGE : TH->fbtn);
            g_rect(x + 2, KNOB_Y + 31, 56, 1, TH->fbtn_top);
            g_text_c(x + 30, KNOB_Y + 31, ff, hot ? C_BG : C_TXT, T_CROP);
        }
    }
}
#include "xn_vis.c"                             /* (XENOISE) the visualizer */
#include "xn_pages.c"                           /* (XENOISE) ENV, LFO, ARP, the mixer, PERFORM's knobs */
/* (XENOISE) PILOT HELP: SEQ held a moment (alone) shows the page's controls, while it is held */
static int help_on(void) { return (fm1_in.buttons >> BTN_SEQ) & 1u && !seq_used && fm1_ms - seq_down_ms >= 700u; }
static void draw_help(void)
{
    static const char *const H[][7] = {
        {"HOM  THE TRACKER", "PRESETS  ROW", "ALGO  TRACK (PAST: VIS)", "SELECT  PATTERN", "HOME AGAIN  EDIT THE CELL", "HOLD ARP  NOTE REPEAT", "SEQ + OCT-  UNDO"},
        {"SAV  THE STORE", "PRESETS  SLOT", "ALGO  KNOB, SELECT TURNS", "K1-K4 ASK: OCT+ YES, - NO", "SAVE AGAIN  PAGE 2, UNDO", "HOLD SAVE  GEN", "OCT+ x1 EMPTY  x4 GENERATE"},
        {"CHD  THE CHORDS", "PRESETS  CHORD SLOT", "ALGO  TRACK", "SELECT  PROGRESSION", "WHITE KEYS  CHORDS", "F1 WRITE  F2 NEW  F3 UNDO", ""},
        {"ENV  THE SOUND'S SHAPE", "PRESETS  ROW", "ALGO  TRACK", "K1-K4  ITS STAGES", "ENV AGAIN  PAGE 2", "F2  PLAY IT", ""},
        {"LFO  THE TRACK'S LFO", "ALGO  TRACK", "K1-K4  RATE WAVE DEPTHS", "LFO AGAIN  PAGE 2", "F4  BACK TO THE SOUND'S", "", ""},
        {"SEQ  THE SEQUENCE", "PRESETS  SLOT", "SELECT  ITS PATTERN", "K1-K4  COPY PASTE INS OUT", "BLACK 1-6  TRACK IN, OUT", "SEQ AGAIN  LIVE", "OCT+/- ADD, DEL  F4 UNDO"},
        {"EDT  THE SOUND", "ALGO  TRACK", "SELECT  ITS PRESET", "K1-K4  ITS PARAMETERS", "EDIT AGAIN  NEXT PAGE", "F3 RANDOM  F4 INIT", ""},
        {"MST  THE SETTINGS", "PRESETS  GROUP", "ALGO  PARAMETER", "SELECT  TURNS IT", "GLO AGAIN  THE MIXER", "", ""},
        {"GEN  THE NEXT SONG", "PRESETS  GENRE", "ALGO  KNOB, SELECT TURNS", "SAVE  NEXT PAGE", "OCT+  CREATE (OCT+ AGAIN)", "F3 UNDO  F4 ALL AUTO", "HOLD SAVE  SAV"},
        {"MIX  THE MASTER", "ALGO  TRACK", "SELECT  MASTER VOLUME", "K1-K4  VOL PAN VERB DELAY", "GLO AGAIN  SETTINGS", "", ""},
        {"ARP  THE TRACK'S ARP", "ALGO  TRACK", "K1-K4  MODE RATE OCT GATE", "ARP AGAIN  PAGE 2", "F2 HOLD  F4 OFF", "", ""},
        {"FX  PERFORM", "PRESETS  ROW: FX, TRACKS", "ALGO  ALONG IT", "SELECT  ON, OFF", "K FILTER CHOP STEP WASH", "WHITE KEYS FX  BLACK MUTE", "FX AGAIN  CLOSE"},
        {"VIS  THE VISUALIZER", "PLAY  ENGAGE", "HOLD ARP  ALL RANGE", "PRESETS  TURN (L / R)", "ALGO  TURN (L / R)", "HOME  DISENGAGE", "ALGO LEFT  THE TRACKER"}};
    uint32_t k, w = fx_on() ? 11u : page == PG_TRK && vis_on ? 12u : page < PG_ALL ? page : 0u;
    int32_t y = TOP_H + 8;
    g_rect(6, TOP_H + 4, 228, 170, C_BG);
    g_frame(6, TOP_H + 4, 228, 170, C_BLUE);
    g_text(14, y, "PILOT // ", C_MUTED, T_CROP);
    g_text(86, y, H[w][0], C_ORANGE, T_CROP | T_BOLD);
    for (k = 1; k < 7u; k++) if (H[w][k][0]) g_text(14, y + 6 + (int32_t)k * 20, H[w][k], k & 1u ? C_TXT : g_mix(C_BG, C_TXT, 200), T_CROP);
}
static uint32_t ui_t0;                          /* (XENOISE) the first frame: the tracker fades up from the intro's ground */
static void draw_fadein(void)
{
    uint32_t e;
    if (!ui_t0) ui_t0 = fm1_ms ? fm1_ms : 1u;
    e = fm1_ms - ui_t0;
    if (e < 420u) g_blend(0, 0, 240, 240, C_BG, 256u - e * 256u / 420u);
}
static void draw_all(void)
{
    g_rect(0, 0, 240, 240, C_BG);
    draw_top();
    if (!fx_on() && page == PG_TRK && vis_on) { vis_draw(); if (help_on()) draw_help(); draw_fadein(); return; }
    if (!fx_on() && page == PG_TRK && !hom_edit) {   /* (XENOISE) HOM plain: the grid, the info line, no knobs */
        draw_grid();
        info_y = 222;
        draw_info();
        info_y = INFO_Y;
        if (help_on()) draw_help();
        draw_fadein();
        return;
    }
    if (fx_on()) draw_perform();
    else switch (page) {
    case PG_MST: draw_mst(); break;
    case PG_SNG: draw_sng(); break;
    case PG_GEN: if (sub < 2u) draw_gen(); else draw_grid(); break;
    case PG_CHD: draw_chd(); break;
    case PG_SND: draw_snd(); break;
    case PG_ARR:
        if (arr_live) draw_live();
        else draw_arr();
        if (!arr_live && arm_f >= ARR_ASK && arm_f < ARR_ASK + 4u) {   /* (XENOISE) a knob's question */
            static const char *const Q[4] = {"COPY SLOT", "PASTE TO SLOT", "INSERT AT SLOT", "OUTSERT SLOT"};
            char q[24], *e;
            int blink = (fm1_ms / 260u) & 1u;
            q[0] = 0;
            e = str_cat(q, Q[arm_f - ARR_ASK]);
            *e++ = ' ';
            fmt_u(e, (uint32_t)arr_pos + 1u, 0);
            g_rect(24, TOP_H + 70, 192, 40, C_BG);
            g_frame(24, TOP_H + 70, 192, 40, blink ? C_BLUE : g_mix(C_BG, C_BLUE, 120));
            g_text_c(120, TOP_H + 75, q, C_BLUE, T_BOLD);
            g_text_c(120, TOP_H + 93, "OCT+: YES   OCT-: NO", blink ? C_TXT : C_MUTED, T_CROP);
        }
        break;
    case PG_ENV: draw_env(); break;
    case PG_LFO: draw_lfo(); break;
    case PG_ARP: draw_arp(); break;
    case PG_MIX: draw_mixer(); break;
    default: draw_grid(); break;
    }
    draw_info();
    draw_knobs();
    if (help_on()) draw_help();
    draw_fadein();
}

/* ================================================================ the main loop */
static void set_lang(uint32_t l) { (void)l; }
static int app_sysex(const uint8_t *p, uint32_t n) { (void)p; (void)n; return 0; }
static void app_poll(void) {}
static void pattern_ended(uint32_t pi)          /* MUTATE and DRIFT, then SIT */
{
    uint32_t t;
    for (t = 0; t < NT; t++) {
        track_t *tr = &song.trk[t];
        if (tr->mutate && rnd(100) < tr->mutate) mutate_track(pi, t, 8u + tr->mutate / 3u);
        if (tr->drift && TRK_TYPE[t] == TY_SYNTH && tr->patch < WB_NSYN) {
            int32_t base = WB_SYN[tr->patch][PY_INDEX], span = base * tr->drift / 100 + tr->drift / 10;
            tr->p[PY_INDEX] = (int16_t)clampi(base + (int32_t)rnd((uint32_t)span * 2u + 1u) - span, 0, 80);
        }
    }
    if (sit_on) sit_step(pi);
}
static void app_init(void)
{
    keys_init();
#ifndef WB_FONT
#define WB_FONT FN_CLAIRE                       /* a plain sans, the least pixel-looking */
#endif
    g_font_set(WB_FONT);
    slots_scan();
    if (song_load(0)) {
        song_new(4);
        genre_pick = song.genre;
    } else genre_pick = song.genre;
    sq_req = 0;
    prog_sel = song.prog;
    theme_load();                               /* (XENOISE) CUSTOM, as saved */
    theme_apply();                              /* (XENOISE) SETTINGS' theme */
}
static void app_step(void)
{
    uint32_t t;
    ui_input();
    theme_poll();                               /* (XENOISE) a CUSTOM theme that came while playing: saved stopped */
    if (sq_end_pi != 0xFF) {
        uint32_t pi = sq_end_pi;
        sq_end_pi = 0xFF;
        pattern_ended(pi);
    }
    if (sq_play && sq_pi != song.cur) { song.cur = sq_pi; if (cur_row >= plen()) cur_row = 0; }
    prog_pi = (uint8_t)(song.cur % NPAT);       /* (XENOISE: the chords shown, the pattern's: an A or a B part's) */
    for (t = 0; t < NT; t++) if (trig_ms[t]) { trig_ms[t] = 0; flash_ms[t] = fm1_ms; }
    if (prog_due && fm1_ms - prog_ms > 900u) { prog_due = 0; if (prog_sel != song.prog || prog_sel == 0u) { prog_pick(prog_sel); touch(); } }
    if (aud_off_t != 0xFF && (int32_t)(fm1_ms - aud_off_ms) >= 0) { ev_push(EV_OFF, aud_off_t, 0, 0); aud_off_t = 0xFF; }
    if (arm_f != 0xFF && fm1_ms - arm_ms > ARM_MS) arm_f = 0xFF;
    if (sng_taps && fm1_ms - sng_tap_ms > 650u) {   /* (XENOISE) OCT+ on SAV: one tap an empty song; two or three nothing */
        if (sng_taps == 1u && page == PG_SNG) { sng_fresh_empty = 1; fn_press_now(1); }
        else toast("TAP OCT+ 4 TIMES TO GENERATE");
        sng_taps = 0;
    }
    if (vis_on && page == PG_TRK) vis_step();
    if (cur_row >= plen()) cur_row = 0;
    if (fm1_ms - frame_ms >= 33u) {
        frame_ms = fm1_ms;
        ui_leds();
        g_frame_send(draw_all);
    }
    if (dirty && !sq_play && quiet() && fm1_ms - dirty_ms > 6000u) {   /* the autosave (erasing stalls the sound) */
        if (song_save(0) == 0) dirty = 0;
        else dirty_ms = fm1_ms;
    }
}
static void app_main(void)
{
    app_init();
    for (;;) {
        platform_poll();
        app_step();
    }
}

#if defined(KAWA_WASM)
/* the emulator page: the knobs' labels, the page, the track; the song slots live in the page's memory (it keeps them) */
static uint8_t w_smp_stage[16];
static char w_smp_name[16];
WASM_EXPORT("kawa_page") uint32_t kawa_page(void) { return page; }
WASM_EXPORT("kawa_knob_label") const char *kawa_knob_label(uint32_t i) { return i < 4u ? knob_lab[i] : ""; }
WASM_EXPORT("kawa_track") uint32_t kawa_track(void) { return trk(); }
WASM_EXPORT("kawa_theme") uint32_t kawa_theme(void) { return 0; }
WASM_EXPORT("kawa_sample_buf") uint8_t *kawa_sample_buf(void) { return w_smp_stage; }
WASM_EXPORT("kawa_sample_name") char *kawa_sample_name(void) { return w_smp_name; }
WASM_EXPORT("kawa_sample_size") uint32_t kawa_sample_size(void) { return sizeof w_smp_stage; }
WASM_EXPORT("kawa_sample_commit") void kawa_sample_commit(uint32_t slot, uint32_t len, uint32_t rate) { (void)slot; (void)len; (void)rate; toast("SAMPLES: NOT YET"); }
WASM_EXPORT("kawa_sample_slot_name") const char *kawa_sample_slot_name(uint32_t slot) { (void)slot; return ""; }
WASM_EXPORT("kawa_store") uint8_t *kawa_store(void) { return &host_flash[0][0]; }
WASM_EXPORT("kawa_store_size") uint32_t kawa_store_size(void) { return NSLOT * SLOT_SIZE; }
WASM_EXPORT("kawa_store_gen") uint32_t kawa_store_gen(void) { return store_gen; }
/* audio export (the page renders offline, in its own copy of the engine): the song comes in as bytes, then
 * wb_export(what, solo) gets the engine ready and says how many frames make it:
 *   what 0..7   pattern A..H as a loop: the page renders it twice and keeps the second pass (the reverb's tail
 *               runs into the start, so the loop is seamless), the length is exact at the tempo
 *   what 8      the whole arrangement once, then two seconds of tail
 *   solo        0 every track, 1 + t that track alone (a stem); the same random numbers every time */
WASM_EXPORT("wb_song") uint8_t *wb_song(void) { return (uint8_t *)&song; }
WASM_EXPORT("wb_song_size") uint32_t wb_song_size(void) { return sizeof song; }
WASM_EXPORT("wb_pat_used") uint32_t wb_pat_used(uint32_t p)   /* 1: the pattern has a note */
{
    uint32_t t, r;
    for (t = 0; t < NT; t++) for (r = 0; r < NROW; r++) if (song.pat[p % NPAT].c[t][r].n) return 1;
    return 0;
}
static uint32_t pat_frames(uint32_t p)           /* exact: steps x 15 s / BPM at 44100 Hz */
{
    uint32_t L = song.pat[p % NPAT].len ? song.pat[p % NPAT].len : 16u;
    return (uint32_t)(((uint64_t)steps_of(L) * 661500u + song.bpm / 2u) / (song.bpm ? song.bpm : 100u));
}
WASM_EXPORT("wb_export") uint32_t wb_export(uint32_t what, uint32_t solo)
{
    uint32_t t, n = 0, k;
    dsp_reset();
    for (t = 0; t < NT; t++) { song.trk[t].solo = (uint8_t)(solo && t == solo - 1u); if (solo) song.trk[t].mute = 0; }
    if (what < NPAT) { song.loop = 1; song.cur = (uint8_t)what; n = pat_frames(what); }
    else {
        song.loop = 0;
        for (k = 0; k < (song.nchain ? song.nchain : 1u); k++) n += pat_frames(song.nchain ? song.chain[k] : song.cur);
        n += 2u * 44100u;
    }
    sq_req = 1;
    return n;
}
#endif
#if defined(KAWA_WASM) || defined(__EMSCRIPTEN__)   /* (XENOISE: the MIDI file for the emulator and the Dock too) */
#ifndef WASM_EXPORT
#define WASM_EXPORT(name) static
#endif
/* ---- for Renoise and any DAW: the song as a standard MIDI file (format 1, 96 ticks a beat, a row a 16th), and each
 * track's sound as a sample. The arrangement in order (or the pattern looped), the meter and the swing as played;
 * RETRIG as its hits, DELAY as its offset, chords as their notes. BRK: slice k on note 61 + k (Renoise's slices go
 * up from C#4); KIK 36; DRM general MIDI: snare 38, clap 39, closed hat 42, open hat 46; BAS KEY SYN their notes. */
typedef struct {
    uint32_t tick;
    uint8_t st, d1, d2, pad;
} mev_t;
#define NMEV 40000u
static mev_t w_mev[NMEV];
static uint8_t w_midi[300000];
static uint32_t w_midi_n, w_nmev;
static void mput(uint32_t b) { if (w_midi_n < sizeof w_midi) w_midi[w_midi_n++] = (uint8_t)b; }
static void mvlq(uint32_t v)
{
    uint8_t t[5];
    uint32_t n = 0;
    do { t[n++] = (uint8_t)(v & 127u); v >>= 7; } while (v && n < 5u);
    while (n) { n--; mput(t[n] | (n ? 0x80u : 0u)); }
}
static void mstr(const char *x) { while (*x) mput((uint8_t)*x++); }
static void mev(uint32_t tick, uint32_t st, uint32_t d1, uint32_t d2)
{
    if (w_nmev < NMEV) { w_mev[w_nmev].tick = tick; w_mev[w_nmev].st = (uint8_t)st; w_mev[w_nmev].d1 = (uint8_t)d1; w_mev[w_nmev].d2 = (uint8_t)d2; w_nmev++; }
}
static void mnote(uint32_t ch, uint32_t at, uint32_t len, uint32_t note, uint32_t vel)
{
    if (note > 127u) return;
    mev(at, 0x90u | ch, note, vel ? vel : 1u);
    mev(at + (len > 1u ? len - 1u : 1u), 0x80u | ch, note, 0);
}
static uint32_t mtrack_begin(const char *name)
{
    uint32_t at;
    mstr("MTrk");
    at = w_midi_n;
    mput(0); mput(0); mput(0); mput(0);
    mvlq(0); mput(0xFF); mput(0x03); mvlq(str_len(name)); mstr(name);
    return at;
}
static void mtrack_end(uint32_t at)
{
    uint32_t n;
    mvlq(0); mput(0xFF); mput(0x2F); mput(0);
    n = w_midi_n - at - 4u;
    w_midi[at] = (uint8_t)(n >> 24); w_midi[at + 1] = (uint8_t)(n >> 16); w_midi[at + 2] = (uint8_t)(n >> 8); w_midi[at + 3] = (uint8_t)n;
}
static void msort(void)                         /* by time, a note's end before a note's start at the same tick */
{
    uint32_t i, j;
    for (i = 1; i < w_nmev; i++) {
        mev_t e = w_mev[i];
        uint32_t k = e.tick * 2u + ((e.st & 0xF0u) == 0x90u);
        for (j = i; j > 0u && w_mev[j - 1u].tick * 2u + ((w_mev[j - 1u].st & 0xF0u) == 0x90u) > k; j--) w_mev[j] = w_mev[j - 1u];
        w_mev[j] = e;
    }
}
WASM_EXPORT("wb_midi") uint8_t *wb_midi(void)
{
    static const uint8_t CHN[NT] = {0, 1, 9, 3, 4, 5}, GMD[4] = {38, 42, 46, 39};
    uint32_t t, k, parts = song.loop || !song.nchain ? 1u : song.nchain, B = meter_steps();
    uint32_t sw = (uint32_t)clampi(song.swing - 50, 0, 25) * 24u * 2u / 100u;
    w_midi_n = 0;
    mstr("MThd"); mput(0); mput(0); mput(0); mput(6); mput(0); mput(1); mput(0); mput(NT + 1u); mput(0); mput(96);
    {                                           /* the tempo, the meter, the name */
        uint32_t at = mtrack_begin(song.name), us = 60000000u / (song.bpm ? song.bpm : 100u);
        static const uint8_t NUM[NMETER] = {4, 3, 6, 5, 7, 9}, DEN[NMETER] = {2, 2, 3, 2, 3, 3};
        mvlq(0); mput(0xFF); mput(0x51); mput(3); mput(us >> 16); mput(us >> 8); mput(us);
        mvlq(0); mput(0xFF); mput(0x58); mput(4); mput(NUM[song.meter % NMETER]); mput(DEN[song.meter % NMETER]); mput(24); mput(8);
        mtrack_end(at);
    }
    for (t = 0; t < NT; t++) {
        uint32_t at = mtrack_begin(TRK_ID[t]), base = 0, last = 0, i;
        w_nmev = 0;
        for (k = 0; k < parts; k++) {
            const pat_t *p = &song.pat[(song.loop || !song.nchain ? song.cur : song.chain[k]) % NPAT];
            uint32_t L = p->len ? p->len : 16u, n = steps_of(L), st;
            for (st = 0; st < n; st++) {
                uint32_t r = row_of(st) % L, at0 = base + st * 24u + ((st & 1u) ? sw : 0u), hits, h, vel, len;
                const cell_t *c = &p->c[t][r];
                if (!c->n || c->n == NOTE_OFF) continue;
                at0 += c->fx == FX_D ? c->fv * 24u / 16u : 0u;
                hits = c->fx == FX_R && c->fv > 1u ? c->fv : 1u;
                vel = (uint32_t)c->v * 127u / 15u;
                len = TRK_TYPE[t] == TY_SYNTH ? gate_rows(p, t, r) * 24u * (B < 16u ? 1u : 1u) : 24u / hits;
                for (h = 0; h < hits; h++) {
                    uint32_t a = at0 + 24u * h / hits;
                    switch (TRK_TYPE[t]) {
                    case TY_BREAK: mnote(CHN[t], a, 24u / hits, 61u + (c->n - 1u) % 16u, vel); break;
                    case TY_KICK: mnote(CHN[t], a, 24u / hits, 36u, vel); break;
                    case TY_DRUMS: mnote(CHN[t], a, 24u / hits, GMD[drm_kind(c->n)], vel); break;
                    default: {
                        int32_t ns[5];
                        uint32_t q, m = (uint32_t)clampi((int32_t)c->n + 12 * song.trk[t].p[PY_OCT], 12, 120);
                        uint32_t cnt = chord_notes((int32_t)m, (uint32_t)clampi(song.trk[t].p[PY_CHORD], 0, WB_NCHORD - 1), ns);
                        for (q = 0; q < cnt; q++) mnote(CHN[t], a, hits > 1u ? 24u / hits : len, (uint32_t)clampi(ns[q], 0, 127), vel);
                    }
                    }
                }
            }
            base += n * 24u;
        }
        msort();
        for (i = 0; i < w_nmev; i++) {
            mvlq(w_mev[i].tick - last);
            last = w_mev[i].tick;
            mput(w_mev[i].st); mput(w_mev[i].d1); mput(w_mev[i].d2);
        }
        mtrack_end(at);
    }
    return w_midi;
}
WASM_EXPORT("wb_midi_size") uint32_t wb_midi_size(void) { return w_midi_n; }
#endif
#if defined(KAWA_WASM)
/* a sample of a track's sound: one hit (dry: no sends, no tape, no crush, no pump; synths one note, no chord) */
WASM_EXPORT("wb_hit") void wb_hit(uint32_t t, uint32_t note)
{
    uint32_t k;
    dsp_reset();
    for (k = 0; k < NT; k++) { song.trk[k].rev = song.trk[k].dly = 0; song.trk[k].mute = song.trk[k].solo = 0; song.trk[k].vol = 70; }
    song.space = song.age = song.crush = song.pump = 0;
    song.mvol = 100;
    if (TRK_TYPE[t % NT] == TY_SYNTH) { song.trk[t % NT].p[PY_CHORD] = 0; song.trk[t % NT].p[PY_CTR] = 0; song.trk[t % NT].p[PY_OCT] = 0; }
    ev_push(EV_ON, t % NT, note, 15);
}
WASM_EXPORT("wb_release") void wb_release(uint32_t t) { ev_push(EV_OFF, t % NT, 0, 0); }
WASM_EXPORT("wb_break") int16_t *wb_break(void)   /* the break as it is stored, a bar at 160 BPM */
{
    static int16_t pcm[BRK_LEN];
    uint32_t i, src = (uint32_t)clampi(song.trk[T_BRK].p[PB_SRC], 0, BRK_N - 1);
    for (i = 0; i < BRK_LEN; i++) pcm[i] = BRK_ULAW[BRK_DATA[src][i]];
    return pcm;
}
WASM_EXPORT("wb_break_len") uint32_t wb_break_len(void) { return BRK_LEN; }
WASM_EXPORT("wb_break_name") const char *wb_break_name(void) { return BRK_NAME[clampi(song.trk[T_BRK].p[PB_SRC], 0, BRK_N - 1)]; }
WASM_EXPORT("wb_patch_name") const char *wb_patch_name(uint32_t t) { return TRK_TYPE[t % NT] == TY_BREAK ? "" : patch_name(t % NT, song.trk[t % NT].patch); }
WASM_EXPORT("wb_hat_name") const char *wb_hat_name(void) { return WB_HAT_NAME[song.trk[T_DRM].patch2 % WB_NHAT]; }
WASM_EXPORT("wb_export_mark") uint32_t wb_export_mark(void) { return exp_mark; }   /* the frame pass 2 starts on */
WASM_EXPORT("kawa_store_loaded") void kawa_store_loaded(void)   /* the page put the slots back: the autosave, if any */
{
    slots_scan();
    if (!song_load(0)) { genre_pick = song.genre; prog_sel = song.prog; cur_row = 0; }
    g_force = 1;
}
#endif
#if defined(__EMSCRIPTEN__)                    /* (felucca_web.c exports them) */
/* (XENOISE) the emulator page's EXPORT: a copy of the running device (its whole memory) plays the sequence from its
 * first slot, once, offline; this says how many frames that takes (steps x 15 s / BPM at 44.1 kHz), then STOP lets
 * the tail ring out */
static uint32_t xn_export_song(uint32_t solo)   /* solo: 0 the mix, 1 + t that track alone (a stem) */
{
    /* a stem: every track still plays (the kick still pumps the rest), only the others' sound is left out of the mix
     * (JIANT's mix_skip: a bit a JIANT track; KEY with its counter voice). KIK and PER share one: the other's notes off */
    static const uint8_t JT[NT] = {1u << 0, 1u << 1, 1u << 1, 1u << 2, 1u << 3 | 1u << 5, 1u << 4};
    uint32_t k, n = 0, bpm = song.bpm ? song.bpm : 120u;
    for (k = 0; k < NT; k++) song.trk[k].solo = 0;
    mix_skip = (uint8_t)(solo ? 0x3Fu & ~JT[(solo - 1u) % NT] : 0u);
    if (solo == 1u + T_KIK) song.trk[T_DRM].mute = 1;
    if (solo == 1u + T_DRM) song.trk[T_KIK].mute = 1;
    song.loop = 0;
    sq_start = 0;
    sit_on = 0;
    for (k = 0; k < (song.nchain ? song.nchain : 1u); k++) {
        uint32_t pi = (song.nchain ? song.chain[k] : song.cur) % NPAT, L = song.pat[pi].len ? song.pat[pi].len : 16u;
        n += (uint32_t)(((uint64_t)steps_of(L) * 661500u + bpm / 2u) / bpm);
    }
    sq_req = 1;
    return n;
}
static void xn_export_stop(void) { sq_req = 2; }
static uint8_t *xn_song_buf(void) { return (uint8_t *)&song; }
static uint32_t xn_song_size(void) { return sizeof song; }
static uint32_t xn_song_took(void)               /* (XENOISE) a song written in from outside (the Dock): 0 = taken */
{
    if (song.magic != SONG_MAGIC || song.version != SONG_VERSION) return 1;
    if (song.x.magic != SONG_EXT_MAGIC) { memset(&song.x, 0, sizeof song.x); song.x.magic = SONG_EXT_MAGIC; }
    sq_req = 2;
    song.cur %= NPAT;
    if (!song.nchain || song.nchain > 16u) { song.nchain = 1; song.chain[0] = 0; }
    seed_of = 0xFF;
    sit_mute = 0;
    cur_row = 0;
    g_force = 1;
    return 0;
}
static const char *xn_track_name(uint32_t t) { return TRK_ID[t % NT]; }
static const char *xn_song_name(void) { return song.name; }
#endif
