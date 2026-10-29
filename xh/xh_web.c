/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Xanacognition (XENOISE) */
/* XENOISE-H in the browser: the front end and its HYP-1 bridge (xh_wb.c) against a simulated FM-1, with the exports
 * XENOISE's emulator page and worklet already call (web/emu: felucca_web.c's names), so the same page runs it. The
 * worklet asks for audio; making it moves the device's clock a millisecond at a time: the tracker's sequencer and
 * HYP-1's render each millisecond's 44 or 45 frames, the main loop runs every 4 ms (its screen at 30 a second).
 * HYP-1's instrument (engine/amb.c) is its own unit, float, as on HYP-1.
 *   xh/build.sh  ->  build/xh/{xenoise_h.wasm, index.html, worklet.js} */
#include <stdint.h>
#include <string.h>

#define EXPORT __attribute__((used, visibility("default")))

/* ---- what the front end takes from a platform */
#define DINGHY_HOST 1                            /* (wb_ui.c: the store in RAM, the LEDs to host_leds) */
#define XN_WB_RAMSTORE 1
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define CV_MAX (3u * 240u * 40u)
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static uint16_t cv_px[CV_MAX];
static uint32_t fm1_ms;
static struct { uint32_t buttons, notes; } fm1_in;
static uint32_t web_pressed, web_released, web_notes_pressed;
static int32_t web_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = web_pressed;
    if (released) *released = web_released;
    web_pressed = web_released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = web_notes_pressed; web_notes_pressed = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = web_enc_steps[e % 7u]; web_enc_steps[e % 7u] = 0; return s; }
static uint8_t web_led[41];
static void host_leds(const uint8_t *w) { memcpy(web_led, w, sizeof web_led); }
static int str_eq(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }
static unsigned str_len(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the screen: the bands the front end sends, as the panel gets them (RGB565, high byte first) */
static uint16_t web_fb[240 * 240];
static uint32_t web_draws;
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
}

#include "xh_wb.c"

/* ---- the clock: a millisecond at a time, the audio made as it goes */
static float out_l[1024], out_r[1024];
#define RING 8192u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_acc, web_master_q12 = 3600, web_booted;
static int32_t blk[2 * 64];
static void web_ms(void)
{
    uint32_t n, i;
    web_acc += 441u;                             /* 44.1 frames a millisecond, in tenths */
    n = web_acc / 10u;
    web_acc %= 10u;
    fm1_ms++;
    if (web_booted) {
        wb_isr(n);                               /* the tracker's rows, then HYP-1's sound for them */
        en_render(blk, n, web_master_q12);
        for (i = 0; i < n; i++) {
            uint32_t k = (ring_w++ % RING) * 2u;
            ring[k] = (float)blk[2 * i] / 8388608.0f;
            ring[k + 1] = (float)blk[2 * i + 1] / 8388608.0f;
            xh_scope[xh_scope_w++ & (XH_SCOPE_N - 1u)] = (int16_t)clamp((blk[2 * i] + blk[2 * i + 1]) >> 9, -32767, 32767);
        }
        if (fm1_ms % 4u == 0u) wb_frame();
    } else {
        for (i = 0; i < n; i++) { uint32_t k = (ring_w++ % RING) * 2u; ring[k] = ring[k + 1] = 0.0f; }
    }
}
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u) n = 1024u;
    while (ring_w - ring_r < n) web_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}
EXPORT void web_boot(void)
{
    if (web_booted) return;
    wb_init();
    web_booted = 1;
}
EXPORT void web_seed(uint32_t s) { en_seed(s); }

/* ---- the panel, by its printed labels (as felucca_web.c): FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+ */
static const uint8_t LBL_BTN[14] = {BTN_FX, BTN_SEL, BTN_ENV, BTN_LFO, BTN_EDIT, BTN_GLO, BTN_HOME, BTN_SAVE, BTN_ARP,
                                    BTN_SEQ, BTN_PLAY, BTN_REC, BTN_OCT_DOWN, BTN_OCT_UP};
static const uint8_t ROLE_ENC[7] = {ENC_SELECT, ENC_ALGORITHM, ENC_PRESETS, ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4};
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    for (b = 0; b < 14u; b++) if ((labels >> b) & 1u) m |= 1u << LBL_BTN[b];
    web_pressed |= m & ~old;
    web_released |= old & ~m;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask)
{
    uint32_t old = fm1_in.notes;
    mask &= (1u << 27) - 1u;
    web_notes_pressed |= mask & ~old;
    fm1_in.notes = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n) { if (role < 7u) web_enc_steps[ROLE_ENC[role]] += n; }
EXPORT void web_master(uint32_t v) { web_master_q12 = (v > 1023u ? 1023u : v) * 4u; }
EXPORT uint32_t web_midi(uint32_t s, uint32_t d1, uint32_t d2) { (void)s; (void)d1; (void)d2; return 1; }
static uint32_t lit(uint32_t lvl, int keys)
{
    uint32_t i, m = 0;
    if (keys) { for (i = 0; i < 27u; i++) if (web_led[14u + i] == lvl) m |= 1u << i; }
    else for (i = 0; i < 14u; i++) if (web_led[LBL_BTN[i]] == lvl) m |= 1u << i;
    return m;
}
EXPORT uint32_t web_lit_buttons(void) { return lit(2u, 0) | (sq_play ? 1u << 14 : 0u); }
EXPORT uint32_t web_lit_keys(void) { return lit(2u, 1); }
EXPORT uint32_t web_dim_buttons(void) { return lit(1u, 0); }
EXPORT uint32_t web_dim_keys(void) { return lit(1u, 1); }
EXPORT uint32_t web_dim_level(void) { return 0; }
EXPORT uint32_t web_breath_buttons(void) { return 0; }
EXPORT uint32_t web_breath_keys(void) { return 0; }
EXPORT uint32_t web_mid_keys(void) { return 0; }
static uint8_t web_anim[64];
EXPORT uint8_t *web_anim_levels(void) { return web_anim; }
EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_sample_rate(void) { return 44100u; }
/* the page's saved flash: a small image it may keep (nothing in it yet: the store is in RAM) */
static uint8_t web_nor_img[4096];
EXPORT uint8_t *web_nor(void) { return web_nor_img; }
EXPORT uint32_t web_nor_size(void) { return sizeof web_nor_img; }
EXPORT void web_nor_erase(void) { memset(web_nor_img, 0xFF, sizeof web_nor_img); }
EXPORT uint32_t web_flash_writes_count(void) { return 0; }
/* the page's EXPORT, SAVE / LOAD SONG and MIDI buttons: not in the prototype yet */
EXPORT uint32_t web_xn_export(uint32_t solo) { (void)solo; return 0; }
EXPORT void web_xn_export_stop(void) {}
EXPORT const char *web_xn_track(uint32_t t) { return TRK_ID[t % NT]; }
EXPORT const char *web_xn_song(void) { return wb_song.wb_name; }
EXPORT uint8_t *web_xn_song_buf(void) { return (uint8_t *)&wb_song; }
EXPORT uint32_t web_xn_song_size(void) { return sizeof wb_song; }
EXPORT uint32_t web_xn_song_took(void) { return 1; }
EXPORT uint8_t *web_xn_midi(void) { return 0; }
EXPORT uint32_t web_xn_midi_size(void) { return 0; }
EXPORT void web_lev_mode(uint32_t m) { (void)m; }
static int32_t web_lev_buf[3];
EXPORT int32_t *web_lev_rec(void) { return web_lev_buf; }
EXPORT uint32_t web_lev_n(void) { return 0; }
EXPORT void web_lev_set_n(uint32_t n) { (void)n; }
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; }   /* (MIDI out: not in the prototype) */
