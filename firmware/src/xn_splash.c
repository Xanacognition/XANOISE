/* SPDX-License-Identifier: GPL-3.0-only */
/* XENOISE: the power-on splash, a mecha's boot into combat mode (ui_draw.c's draw_splash / splash_write under XN_WB, in
 * place of JIANT's signature; main.c's splash loop and the emulator's run it as they ran that). All from ms, nothing kept:
 *   0 ..       the header, its rule drawn out; the boot log typed at the bottom, a line at a time; a scan line falling
 *   250 ..     the HUD's corner brackets grow in; the targeting reticle closes in from twice its size, its ticks turning
 *   1050 ..    the name, in block letters, slices in (each row of cells thrown sideways, the throw dying away), a
 *              magenta ghost behind it; a white flash as it locks
 *   1500 ..    the log gives way to the banner: COMBAT MODE ENGAGED on hazard stripes, blinking, then steady; a shine
 *              runs over the name
 * The screen in two halves of 240 x 120 (the canvas holds 150 rows): the scene drawn into each with cv_oy. Its own
 * colours the theme's (xn_themes.h: its ground, its accents, its text), as the screen will be */

#include "xn_themes.h"
#define XS_END 2000u                                /* ms: the scene whole (main.c then holds it SPL_HOLD) */
static uint16_t xs_col[5];                          /* the theme's (xs_splash): its ground, what is picked, what plays, */
#define XS_BG xs_col[0]                             /* its third accent, its text */
#define XS_CY xs_col[1]
#define XS_MAG xs_col[2]
#define XS_AMB xs_col[3]
#define XS_WH xs_col[4]

static const char *const XS_LOG[] = {
    "> POWER CELL ........ OK", "> FM CORES x6 ....... OK", "> SEQUENCER ......... OK",
    "> NEURAL LINK ....... OK", "> TARGETING ......... OK", "> AUDIO WEAPONS ..... ARMED",
};
#define XS_NLOG (sizeof XS_LOG / sizeof XS_LOG[0])
/* the name's block letters, 5 x 7 cells, a row a byte (bit 4 the left) */
static const uint8_t XS_GLYPH[6][7] = {
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},     /* X */
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},     /* E */
    {0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11},     /* N */
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},     /* O */
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F},     /* I */
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},     /* S */
};
static const uint8_t XS_NAME[7] = {0, 1, 2, 3, 4, 5, 1};   /* X E N O I S E */
#define XS_CW 5                                     /* a cell: 5 x 6 px, drawn 4 x 5 (the seams of plating) */
#define XS_CH 6
#define XS_TX ((240 - (7 * 5 * XS_CW + 6 * 6)) / 2)
#define XS_TY 79
#define XS_RX 120                                   /* the reticle's centre */
#define XS_RY 100

static int32_t xs_ramp(uint32_t t, uint32_t a, uint32_t d)   /* 0 before a .. 256 at a + d */
{
    return t <= a ? 0 : t >= a + d ? 256 : (int32_t)((t - a) * 256u / d);
}
static void xs_circle(int32_t cx, int32_t cy, int32_t r, uint32_t from, uint32_t span, uint16_t c)   /* Q4; an arc */
{
    int32_t i, px = 0, py = 0;
    for (i = 0; i <= 32; i++) {
        uint32_t th = from + span * (uint32_t)i / 32u;
        int32_t x = cx + (r * bg_sin(th + 0x4000u) >> 15), y = cy + (r * bg_sin(th) >> 15);
        if (i)
            og_line(px, py, x, y, c);
        px = x;
        py = y;
    }
}
static void xs_text(int32_t x, int32_t y, const char *s, uint32_t n, uint16_t c)   /* the first n characters of s */
{
    char b[40];
    uint32_t k;
    for (k = 0; k < n && s[k] && k + 1u < sizeof b; k++)
        b[k] = s[k];
    b[k] = 0;
    cv_text(x, y, &AF_S, b, c);
}

static void xs_scene(uint32_t t)
{
    int32_t a, k, i;
    uint32_t r;
    /* the flash as the name locks (1480 .. 1580): the whole screen white, fading */
    if (t >= 1480u && t < 1580u)
        cv_rect(0, 0, 240, 240, ux_mix(XS_WH, XS_BG, (int32_t)(t - 1480u)));
    /* the header and its rule */
    a = xs_ramp(t, 0, 300);
    cv_text(6, 4, &AF_S, "XENOISE SYS // MK-I", ux_mix(XS_BG, XS_CY, 40 + a * 60 / 256));
    {
        char tt[12] = "T+0.00";
        uint32_t cs = (t < XS_END ? t : XS_END) / 10u;
        tt[2] = (char)('0' + cs / 100u % 10u);
        tt[4] = (char)('0' + cs / 10u % 10u);
        tt[5] = (char)('0' + cs % 10u);
        cv_text_r(234, 4, &AF_S, tt, ux_mix(XS_BG, XS_AMB, 80), XS_BG);
    }
    cv_rect(0, 20, 240 * a / 256, 1, ux_mix(XS_BG, XS_CY, 55));
    /* the scan line, falling through the first 700 ms */
    if (t < 700u)
        cv_rect(0, 22 + (int32_t)(t * 214u / 700u), 240, 1, ux_mix(XS_BG, XS_CY, 35));
    /* the corner brackets */
    a = xs_ramp(t, 250, 250);
    {
        int32_t L = 4 + 24 * a / 256, y0 = 26, y1 = 170;
        uint16_t c = ux_mix(XS_BG, XS_CY, 85);
        if (a) {
            cv_rect(6, y0, L, 2, c); cv_rect(6, y0, 2, L, c);
            cv_rect(234 - L, y0, L, 2, c); cv_rect(232, y0, 2, L, c);
            cv_rect(6, y1 - 2, L, 2, c); cv_rect(6, y1 - L, 2, L, c);
            cv_rect(234 - L, y1 - 2, L, 2, c); cv_rect(232, y1 - L, 2, L, c);
        }
    }
    /* the reticle: closing in, its ticks turning; dimmed behind the name */
    a = xs_ramp(t, 350, 500);
    if (a) {
        int32_t sc = 512 - a, dim = t > 1050u ? 100 - xs_ramp(t, 1050, 300) * 60 / 256 : 100;
        int32_t R1 = 58 * 16 * sc / 256, R2 = 40 * 16 * sc / 256, cx = XS_RX * 16, cy = XS_RY * 16;
        uint16_t c1 = ux_mix(XS_BG, XS_CY, dim * (40 + a * 50 / 256) / 100), c2 = ux_mix(XS_BG, XS_MAG, dim * a * 70 / 256 / 100);
        uint32_t rot = t * 30u;
        xs_circle(cx, cy, R1, rot, 0x5000u, c1);   /* the outer ring in four arcs, turning */
        xs_circle(cx, cy, R1, rot + 0x8000u, 0x5000u, c1);
        xs_circle(cx, cy, R2, 0u - rot * 2u, 0x3000u, c2);   /* (angles: 16 bits, round) */
        xs_circle(cx, cy, R2, 0x8000u - rot * 2u, 0x3000u, c2);
        for (k = 0; k < 12; k++) {                  /* the ticks */
            uint32_t th = (uint32_t)k * 65536u / 12u - rot / 2u;
            int32_t ox = bg_sin(th + 0x4000u), oy = bg_sin(th);
            og_line(cx + (R1 + 40) * ox / 32768, cy + (R1 + 40) * oy / 32768, cx + (R1 + 110) * ox / 32768,
                    cy + (R1 + 110) * oy / 32768, c1);
        }
        {                                           /* the crosshair, growing out of the centre */
            int32_t g = 16 * (8 + 70 * xs_ramp(t, 500, 300) / 256);
            og_line(cx - g, cy, cx - 16 * 10, cy, c1); og_line(cx + 16 * 10, cy, cx + g, cy, c1);
            og_line(cx, cy - g * 7 / 10, cx, cy - 16 * 10, c1); og_line(cx, cy + 16 * 10, cx, cy + g * 7 / 10, c1);
        }
        if (t < 1050u && (t / 120u) & 1u)           /* the lock: a blinking pip */
            cv_rect(XS_RX - 2, XS_RY - 2, 5, 5, XS_MAG);
    }
    /* the name: each cell row thrown sideways, the throw dying; a magenta ghost; then a shine */
    if (t >= 1050u) {
        int32_t amp = 30 - 30 * xs_ramp(t, 1050, 430) / 256, shine = t > 1600u ? (int32_t)(t - 1600u) * 300 / 400 - 30 : -999;
        for (r = 0; r < 7u; r++) {
            uint32_t h = (r + 1u) * 2654435761u ^ (t / 40u) * 40503u;
            int32_t off = amp ? (int32_t)((h >> 16) % (uint32_t)(2 * amp + 1)) - amp : 0;
            for (i = 0; i < 7; i++) {
                uint8_t bits = XS_GLYPH[XS_NAME[i]][r];
                for (k = 0; k < 5; k++) {
                    int32_t x, y;
                    if (!(bits & (0x10u >> k)))
                        continue;
                    x = XS_TX + i * (5 * XS_CW + 6) + k * XS_CW + off;
                    y = XS_TY + (int32_t)r * XS_CH;
                    if (amp > 2)
                        cv_rect(x + 3 + amp / 4, y, XS_CW - 1, XS_CH - 1, ux_mix(XS_BG, XS_MAG, 70));
                    cv_rect(x, y, XS_CW - 1, XS_CH - 1, x >= shine && x < shine + 14 ? XS_WH : XS_CY);
                }
            }
        }
        a = xs_ramp(t, 1500, 250);                  /* the line under it */
        cv_rect(120 - 100 * a / 256, XS_TY + 7 * XS_CH + 6, 200 * a / 256, 1, ux_mix(XS_BG, XS_MAG, 80));
        if (a)
            cv_text_in(0, XS_TY + 7 * XS_CH + 12, 240, &AF_S, "MICRO-JAM COMBAT UNIT", ux_mix(XS_BG, XS_CY, a * 70 / 256), XS_BG);
    }
    /* the boot log (till the banner): the last four lines, each typed */
    if (t < 1500u) {
        uint32_t n = t < 150u ? 0u : (t - 150u) / 150u + 1u, top = n > 4u ? n - 4u : 0u;
        for (r = top; r < n && r < XS_NLOG; r++) {
            uint32_t chars = (t - 150u - r * 150u) / 6u;
            const char *s = XS_LOG[r];
            int last = r + 1u == XS_NLOG;
            xs_text(8, 176 + (int32_t)(r - top) * 15, s, chars, last ? XS_AMB : ux_mix(XS_BG, XS_CY, r + 1u == n ? 90 : 55));
        }
    } else {                                        /* the banner: hazard stripes, the words blinking, then steady */
        int32_t y0 = 184, h = 34, x, y, w = 172;
        uint32_t ph = t / 50u;
        for (y = 0; y < h; y++)                     /* (diagonal, moving: 8 px of amber in every 16) */
            for (x = -((y + (int32_t)ph * 2) & 15); x < 240; x += 16)
                cv_rect(x, y0 + y, 8, 1, XS_AMB);
        cv_rect(120 - w / 2, y0 + 4, w, h - 8, XS_BG);
        if (t > 1900u || ((t - 1500u) / 100u) % 2u == 0u)
            cv_text_in(120 - w / 2, y0 + 10, w, &AF_S, "COMBAT MODE ENGAGED", XS_AMB, XS_BG);
        cv_text_in(0, 224, 240, &AF_S, "POWERED BY JIANT FM", ux_mix(XS_BG, XS_CY, 40), XS_BG);
    }
}
/* the scene at ms, on the screen */
static void xs_splash(uint32_t ms)
{
    const wb_theme_t *th = xn_theme_ptr(xn_theme_index());
    uint32_t h;
    xs_col[0] = th->bg; xs_col[1] = th->sel; xs_col[2] = th->play; xs_col[3] = th->chord; xs_col[4] = th->txt;
    for (h = 0; h < 2u; h++) {
        cv_begin(240, 120, XS_BG);
        cv_oy = -(int32_t)h * 120;
        xs_scene(ms);
        cv_oy = 0;
        if (ms > XS_END) {                       /* (the hold: it all dissolves into the theme's ground; the tracker then
                                                  * comes up out of it, wb_ui.c draw_all) */
            uint32_t k = (ms - XS_END) * 256u / 280u, i, b = XS_BG, br = b >> 11, bg = (b >> 5) & 63u, bb = b & 31u;
            if (k > 256u) k = 256u;
            for (i = 0; i < 240u * 120u; i++) {
                uint32_t c = swap16(cv_px[i]), r = c >> 11, g = (c >> 5) & 63u, bl = c & 31u;
                r = (r * (256u - k) + br * k) >> 8;
                g = (g * (256u - k) + bg * k) >> 8;
                bl = (bl * (256u - k) + bb * k) >> 8;
                cv_px[i] = swap16(r << 11 | g << 5 | bl);
            }
        }
        cv_blit(0, h * 120u);
    }
    lcd_sync();
}
