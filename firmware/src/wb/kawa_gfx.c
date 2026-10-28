/* SPDX-License-Identifier: GPL-3.0-only */
/* KAWA: drawing. The screen is drawn in six bands of 240 x 40: each band is drawn whole (everything
 * outside it is clipped), hashed, and sent only if it changed (12 MHz SPI: a full screen is ~77 ms).
 * Two send buffers take turns, so a band is drawn while the last one goes out. Colours are RGB565
 * in the drawing buffer, byte-swapped for the panel on the way out. */
#define BAND_H 40u
#define NBAND 6u
/* (XENOISE) the band being drawn and the two send buffers, all three in JIANT's cv_px (gfx.c, 72 KiB): no RAM of their own */
#define g_buf (cv_px + 2u * 240u * BAND_H)
#define g_txb (cv_px + 240u * BAND_H)
typedef char wb_bands_fit[3u * 240u * BAND_H <= CV_MAX ? 1 : -1];
static int32_t g_y0;                            /* the band's first row */
static uint32_t g_hash[NBAND];
static uint8_t g_force = 1, g_tx;

static int g_vis(int32_t y, int32_t h) { return y < g_y0 + (int32_t)BAND_H && y + h > g_y0; }

static uint16_t g_mix(uint16_t a, uint16_t b, uint32_t t)   /* a .. b, t = 0..256 */
{
    uint32_t ar = a >> 11, ag = (a >> 5) & 63u, ab = a & 31u, br = b >> 11, bg = (b >> 5) & 63u, bb = b & 31u;
    ar = (ar * (256u - t) + br * t) >> 8;
    ag = (ag * (256u - t) + bg * t) >> 8;
    ab = (ab * (256u - t) + bb * t) >> 8;
    return (uint16_t)(ar << 11 | ag << 5 | ab);
}

static void g_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h, i;
    if (x < 0) x = 0;
    if (x1 > 240) x1 = 240;
    if (y < g_y0) y = g_y0;
    if (y1 > g_y0 + (int32_t)BAND_H) y1 = g_y0 + (int32_t)BAND_H;
    for (; y < y1; y++) {
        uint16_t *p = &g_buf[(uint32_t)(y - g_y0) * 240u];
        for (i = x; i < x1; i++)
            p[i] = c;
    }
}

static void g_blend(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t t)
{
    int32_t x1 = x + w, y1 = y + h, i;
    if (x < 0) x = 0;
    if (x1 > 240) x1 = 240;
    if (y < g_y0) y = g_y0;
    if (y1 > g_y0 + (int32_t)BAND_H) y1 = g_y0 + (int32_t)BAND_H;
    for (; y < y1; y++) {
        uint16_t *p = &g_buf[(uint32_t)(y - g_y0) * 240u];
        for (i = x; i < x1; i++)
            p[i] = g_mix(p[i], c, t);
    }
}

static void g_px(int32_t x, int32_t y, uint16_t c)
{
    y -= g_y0;
    if ((uint32_t)x < 240u && (uint32_t)y < BAND_H)
        g_buf[(uint32_t)y * 240u + (uint32_t)x] = c;
}

/* (XENOISE) the shapes smooth: a pixel's share of the colour (t 0..256) blends it in, so corners, circles and lines
 * have soft edges as the text has (the same places as before: only the edges' pixels change) */
static void g_apx(int32_t x, int32_t y, uint16_t c, uint32_t t)
{
    y -= g_y0;
    if ((uint32_t)x < 240u && (uint32_t)y < BAND_H && t)
        g_buf[(uint32_t)y * 240u + (uint32_t)x] = t >= 256u ? c : g_mix(g_buf[(uint32_t)y * 240u + (uint32_t)x], c, t);
}

static void g_rrect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* corners rounded (a 2 px radius) */
{
    if (!g_vis(y, h))
        return;
    g_rect(x + 2, y, w - 4, 1, c);
    g_rect(x + 1, y + 1, w - 2, 1, c);
    g_rect(x, y + 2, w, h - 4, c);
    g_rect(x + 1, y + h - 2, w - 2, 1, c);
    g_rect(x + 2, y + h - 1, w - 4, 1, c);
    g_apx(x + 1, y, c, 120); g_apx(x + w - 2, y, c, 120); g_apx(x, y + 1, c, 120); g_apx(x + w - 1, y + 1, c, 120);
    g_apx(x + 1, y + h - 1, c, 120); g_apx(x + w - 2, y + h - 1, c, 120); g_apx(x, y + h - 2, c, 120); g_apx(x + w - 1, y + h - 2, c, 120);
}

static void g_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)  /* a rounded outline */
{
    if (!g_vis(y, h))
        return;
    g_rect(x + 2, y, w - 4, 1, c);
    g_rect(x + 2, y + h - 1, w - 4, 1, c);
    g_rect(x, y + 2, 1, h - 4, c);
    g_rect(x + w - 1, y + 2, 1, h - 4, c);
    g_apx(x + 1, y + 1, c, 200); g_apx(x + w - 2, y + 1, c, 200);           /* the corners: a soft step */
    g_apx(x + 1, y + h - 2, c, 200); g_apx(x + w - 2, y + h - 2, c, 200);
    g_apx(x + 1, y, c, 90); g_apx(x + w - 2, y, c, 90); g_apx(x, y + 1, c, 90); g_apx(x + w - 1, y + 1, c, 90);
    g_apx(x + 1, y + h - 1, c, 90); g_apx(x + w - 2, y + h - 1, c, 90); g_apx(x, y + h - 2, c, 90); g_apx(x + w - 1, y + h - 2, c, 90);
}

static void g_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* (XENOISE) Wu's: two pixels a step */
{
    int32_t dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, i, n;
    if (y0 > g_y0 + (int32_t)BAND_H + 1 && y1 > g_y0 + (int32_t)BAND_H + 1) return;
    if (y0 < g_y0 - 1 && y1 < g_y0 - 1) return;
    if (!adx && !ady) { g_px(x0, y0, c); return; }
    if (adx >= ady) {                           /* along x: y in 1/256 */
        int32_t y = y0 << 8, step = (dy << 8) / (adx ? adx : 1), sx = dx < 0 ? -1 : 1;
        n = adx > 2000 ? 2000 : adx;
        for (i = 0; i <= n; i++, y += step) {
            uint32_t f = (uint32_t)(y & 255);
            g_apx(x0 + i * sx, y >> 8, c, 256u - f);
            g_apx(x0 + i * sx, (y >> 8) + 1, c, f);
        }
    } else {                                    /* along y */
        int32_t x = x0 << 8, step = (dx << 8) / ady, sy = dy < 0 ? -1 : 1;
        n = ady > 2000 ? 2000 : ady;
        for (i = 0; i <= n; i++, x += step) {
            uint32_t f = (uint32_t)(x & 255);
            g_apx(x >> 8, y0 + i * sy, c, 256u - f);
            g_apx((x >> 8) + 1, y0 + i * sy, c, f);
        }
    }
}

/* (XENOISE) a filled circle, its edge smooth: cx, cy, r in 1/16 px (each pixel's centre against the radius) */
static void g_disc(int32_t cx16, int32_t cy16, int32_t r16, uint16_t c)
{
    int32_t x0 = (cx16 - r16) >> 4, x1 = (cx16 + r16) >> 4, y0 = (cy16 - r16) >> 4, y1 = (cy16 + r16) >> 4, x, y;
    if (!g_vis(y0, y1 - y0 + 1))
        return;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            int32_t ddx = x * 16 + 8 - cx16, ddy = y * 16 + 8 - cy16, d2 = ddx * ddx + ddy * ddy, in = r16 - 8, out = r16 + 8;
            if (d2 <= in * in) g_apx(x, y, c, 256u);
            else if (d2 < out * out) {          /* the edge: the distance past the radius, as coverage */
                int32_t d = 0, b;
                for (b = 1 << 10; b; b >>= 1) if ((d + b) * (d + b) <= d2) d += b;
                g_apx(x, y, c, (uint32_t)((out - d) * 16));
            }
        }
}

/* the screen's font: (XENOISE) CLAIRE alone (WND-1 offered BASE, DOUCE, CLAIRE, 8-BIT: OPTIONS, KNOB 4; the others
 * cost 37 KiB of flash); each a small one (8 x 16) and a large one (16 x 32) */
#define NFONT 4u
enum { FN_BASE, FN_DOUCE, FN_CLAIRE, FN_8BIT };
static const felucca_font_t *g_fs = &FONT_S_CLAIRE, *g_fsb = &FONT_SB_CLAIRE, *g_fl = &FONT_L_CLAIRE;
static uint8_t g_font;
static void g_font_set(uint32_t k) { g_font = (uint8_t)(k < NFONT ? k : 0u); }

/* text in the small font (8 x 16): T_CROP draws rows 2..13 only (12 px: capitals, digits), T_BOLD in the SemiBold
 * face (XENOISE: it was the strokes drawn twice, a pixel apart); the glyph's grey levels blend into what is under it.
 * Returns the x after the text. */
enum { T_CROP = 1, T_BOLD = 2 };
static int32_t g_text(int32_t x, int32_t y, const char *s, uint16_t c, uint32_t mode)
{
    const felucca_font_t *f = mode & T_BOLD ? g_fsb : g_fs;
    int32_t r0 = mode & T_CROP ? 2 : 0, r1 = mode & T_CROP ? 14 : 16;
    if (!g_vis(y, r1 - r0)) {
        while (*s) { x += 8; s++; }
        return x;
    }
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), w = f->bw[gi], bpr = (w + 1u) / 2u, gx;
        const uint8_t *gd = f->data + f->off[gi];
        int32_t gy;
        for (gy = r0; gy < r1; gy++) {
            int32_t py = y + gy - r0 - g_y0;
            uint16_t *row;
            if ((uint32_t)py >= BAND_H)
                continue;
            row = &g_buf[(uint32_t)py * 240u];
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[(uint32_t)gy * bpr + gx / 2u];
                int32_t px = x - (int32_t)f->pad + (int32_t)gx;
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (!v)
                    continue;
                if ((uint32_t)px < 240u)
                    row[px] = v >= 15u ? c : g_mix(row[px], c, v * 17u);
            }
        }
        x += 8;
    }
    return x;
}
static int32_t g_text_w(const char *s) { return (int32_t)str_len(s) * 8; }
static void g_text_c(int32_t cx, int32_t y, const char *s, uint16_t c, uint32_t mode)   /* centred on cx */
{
    g_text(cx - g_text_w(s) / 2, y, s, c, mode);
}

/* the large font: 16 x 32 (capitals, digits) */
static int32_t g_text_l(int32_t x, int32_t y, const char *s, uint16_t c)
{
    const felucca_font_t *f = g_fl;
    if (!g_vis(y, 32)) {
        while (*s) { x += 16; s++; }
        return x;
    }
    for (; *s; s++) {
        uint32_t gi = glyph(f, (uint8_t)*s), w = f->bw[gi], bpr = (w + 1u) / 2u, gx, gy;
        const uint8_t *gd = f->data + f->off[gi];
        for (gy = 0; gy < f->h; gy++) {
            int32_t py = y + (int32_t)gy - g_y0;
            uint16_t *row;
            if ((uint32_t)py >= BAND_H)
                continue;
            row = &g_buf[(uint32_t)py * 240u];
            for (gx = 0; gx < w; gx++) {
                uint32_t v = gd[gy * bpr + gx / 2u];
                int32_t px = x - (int32_t)f->pad + (int32_t)gx;
                v = (gx & 1u) ? (v & 15u) : (v >> 4);
                if (v && (uint32_t)px < 240u)
                    row[px] = v >= 15u ? c : g_mix(row[px], c, v * 17u);
            }
        }
        x += f->adv[gi];
    }
    return x;
}

static void g_icon(int32_t x, int32_t y, uint32_t id, uint16_t c, uint32_t scale)
{
    uint32_t r, k;
    if (id >= ICO_N || !g_vis(y, (int32_t)(8u * scale)))
        return;
    for (r = 0; r < 8u; r++)
        for (k = 0; k < 8u; k++)
            if (ICONS[id][r] >> (7u - k) & 1u) {
                if (scale == 1u)
                    g_px(x + (int32_t)k, y + (int32_t)r, c);
                else
                    g_rect(x + (int32_t)(k * scale), y + (int32_t)(r * scale), (int32_t)scale, (int32_t)scale, c);
            }
}

/* a picture: w x h pixels, a byte each into pal (RGB565), at 1x (the boot's): the rows of this band only */
static void g_pic(int32_t x, int32_t y, const uint8_t *img, uint32_t w, uint32_t h, const uint16_t *pal)
{
    int32_t r = g_y0 - y, r1 = r + (int32_t)BAND_H, k0 = x < 0 ? -x : 0, k1 = x + (int32_t)w > 240 ? 240 - x : (int32_t)w, k;
    if (r < 0) r = 0;
    if (r1 > (int32_t)h) r1 = (int32_t)h;
    for (; r < r1; r++) {
        const uint8_t *s = img + (uint32_t)r * w;
        uint16_t *d = &g_buf[(uint32_t)(y + r - g_y0) * 240u + (uint32_t)x];
        for (k = k0; k < k1; k++)
            d[k] = pal[s[k]];
    }
}

/* the dog: w x h pixels at 4 bits (the low nibble is the left pixel), ink 0 clear, pal[ink] for the
 * others; flip mirrors it */
static void g_dog(int32_t x, int32_t y, const uint8_t *img, uint32_t w, uint32_t h, const uint16_t *pal,
                  uint32_t scale, uint32_t flip)
{
    uint32_t r, k;
    if (!g_vis(y, (int32_t)(h * scale)))
        return;
    for (r = 0; r < h; r++) {
        int32_t yy = y + (int32_t)(r * scale);
        if (!g_vis(yy, (int32_t)scale))
            continue;
        for (k = 0; k < w; k++) {
            uint32_t sk = flip ? w - 1u - k : k, b = img[(r * w + sk) >> 1], ink = sk & 1u ? b >> 4 : b & 15u;
            if (!ink)
                continue;
            if (scale == 1u)
                g_px(x + (int32_t)k, yy, pal[ink]);
            else
                g_rect(x + (int32_t)(k * scale), yy, (int32_t)scale, (int32_t)scale, pal[ink]);
        }
    }
}

/* a number into text (no printf on the chip) */
static char *fmt_u(char *p, uint32_t v, uint32_t digits)   /* digits 0: as many as needed */
{
    char t[12];
    uint32_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v && n < 11u);
    while (n < digits) t[n++] = '0';
    while (n) *p++ = t[--n];
    *p = 0;
    return p;
}
static char *fmt_s(char *p, int32_t v)          /* with its sign */
{
    *p++ = v < 0 ? '-' : '+';
    return fmt_u(p, (uint32_t)(v < 0 ? -v : v), 0);
}
static char *str_cat(char *p, const char *s)   /* append s to the string at p; returns its new end */
{
    while (*p) p++;
    while (*s) *p++ = *s++;
    *p = 0;
    return p;
}

/* one frame: draw(band) for each band, send what changed; poll in between (USB) */
static void g_frame_send(void (*draw)(void))
{
    uint32_t b;
    for (b = 0; b < NBAND; b++) {
        uint32_t h = 2166136261u, i, *w = (uint32_t *)g_buf;
        uint16_t *tx;
        g_y0 = (int32_t)(b * BAND_H);
        draw();
        for (i = 0; i < 240u * BAND_H / 2u; i++)
            h = (h ^ w[i]) * 16777619u;
        if (h == g_hash[b] && !g_force) {
            platform_poll();
            continue;
        }
        g_hash[b] = h;
        tx = g_tx ? g_txb : cv_px;              /* the other buffer than the one in flight (lcd_blit waits for */
        g_tx ^= 1u;                             /* the one before it: this one is free) */
        for (i = 0; i < 240u * BAND_H; i++) {
            uint16_t c = g_buf[i];
            tx[i] = (uint16_t)(c >> 8 | c << 8);
        }
        lcd_blit(0, b * BAND_H, 240, BAND_H, tx);
        platform_poll();
    }
    g_force = 0;
}
