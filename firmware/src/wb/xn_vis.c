/* SPDX-License-Identifier: GPL-3.0-only */
/* XENOISE: the visualizer (HOM, the track scrolled right past SYN; back left: SYN again). A mech's cockpit, first
 * person; the song is its fight. Each track fires its own weapon on its hits (the sequencer's trig_ms, flash_ms):
 *   KIK  the left cannon     PER  the right cannon     BRK  the chin gun (a burst; the cockpit shakes)
 *   BAS  a missile           KEY / SYN  a laser (at what flies)
 * The targets (city towers and military bases along the way; tanks, gunships, drones coming) come in a wave on every
 * bar line while it plays: more in a CHORUS,
 * the most in a DROP (a gunship leads it), one at a time in an INTRO or OUTRO; stopped, they patrol and nothing fires.
 * The ground runs at the tempo. What reaches the mech takes its shield (red at the edges); it comes back by itself.
 * The state: a few small arrays, stepped once a frame (vis_step, the main loop); drawn band by band (vis_draw, the
 * same every band: nothing random in it). World units: 1/16 m; the view: x right, h up, z ahead.
 * ALL RANGE (ARP held on it): the mech stops and turns; PRESETS or ALGORITHM turns it (left, right); what comes, comes from all
 * round. HOME: it disengages and goes home. Each way a boot screen plays first, as the intro's. */

#define VIS_Y0 TOP_H                            /* the view: from under the tabs to the screen's foot */
#define VIS_HOR (TOP_H + 70)                    /* the horizon */
#define VIS_F 150                               /* the lens: px a world unit at depth 1 */
#define VIS_CAMH 40                             /* the eyes, 2.5 m up */
#define VIS_ZFAR 1440                           /* 90 m: where they come from */
#define VIS_ZNEAR 96                            /* 6 m: where they reach the mech */
#define VIS_NE 12u
#define VIS_NS 10u
#define VIS_NB 12u
enum { VE_TOWER, VE_TANK, VE_SHIP, VE_DRONE, VE_BASE };   /* (XENOISE: a city tower, a military base; WND: a walker) */
#define VIS_FIXED(t) ((t) == VE_TOWER || (t) == VE_BASE)   /* (they stand: the mech goes by them) */
enum { VW_LEFT, VW_RIGHT, VW_CHIN, VW_MISSILE, VW_LASER };
typedef struct { uint8_t on, type, hp, pad; int32_t x, h, z; uint32_t hit_ms, born; } vis_en_t;
typedef struct { uint8_t on, kind; int32_t x, h, z; uint32_t t0; } vis_shot_t;
typedef struct { uint8_t on, big; int32_t x, h, z; uint32_t t0; } vis_boom_t;
static vis_en_t vis_en[VIS_NE];
static vis_shot_t vis_sh[VIS_NS];
static vis_boom_t vis_bm[VIS_NB];
static uint8_t vis_on, vis_wave_row = 0xFF;
static uint32_t vis_rnd_s = 0x2545F491u, vis_ms, vis_scroll, vis_seen[NT], vis_kills, vis_wave, vis_shake_ms, vis_dmg_ms,
                vis_fire_ms[5], vis_patrol_ms;
static int32_t vis_shield = 100;
static uint8_t vis_ar;                          /* ALL RANGE: 0 off, 1 engaging, 2 on, 3 disengaging */
static uint32_t vis_ar_ms;
static uint16_t vis_yaw;                        /* where the mech looks (ALL RANGE), 16 bits a turn */

static uint32_t vis_rnd(uint32_t n)
{
    vis_rnd_s ^= vis_rnd_s << 13;
    vis_rnd_s ^= vis_rnd_s >> 17;
    vis_rnd_s ^= vis_rnd_s << 5;
    return n ? (vis_rnd_s >> 8) % n : 0u;
}
static const char *vis_section(void)
{
    uint32_t pi = sq_play && !song.loop && song.nchain ? song.chain[sq_chainpos % song.nchain] : song.cur;
    return SECT_NAME[pi % NPAT];
}
static void vis_spawn(uint32_t type)
{
    uint32_t i;
    vis_en_t *e;
    for (i = 0; i < VIS_NE && vis_en[i].on; i++) {}
    if (i == VIS_NE) return;
    e = &vis_en[i];
    e->on = 1;
    e->type = (uint8_t)type;
    e->hp = (uint8_t)(type == VE_SHIP ? 9 : type == VE_TANK ? 4 : type == VE_TOWER ? 5 : type == VE_BASE ? 6 : 1);
    e->x = (int32_t)vis_rnd(800) - 400;
    if (VIS_FIXED(type)) e->x = (e->x < 0 ? -1 : 1) * (260 + (int32_t)vis_rnd(260));   /* (along the way, either side) */
    e->z = VIS_ZFAR - (int32_t)vis_rnd(320);
    if (vis_ar) {                               /* ALL RANGE: from anywhere round */
        uint32_t a = vis_rnd(65536);
        int32_t d = VIS_FIXED(type) ? 300 + (int32_t)vis_rnd(900) : VIS_ZFAR - (int32_t)vis_rnd(320);
        e->x = d * xj_sin(a) / 32768;
        e->z = d * xj_sin(a + 0x4000u) / 32768;
    }
    e->h = type == VE_SHIP ? 140 + (int32_t)vis_rnd(80) : type == VE_DRONE ? 60 + (int32_t)vis_rnd(60) : 0;
    e->hit_ms = 0;
    e->born = vis_ms;
}
static void vis_boom(int32_t x, int32_t h, int32_t z, uint32_t at, int big)
{
    uint32_t i, old = 0;
    for (i = 0; i < VIS_NB; i++) {
        if (!vis_bm[i].on) break;
        if (vis_bm[i].t0 < vis_bm[old].t0) old = i;
    }
    if (i == VIS_NB) i = old;
    vis_bm[i].on = 1;
    vis_bm[i].big = (uint8_t)big;
    vis_bm[i].x = x;
    vis_bm[i].h = h + (big ? 8 : 16);
    vis_bm[i].z = z;
    vis_bm[i].t0 = at;
}
static void vis_turn(int32_t x, int32_t z, int32_t *vx, int32_t *vz)   /* the world as the mech looks (ALL RANGE) */
{
    int32_t c, sn;
    if (!vis_yaw) { *vx = x; *vz = z; return; }
    c = xj_sin((uint32_t)vis_yaw + 0x4000u);
    sn = xj_sin(vis_yaw);
    *vx = (int32_t)(((int64_t)x * c - (int64_t)z * sn) >> 15);
    *vz = (int32_t)(((int64_t)x * sn + (int64_t)z * c) >> 15);
}
/* a weapon fires: at the nearest enemy it suits (lasers at what flies, cannons at what walks), its damage at once, the
 * explosion when the round gets there */
static void vis_fire(uint32_t kind)
{
    static const uint8_t DMG[5] = {2, 2, 1, 4, 2};
    uint32_t i, best = VIS_NE, travel = kind == VW_MISSILE ? 260u : kind == VW_LASER ? 40u : 110u;
    int32_t bz = 0x7FFFFFFF;
    vis_shot_t *s = 0;
    for (i = 0; i < VIS_NE; i++) {
        const vis_en_t *e = &vis_en[i];
        int air = e->type == VE_SHIP || e->type == VE_DRONE;
        int32_t z;
        if (!e->on) continue;
        if (vis_ar) {                           /* ALL RANGE: what is in view, near enough */
            int32_t vx, vz;
            vis_turn(e->x, e->z, &vx, &vz);
            if (vz < 16 || vz > VIS_ZFAR * 3 / 5 || (vx < 0 ? -vx : vx) > vz) continue;
            z = vz + (kind == VW_LASER ? (air ? 0 : 2000) : (air ? 600 : 0));
            if (z < bz) { bz = z; best = i; }
            continue;
        }
        if (e->z > VIS_ZFAR * 2 / 5) continue;   /* (in range: near enough to see go) */
        z = e->z + (kind == VW_LASER ? (air ? 0 : 2000) : kind == VW_MISSILE ? (e->type == VE_SHIP ? -400 : 0) : (air ? 600 : 0));
        if (z < bz) { bz = z; best = i; }
    }
    for (i = 0; i < VIS_NS && vis_sh[i].on; i++) {}
    if (i < VIS_NS) s = &vis_sh[i];
    vis_fire_ms[kind] = vis_ms;
    if (kind == VW_CHIN) vis_shake_ms = vis_ms;
    if (s) {
        s->on = 1;
        s->kind = (uint8_t)kind;
        s->t0 = vis_ms;
    }
    if (best == VIS_NE) {                       /* nothing there: a round into the distance */
        if (s) { s->x = ((int32_t)vis_rnd(200) - 100); s->h = 30; s->z = VIS_ZFAR; }
        return;
    }
    {
        vis_en_t *e = &vis_en[best];
        if (s) { s->x = e->x; s->h = e->h + 20; s->z = e->z; }
        e->hit_ms = vis_ms + travel;
        if (e->hp > DMG[kind]) { e->hp = (uint8_t)(e->hp - DMG[kind]); vis_boom(e->x, e->h, e->z, vis_ms + travel, 0); return; }
        e->on = 0;
        vis_kills++;
        vis_boom(e->x, e->h, e->z, vis_ms + travel, 1);
    }
}

/* once a frame (the main loop, while it shows): time, the ground, the waves, the hits, what reaches the mech */
static void vis_step(void)
{
    static const uint8_t WEAPON[NT] = {VW_CHIN, VW_LEFT, VW_RIGHT, VW_MISSILE, VW_LASER, VW_LASER};
    uint32_t now = fm1_ms, dt = vis_ms ? now - vis_ms : 33u, t, i;
    int32_t speed = (int32_t)(song.bpm ? song.bpm : 120u);   /* the ground: 6 m a beat */
    if (vis_ms && dt < 30u) return;             /* (a step a frame: the main loop runs every 4 ms, too fine to move by) */
    if (dt > 100u) dt = 100u;
    vis_ms = now;
    if (vis_ar == 1u && now - vis_ar_ms > 2000u) vis_ar = 2;   /* (engaged) */
    if (vis_ar == 3u && now - vis_ar_ms > 1100u) { vis_ar = 0; vis_yaw = 0; vis_on = 0; memset(vis_en, 0, sizeof vis_en); return; }   /* (home) */
    if (!vis_ar) vis_scroll += dt * (uint32_t)speed * 96u / 60000u;   /* (ALL RANGE: it stands) */
    if (sq_play) {
        uint32_t row = sq_row, bar = (row / 16u) & 0xFFu;
        if (vis_wave_row == 0xFF || (row % 16u == 0u && bar != vis_wave_row)) {   /* a wave on the bar line */
            const char *sec = vis_section();
            uint32_t n = sec[0] == 'D' ? 4u : sec[0] == 'C' ? 3u : sec[0] == 'I' || sec[0] == 'O' || (sec[0] == 'B' && sec[1] == 'R') ? 1u : 2u;
            vis_wave_row = (uint8_t)bar;
            vis_wave++;
            if (sec[0] == 'D' && !vis_rnd(2)) vis_spawn(VE_SHIP);
            for (i = 0; i < n; i++) vis_spawn((const uint8_t[]){VE_TOWER, VE_TANK, VE_DRONE, VE_BASE, VE_DRONE, VE_TOWER}[vis_rnd(6)]);
        }
        for (t = 0; t < NT; t++)                /* the hits: each track its weapon */
            if (flash_ms[t] != vis_seen[t]) {
                vis_seen[t] = flash_ms[t];
                if (!song.trk[t].mute) vis_fire(WEAPON[t]);
            }
    } else {
        vis_wave_row = 0xFF;
        for (t = 0; t < NT; t++) vis_seen[t] = flash_ms[t];
        if (now - vis_patrol_ms > 2400u) { vis_patrol_ms = now; vis_spawn((const uint8_t[]){VE_TOWER, VE_DRONE, VE_BASE}[vis_rnd(3)]); }
    }
    for (i = 0; i < VIS_NE; i++) {
        vis_en_t *e = &vis_en[i];
        int32_t v;
        if (!e->on) continue;
        v = (e->type == VE_DRONE ? 14 : e->type == VE_SHIP ? 7 : e->type == VE_TANK ? 9 : 12) * speed / 120;   /* m/s (90 m in ~10 s;
                                                 * what stands: the ground's speed) */
        if (vis_ar) {                           /* ALL RANGE: straight at the mech, from where it is; what stands, stands */
            int32_t ax = e->x < 0 ? -e->x : e->x, az = e->z < 0 ? -e->z : e->z, d = (ax > az ? ax + az / 2 : az + ax / 2) + 1, st;
            if (VIS_FIXED(e->type)) { if (vis_ms - e->born > 16000u) e->on = 0; continue; }
            st = (int32_t)((uint32_t)(v * 16) * dt / 1000u);
            e->x -= e->x * st / d;
            e->z -= e->z * st / d;
            if (d > VIS_ZNEAR) continue;
        } else {
            e->z -= (int32_t)((uint32_t)(v * 16) * dt / 1000u);
            if (e->type == VE_DRONE) e->x += ((vis_ms / 700u + i) & 1u) ? 1 : -1;
            if (e->z >= VIS_ZNEAR) continue;
        }
        if (VIS_FIXED(e->type)) { e->on = 0; continue; }   /* (gone by) */
        {                                       /* it reached the mech */
            e->on = 0;
            vis_dmg_ms = now;
            vis_shield -= e->type == VE_SHIP ? 25 : 10;
            if (vis_shield < 0) vis_shield = 100;   /* (a reboot: no end to it) */
        }
    }
    if (vis_shield < 100 && !(now / 400u % 4u) && now - vis_dmg_ms > 3000u) vis_shield++;
    for (i = 0; i < VIS_NS; i++) if (vis_sh[i].on && now - vis_sh[i].t0 > 300u) vis_sh[i].on = 0;
    for (i = 0; i < VIS_NB; i++) if (vis_bm[i].on && (int32_t)(now - vis_bm[i].t0) > 900) vis_bm[i].on = 0;
}
static void vis_open(void)
{
    vis_on = 1;
    vis_ms = 0;
    vis_wave_row = 0xFF;
    vis_shield = 100;
    vis_ar = 0;
    vis_yaw = 0;
    toast("VISUALIZER - HOLD ARP: ALL RANGE");
}

/* ---- drawing */
static int vis_proj(int32_t x, int32_t h, int32_t z, int32_t *px, int32_t *py, int32_t *sc)   /* sc: px a metre, Q4 */
{
    vis_turn(x, z, &x, &z);
    if (z < 16) return 0;
    *px = 120 + x * VIS_F / z;
    *py = VIS_HOR + (VIS_CAMH - h) * VIS_F / z;
    *sc = 16 * VIS_F * 32 / z;                   /* (drawn larger than life: they read at a distance) */
    return 1;
}
/* a box of w x hh metres (Q4) whose foot is dy metres (Q4) over (px, py), centred */
static void vis_box(int32_t px, int32_t py, int32_t sc, int32_t w, int32_t hh, int32_t dy, uint16_t c)
{
    int32_t W = w * sc >> 8, H = hh * sc >> 8, Y = py - (dy * sc >> 8) - H;
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    g_rect(px - W / 2, Y, W, H, c);
}
static void vis_enemy(const vis_en_t *e, uint16_t fill, uint16_t hi, uint16_t eye)
{
    int32_t px, py, sc;
    if (!vis_proj(e->x, e->h, e->z, &px, &py, &sc)) return;
    if (!g_vis(py - (sc * 8 >> 4) - 4, (sc * 8 >> 4) + 8)) return;
    switch (e->type) {
    case VE_TOWER: {                            /* (XENOISE) a city tower: its windows lit, some, a mast on top */
        int32_t j, col;
        uint32_t seed = (uint32_t)(e->x * 7 + e->born);
        vis_box(px, py, sc, 40, 120, 0, fill);
        vis_box(px, py, sc, 44, 4, 120, hi);
        if ((seed & 3u) == 0u) vis_box(px, py, sc, 2, 24, 124, hi);
        for (j = 0; j < 6; j++)
            for (col = -1; col <= 1; col++)
                if ((seed >> ((j * 3 + col + 1) & 15)) & 1u) vis_box(px + col * (sc * 12 >> 8), py, sc, 6, 8, 12 + j * 17, eye);
        break;
    }
    case VE_BASE: {                             /* (XENOISE) a military base: a bunker, its dome, a radar mast turning, a light */
        int32_t r = (sc * 14 >> 8) * xj_sin(vis_ms * 300u) / 32768, my = py - (sc * 46 >> 8);
        vis_box(px, py, sc, 80, 14, 0, fill);
        vis_box(px - (sc * 12 >> 8), py, sc, 36, 10, 14, hi);
        vis_box(px + (sc * 24 >> 8), py, sc, 3, 32, 14, fill);
        g_line(px + (sc * 24 >> 8) - r, my, px + (sc * 24 >> 8) + r, my, hi);
        if ((vis_ms / 400u) & 1u) vis_box(px + (sc * 24 >> 8), py, sc, 4, 4, 46, eye);
        break;
    }
    case VE_TANK:                               /* tracks, a hull, a turret, the barrel's mouth at the mech */
        vis_box(px, py, sc, 64, 8, 0, fill);
        vis_box(px, py, sc, 56, 12, 8, hi);
        vis_box(px, py, sc, 28, 10, 20, fill);
        vis_box(px, py, sc, 6, 6, 22, eye);
        break;
    case VE_SHIP: {                             /* a gunship: a long hull, swept wings, two engines glowing */
        int32_t bob = (int32_t)((vis_ms / 90u) % 8u);
        int32_t W = 100 * sc >> 8, y = py - (bob > 3 ? 7 - bob : bob);
        vis_box(px, y, sc, 100, 14, 0, fill);
        vis_box(px, y, sc, 50, 8, 14, hi);
        g_line(px - W / 2, y - 1, px - W, y + (W >> 3), hi);
        g_line(px + W / 2, y - 1, px + W, y + (W >> 3), hi);
        vis_box(px - (sc * 30 >> 8), y, sc, 10, 6, -6, eye);
        vis_box(px + (sc * 30 >> 8), y, sc, 10, 6, -6, eye);
        break;
    }
    default: {                                  /* a drone: a body, its rotor turning */
        int32_t r = (sc * 22 >> 8) * xj_sin(vis_ms * 900u) / 32768;
        vis_box(px, py, sc, 14, 8, 0, fill);
        g_line(px - r, py - (sc * 10 >> 8), px + r, py - (sc * 10 >> 8), hi);
        vis_box(px, py, sc, 4, 3, 2, eye);
        break;
    }
    }
}
/* (XENOISE) ALL RANGE's boot screens, as the intro's: the protocol's name, its checks one by one, a bar */
static void vis_boot(int on)
{
    static const char *const L1[4] = {"> GYRO LOCK ........ OK", "> 360 TRACKING ..... OK", "> RADAR SWEEP ...... OK", "> ALL WEAPONS ...... FREE"};
    static const char *const L0[3] = {"> WEAPONS .......... SAFE", "> GYRO ............. HOME", "> RETURNING TO BASE"};
    uint32_t t = fm1_ms - vis_ar_ms, len = on ? 2000u : 1100u, n = on ? 4u : 3u, k;
    int32_t y0 = VIS_Y0 + 30, f = (int32_t)(t * 200u / len);
    uint16_t bg = C_BG, cy = C_BLUE, hot = on ? C_ORANGE : C_CYAN;
    g_rect(0, VIS_Y0, 240, 240 - VIS_Y0, on && t > len - 200u ? g_mix(bg, C_TXT, (uint32_t)(len - t)) : bg);
    g_frame(4, VIS_Y0 + 4, 232, 240 - VIS_Y0 - 8, g_mix(bg, cy, 120));
    g_rect(4 + (int32_t)(t / 3u % 232u), VIS_Y0 + 5, 2, 240 - VIS_Y0 - 10, g_mix(bg, cy, 60));   /* the scan */
    g_text(10, VIS_Y0 + 8, "XENOISE SYS // MK-I", g_mix(bg, cy, 150), T_CROP);
    g_text_c(120, y0, on ? "ENGAGING" : "DISENGAGING", hot, T_BOLD);
    g_text_c(120, y0 + 18, on ? "ALL RANGE PROTOCOL" : "ALL RANGE MODE", (t / 120u) & 1u ? C_TXT : hot, T_CROP | T_BOLD);
    for (k = 0; k < n; k++)
        if (t > 300u + k * (len - 500u) / n) g_text(14, y0 + 40 + (int32_t)k * 14, on ? L1[k] : L0[k], k + 1u == n ? hot : cy, T_CROP);
    g_rect(20, 214, 200, 6, g_mix(bg, cy, 50));
    g_rect(20, 214, on ? f : 200 - f, 6, hot);
}
static void vis_draw(void)
{
    const uint16_t sky = C_BG, grid = g_mix(C_BG, C_BLUE, 60), hud = C_BLUE, foe = C_ORANGE, amber = C_CYAN;
    int32_t sx = 0, sy = 0, k, x, y;
    uint32_t i, pan = (uint32_t)vis_yaw * 960u >> 16;   /* (the far hills and stars: a turn, 960 px of them) */
    char s[32];
    if (vis_ms - vis_shake_ms < 120u) { sx = (int32_t)((vis_ms / 16u) % 5u) - 2; sy = (int32_t)((vis_ms / 22u) % 3u) - 1; }
    g_rect(0, VIS_Y0, 240, 240 - VIS_Y0, sky);
    for (i = 0; i < 26u; i++) {                 /* stars, and the hills far off */
        uint32_t h = (i + 1u) * 2654435761u;
        g_px((int32_t)((h + 960u - pan) % 240u) + sx, VIS_Y0 + 2 + (int32_t)((h >> 12) % 60u), g_mix(sky, C_TXT, 60 + (h >> 28) * 10u));
    }
    for (x = 0; x < 240; x += 8) {
        int32_t wx = (int32_t)(((uint32_t)x + pan) % 960u), hh = 4 + ((wx * 7 + 13) % 11) + (xj_sin((uint32_t)wx * 400u) >> 13);
        g_rect(x + sx, VIS_HOR - hh + sy, 8, hh, g_mix(sky, hud, 22));
    }
    g_rect(0, VIS_HOR + sy, 240, 1, g_mix(sky, hud, 120));
    for (k = 1; k < 24; k++) {                  /* the ground: its lines coming at the mech, at the tempo */
        int32_t z = k * 64 - (int32_t)(vis_scroll % 64u);
        if (z < 24) continue;
        y = VIS_HOR + VIS_CAMH * VIS_F / z + sy;
        g_rect(0, y, 240, 1, g_mix(sky, grid, (uint32_t)clampi(256 - z / 6, 30, 256)));
    }
    if (!vis_ar)
        for (x = -480; x <= 480; x += 96)
            g_line(120 + x * VIS_F / VIS_ZFAR + sx, VIS_HOR + 4 + sy, 120 + x * VIS_F / 48 + sx, VIS_HOR + VIS_CAMH * VIS_F / 48 + sy, grid);
    else
        for (x = 0; x < 240; x += 30) {         /* ALL RANGE: the ground's lines slide as it turns */
            int32_t gx = x + 29 - (int32_t)(pan * 4u % 30u);
            g_line(gx + sx, VIS_HOR + 4 + sy, 120 + (gx - 120) * 6 + sx, 240 + sy, grid);
        }
    for (k = 0; k < 2; k++)                     /* the enemies, far first */
        for (i = 0; i < VIS_NE; i++) {
            const vis_en_t *e = &vis_en[i];
            int lit;
            uint16_t f;
            int32_t vx, vz;
            if (!e->on) continue;
            vis_turn(e->x, e->z, &vx, &vz);
            if ((vz > VIS_ZFAR / 2) != (k == 0)) continue;
            lit = e->hit_ms && vis_ms >= e->hit_ms && vis_ms - e->hit_ms < 90u;
            f = g_mix(sky, foe, (uint32_t)clampi(256 - vz / 8, 70, 200));
            vis_enemy(e, lit ? C_TXT : f, lit ? C_TXT : g_mix(f, foe, 120), lit ? C_TXT : amber);
        }
    for (i = 0; i < VIS_NB; i++) {              /* explosions: a fireball, a ring, debris */
        const vis_boom_t *b = &vis_bm[i];
        int32_t px, py, sc, age = (int32_t)(vis_ms - b->t0), r;
        if (!b->on || age < 0 || !vis_proj(b->x, b->h, b->z, &px, &py, &sc)) continue;
        r = (sc * (b->big ? 40 : 16) >> 8) * (age + 60) / 300 + 3;
        if (age < 500) {
            uint16_t c = age < 80 ? C_TXT : g_mix(amber, foe, (uint32_t)clampi(age / 2, 0, 256));
            g_rect(px - r / 2 + sx, py - r / 2 + sy, r, r, c);
        }
        for (k = 0; k < (b->big ? 8 : 4); k++) {
            uint32_t a = (uint32_t)k * 8192u + i * 3000u;
            int32_t d = r + age / 6;
            g_rect(px + sx + d * xj_sin(a + 0x4000u) / 32768, py + sy + d * xj_sin(a) / 32768 + age * age / 9000, 2, 2,
                   age < 600 ? amber : g_mix(sky, foe, 100));
        }
        if (b->big && age < 300) {
            int32_t q = r + age / 3;
            g_line(px - q + sx, py + sy, px + q + sx, py + sy, C_TXT);
        }
    }
    for (i = 0; i < VIS_NS; i++) {              /* the rounds: tracers from the guns, a missile's smoke, a laser */
        const vis_shot_t *sh = &vis_sh[i];
        int32_t px, py, sc, mx, my, age = (int32_t)(vis_ms - sh->t0), j;
        if (!sh->on || !vis_proj(sh->x, sh->h, sh->z, &px, &py, &sc)) continue;
        mx = sh->kind == VW_LEFT ? 26 : sh->kind == VW_RIGHT ? 214 : sh->kind == VW_MISSILE ? 200 : 120;
        my = sh->kind == VW_CHIN ? 214 : sh->kind == VW_MISSILE ? 172 : 196;
        px += sx; py += sy;
        if (sh->kind == VW_LASER) {
            if (age < 120) { g_line(100, 200, px, py, hud); g_line(140, 200, px, py, hud); g_line(101, 200, px, py, C_TXT); }
        } else if (sh->kind == VW_MISSILE) {
            for (j = 0; j < 10; j++) {
                int32_t f = clampi(age * 256 / 260 - j * 14, 0, 256), cx = mx + (px - mx) * f / 256, cy = my + (py - my) * f / 256 - (f * (256 - f) >> 9);
                g_rect(cx - 1, cy - 1, 3, 3, j ? g_mix(sky, C_MUTED, (uint32_t)(220 - j * 18)) : amber);
            }
        } else if (age < 130) {
            int32_t f0 = age * 256 / 130, f1 = clampi(f0 + 70, 0, 256);
            g_line(mx + (px - mx) * f0 / 256, my + (py - my) * f0 / 256, mx + (px - mx) * f1 / 256, my + (py - my) * f1 / 256,
                   sh->kind == VW_CHIN ? amber : C_TXT);
        }
    }
    {                                           /* the cockpit: the canopy's struts, the guns, the dash */
        uint16_t frame = g_mix(sky, hud, 40), dark = g_mix(sky, C_PANEL, 120);
        for (k = 0; k < 4; k++) {
            g_line(k, VIS_Y0, 34 + k, 240, frame);
            g_line(239 - k, VIS_Y0, 205 - k, 240, frame);
        }
        g_line(0, VIS_Y0 + 18, 240, VIS_Y0 + 18, g_mix(sky, hud, 30));
        for (k = 0; k < 2; k++) {               /* the cannons, a flash at the muzzle */
            int32_t gx = k ? 202 : 14, fl = vis_ms - vis_fire_ms[k ? VW_RIGHT : VW_LEFT] < 70u;
            g_rect(gx, 192, 24, 30, dark);
            g_rect(gx + 6, 182, 12, 12, dark);
            g_rect(gx + 8, 184, 8, 3, g_mix(dark, hud, 120));
            if (fl) { g_rect(gx + 4, 174, 16, 9, C_TXT); g_rect(gx + 1, 177, 22, 3, amber); }
        }
        g_rect(40, 222, 160, 18, dark);
        g_rect(40, 222, 160, 1, g_mix(dark, hud, 150));
        if (vis_ms - vis_fire_ms[VW_CHIN] < 60u) g_rect(114, 210, 12, 6, amber);
    }
    {                                           /* the HUD: sight, wave, kills, shield, radar, power */
        int32_t cx = 120, cy = VIS_HOR - 6, open = vis_ms - vis_fire_ms[VW_LEFT] < 90u || vis_ms - vis_fire_ms[VW_RIGHT] < 90u ? 3 : 0;
        g_rect(cx - 14 - open, cy, 8, 1, hud); g_rect(cx + 7 + open, cy, 8, 1, hud);
        g_rect(cx, cy - 12 - open, 1, 7, hud); g_rect(cx, cy + 6 + open, 1, 7, hud);
        g_rect(cx, cy, 1, 1, foe);
        s[0] = 0;
        fmt_u(str_cat(s, "WAVE "), vis_wave % 1000u, 3);
        g_text(8, VIS_Y0 + 3, s, hud, T_CROP);
        g_text(84, VIS_Y0 + 3, vis_section(), amber, T_CROP | T_BOLD);
        s[0] = 0;
        fmt_u(s, vis_kills % 10000u, 4);
        g_text(202, VIS_Y0 + 3, s, foe, T_CROP | T_BOLD);
        g_text(156, VIS_Y0 + 3, "KILLS", g_mix(sky, hud, 150), T_CROP);
        g_rect(44, 226, 52, 6, g_mix(sky, hud, 50));   /* the shield */
        g_rect(44, 226, 52 * vis_shield / 100, 6, vis_shield < 30 && (vis_ms / 200u) & 1u ? foe : hud);
        {
            int32_t rx = 120, ry = 231, a = 0;   /* the radar: what comes, as dots */
            g_rect(rx - 22, ry - 8, 44, 1, g_mix(sky, hud, 80));
            for (i = 0; i < VIS_NE; i++) if (vis_en[i].on) {
                int32_t vx, vz;
                vis_turn(vis_en[i].x, vis_en[i].z, &vx, &vz);
                if (vz < 0 && !vis_ar) continue;
                g_rect(rx + clampi(vx * 22 / (vis_ar ? VIS_ZFAR : 480), -22, 21), ry - 1 - clampi(vz * 7 / VIS_ZFAR, -6, 7), 2, 2, foe);
            }
            for (i = 0; i < 120u; i += 6u) { int32_t v = scope[(scope_w + i) % 120u]; a += v < 0 ? -v : v; }
            a = clampi(a / 20 / 60, 0, 10);       /* the power: the output's level */
            for (k = 0; k < 10; k++) g_rect(150 + k * 4, 226, 3, 6, k < a ? (k > 7 ? foe : amber) : g_mix(sky, hud, 40));
        }
        if (vis_ms - vis_dmg_ms < 250u) {       /* a hit on the mech: red at the edges */
            g_rect(0, VIS_Y0, 240, 3, foe); g_rect(0, 237, 240, 3, foe);
            g_rect(0, VIS_Y0, 3, 240 - VIS_Y0, foe); g_rect(237, VIS_Y0, 3, 240 - VIS_Y0, foe);
        }
        if (!sq_play && !vis_ar && (vis_ms / 500u) & 1u) g_text_c(120, VIS_HOR + 30, "STANDBY - PLAY TO ENGAGE", amber, T_CROP);
        if (vis_ar == 2u) {                     /* ALL RANGE: a compass */
            static const char *const CP[4] = {"N", "E", "S", "W"};
            for (k = 0; k < 4; k++) {
                int32_t cx2 = 146 + (int32_t)((int16_t)((uint16_t)(k * 0x4000) - vis_yaw)) * 200 / 65536;
                if (cx2 > 96 && cx2 < 196) g_text_c(cx2, VIS_Y0 + 16, CP[k], k ? hud : foe, T_CROP | T_BOLD);
            }
            g_rect(96, VIS_Y0 + 29, 100, 1, g_mix(sky, hud, 90));
            g_rect(145, VIS_Y0 + 27, 2, 4, foe);
            g_text(8, VIS_Y0 + 16, "ALL RANGE", amber, T_CROP);
        }
    }
    if (vis_ar == 1u || vis_ar == 3u) vis_boot(vis_ar == 1u);
}
