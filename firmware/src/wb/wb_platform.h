/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XENOISE) */
/* XENOISE: what WND-1's files had from KAWA's platform (its platform.c, gfx.c) that JIANT has not as such: the panel's
 * names, the font type of its screen fonts, the poll its drawing calls between bands (JIANT's main loop does that). */
enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_FX = 2, BTN_SEL = 3, BTN_ENV = 4, BTN_LFO = 5, BTN_EDIT = 6,
       BTN_GLO = 7, BTN_HOME = 8, BTN_SAVE = 9, BTN_ARP = 10, BTN_SEQ = 11, BTN_PLAY = 12, BTN_REC = 13 };
enum { ENC_SELECT = 0, ENC_ALGORITHM = 1, ENC_KNOB1 = 2, ENC_KNOB2 = 3, ENC_KNOB3 = 4, ENC_KNOB4 = 5, ENC_PRESETS = 6 };
typedef struct {
    uint8_t h, pad, first, last;
    const uint8_t *adv, *bw;
    const uint16_t *off;
    const uint8_t *data;
} felucca_font_t;
static uint32_t glyph(const felucca_font_t *f, uint32_t ch)
{
    if (ch >= 'a' && ch <= 'z' && f->last < 'a') ch -= 32u;
    if (ch < f->first || ch > f->last || (ch >= 127u && ch < 160u)) ch = '?';
    return ch - f->first;
}
static void platform_poll(void) {}
#define KEY_LED(k) (14u + (k))                  /* a key's LED id (14 + the key) */
