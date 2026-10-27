/* SPDX-License-Identifier: GPL-3.0-only */
/* XENOISE: the themes (the screen's colours: wb_ui.c TH, the boot's splash xn_splash.c), picked on MST's SYSTEM page, kept
 * in a byte of JIANT's settings (xn_wb.c xj_pref: bits 0-3).
 *   CONCRETE  the default: KNF's CONCRETE cover, black and greys, orange
 *   COMBAT    the boot's first cockpit: pitch black, a bright cyan for what is picked, red for what plays, amber
 *   CLASSIC   WND-1's: Renoise's dark warm greys, orange for what plays, blue for what is picked, cyan for the chords
 *   the rest from KNF's covers (AIRPUNCHR, PARTY PEOPLE ..): their colours, the greys mixed from them
 * (made by a few lines of Python from each cover's base, text and accents). The tracks: the drums one colour, BRK the
 * brightest, the bass another (red where the cover has one), the keys and the synth a third. (Its names are its own:
 * WND-1's files rename trk, knob and name, wb/wb_names.h.) */
#ifndef XN_THEMES_H
#define XN_THEMES_H
typedef struct {
    const char *tname;
    uint16_t bg, row, beat, panel, inset, txt, note, muted, faint, play, sel, chord, rec, arp;
    uint16_t tcol[6], kcol[4];                  /* the tracks' colours, the knobs' */
    uint16_t bp, bp2, bpdot;                    /* the blueprints behind the pages */
    uint16_t fbtn, fbtn_top, wkey, wkey_on, wkey_off, bkey_case, bkey_off;
} wb_theme_t;
#define WB_NTHEME 13u
static const wb_theme_t WB_THEMES[WB_NTHEME] = {
    {"CONCRETE", RGB(22, 22, 24), RGB(35, 35, 37), RGB(47, 48, 49), RGB(72, 74, 76), RGB(14, 14, 16), RGB(232, 236, 234), RGB(242, 222, 160), RGB(138, 140, 140), RGB(68, 69, 70), RGB(255, 154, 0), RGB(160, 166, 176), RGB(255, 204, 70), RGB(232, 72, 40), RGB(255, 204, 70),
     {RGB(255, 154, 0), RGB(204, 125, 5), RGB(171, 106, 9), RGB(232, 72, 40), RGB(196, 202, 212), RGB(196, 202, 212)},
     {RGB(255, 154, 0), RGB(255, 204, 70), RGB(160, 166, 176), RGB(196, 201, 205)},
     RGB(44, 45, 48), RGB(66, 68, 73), RGB(37, 37, 39),
     RGB(56, 56, 58), RGB(81, 82, 83), RGB(173, 176, 175), RGB(232, 236, 234), RGB(81, 82, 83), RGB(9, 9, 10), RGB(51, 52, 53)},
    {"COMBAT", RGB(5, 7, 12), RGB(10, 16, 24), RGB(17, 27, 40), RGB(22, 38, 58), RGB(7, 11, 18), RGB(214, 244, 255), RGB(127, 233, 255), RGB(77, 116, 136), RGB(26, 42, 58), RGB(255, 42, 109), RGB(32, 224, 255), RGB(255, 176, 0), RGB(255, 42, 109), RGB(255, 176, 0),
     {RGB(255, 150, 40), RGB(200, 119, 34), RGB(165, 99, 30), RGB(255, 42, 80), RGB(32, 224, 255), RGB(32, 224, 255)},
     {RGB(255, 42, 109), RGB(255, 176, 0), RGB(32, 224, 255), RGB(143, 168, 255)},
     RGB(14, 40, 54), RGB(20, 80, 100), RGB(12, 22, 30),
     RGB(20, 34, 52), RGB(32, 60, 86), RGB(170, 200, 215), RGB(214, 244, 255), RGB(40, 60, 75), RGB(2, 3, 6), RGB(20, 30, 40)},
    {"CLASSIC", RGB(24, 22, 20), RGB(34, 31, 28), RGB(46, 41, 36), RGB(60, 58, 54), RGB(30, 29, 27), RGB(226, 218, 202), RGB(222, 190, 140), RGB(142, 132, 118), RGB(74, 67, 60), RGB(242, 164, 42), RGB(52, 116, 232), RGB(52, 214, 204), RGB(232, 82, 66), RGB(52, 214, 204),
     {RGB(242, 150, 40), RGB(194, 122, 36), RGB(164, 104, 33), RGB(232, 70, 60), RGB(84, 146, 244), RGB(84, 146, 244)},
     {RGB(240, 92, 80), RGB(244, 196, 64), RGB(96, 214, 120), RGB(84, 152, 244)},
     RGB(40, 52, 74), RGB(58, 78, 116), RGB(36, 38, 44),
     RGB(86, 82, 76), RGB(110, 106, 98), RGB(196, 190, 178), RGB(206, 220, 246), RGB(98, 94, 88), RGB(16, 15, 14), RGB(40, 38, 36)},
    {"AIRPUNCHR", RGB(8, 40, 48), RGB(20, 52, 60), RGB(32, 65, 72), RGB(50, 93, 101), RGB(5, 26, 31), RGB(210, 245, 248), RGB(188, 239, 244), RGB(119, 153, 158), RGB(52, 85, 92), RGB(210, 44, 84), RGB(90, 214, 224), RGB(160, 232, 238), RGB(210, 44, 84), RGB(160, 232, 238),
     {RGB(238, 128, 96), RGB(187, 109, 85), RGB(155, 96, 79), RGB(200, 34, 72), RGB(120, 226, 236), RGB(120, 226, 236)},
     {RGB(210, 44, 84), RGB(160, 232, 238), RGB(90, 214, 224), RGB(150, 230, 236)},
     RGB(21, 68, 76), RGB(34, 96, 104), RGB(22, 54, 62),
     RGB(40, 73, 80), RGB(65, 97, 104), RGB(153, 188, 192), RGB(210, 245, 248), RGB(65, 97, 104), RGB(3, 16, 19), RGB(36, 69, 76)},
    {"PARTY PPL", RGB(236, 236, 232), RGB(223, 223, 219), RGB(210, 210, 207), RGB(185, 185, 182), RGB(243, 243, 240), RGB(20, 20, 20), RGB(38, 38, 38), RGB(117, 117, 115), RGB(188, 188, 185), RGB(24, 24, 24), RGB(96, 96, 96), RGB(60, 60, 60), RGB(200, 30, 30), RGB(60, 60, 60),
     {RGB(30, 30, 30), RGB(75, 75, 74), RGB(104, 104, 103), RGB(200, 30, 30), RGB(110, 110, 110), RGB(110, 110, 110)},
     {RGB(24, 24, 24), RGB(60, 60, 60), RGB(96, 96, 96), RGB(58, 58, 58)},
     RGB(214, 214, 210), RGB(191, 191, 188), RGB(221, 221, 217),
     RGB(201, 201, 198), RGB(176, 176, 173), RGB(80, 80, 79), RGB(20, 20, 20), RGB(176, 176, 173), RGB(47, 47, 46), RGB(206, 206, 202)},
    {"NAVY TAG", RGB(10, 14, 60), RGB(24, 28, 72), RGB(38, 41, 83), RGB(58, 69, 111), RGB(6, 9, 39), RGB(240, 240, 255), RGB(247, 247, 255), RGB(136, 138, 167), RGB(61, 64, 103), RGB(240, 122, 32), RGB(110, 168, 232), RGB(255, 255, 255), RGB(236, 64, 120), RGB(255, 255, 255),
     {RGB(240, 122, 32), RGB(189, 98, 38), RGB(157, 83, 42), RGB(236, 64, 120), RGB(110, 168, 232), RGB(110, 168, 232)},
     {RGB(240, 122, 32), RGB(255, 255, 255), RGB(110, 168, 232), RGB(175, 204, 244)},
     RGB(26, 39, 88), RGB(42, 63, 115), RGB(26, 30, 74),
     RGB(47, 50, 91), RGB(74, 77, 115), RGB(176, 177, 200), RGB(240, 240, 255), RGB(74, 77, 115), RGB(4, 6, 24), RGB(42, 46, 87)},
    {"DUNGEON", RGB(14, 11, 10), RGB(27, 23, 17), RGB(40, 34, 24), RGB(65, 55, 53), RGB(9, 7, 6), RGB(232, 204, 128), RGB(232, 191, 104), RGB(134, 117, 75), RGB(62, 53, 36), RGB(196, 34, 44), RGB(156, 124, 214), RGB(232, 176, 74), RGB(196, 34, 44), RGB(232, 176, 74),
     {RGB(226, 140, 60), RGB(179, 112, 49), RGB(150, 94, 42), RGB(196, 34, 44), RGB(185, 166, 224), RGB(185, 166, 224)},
     {RGB(196, 34, 44), RGB(232, 176, 74), RGB(156, 124, 214), RGB(194, 164, 171)},
     RGB(37, 29, 43), RGB(59, 47, 75), RGB(29, 25, 18),
     RGB(49, 42, 29), RGB(75, 65, 43), RGB(171, 150, 95), RGB(232, 204, 128), RGB(75, 65, 43), RGB(6, 4, 4), RGB(45, 38, 27)},
    {"EMBER 25", RGB(26, 30, 44), RGB(38, 42, 55), RGB(49, 53, 67), RGB(72, 76, 90), RGB(17, 20, 29), RGB(220, 224, 232), RGB(236, 219, 182), RGB(133, 137, 147), RGB(69, 73, 85), RGB(245, 162, 26), RGB(150, 158, 176), RGB(255, 212, 120), RGB(210, 76, 40), RGB(255, 212, 120),
     {RGB(245, 162, 26), RGB(197, 133, 30), RGB(166, 114, 32), RGB(210, 76, 40), RGB(172, 182, 204), RGB(172, 182, 204)},
     {RGB(245, 162, 26), RGB(255, 212, 120), RGB(150, 158, 176), RGB(185, 191, 204)},
     RGB(46, 50, 65), RGB(66, 71, 86), RGB(40, 44, 57),
     RGB(57, 61, 74), RGB(80, 84, 97), RGB(166, 170, 179), RGB(220, 224, 232), RGB(80, 84, 97), RGB(10, 12, 18), RGB(53, 57, 70)},
    {"WUZUP", RGB(0, 0, 0), RGB(15, 15, 15), RGB(31, 31, 31), RGB(61, 61, 61), RGB(0, 0, 0), RGB(255, 255, 255), RGB(235, 235, 235), RGB(140, 140, 140), RGB(56, 56, 56), RGB(255, 255, 255), RGB(170, 170, 170), RGB(210, 210, 210), RGB(255, 255, 255), RGB(210, 210, 210),
     {RGB(245, 245, 245), RGB(191, 191, 191), RGB(157, 157, 157), RGB(185, 185, 185), RGB(130, 130, 130), RGB(130, 130, 130)},
     {RGB(255, 255, 255), RGB(210, 210, 210), RGB(170, 170, 170), RGB(212, 212, 212)},
     RGB(27, 27, 27), RGB(54, 54, 54), RGB(18, 18, 18),
     RGB(41, 41, 41), RGB(71, 71, 71), RGB(184, 184, 184), RGB(255, 255, 255), RGB(71, 71, 71), RGB(0, 0, 0), RGB(36, 36, 36)},
    {"CHAO", RGB(34, 8, 6), RGB(47, 22, 18), RGB(61, 36, 29), RGB(70, 58, 61), RGB(22, 5, 4), RGB(255, 240, 200), RGB(250, 226, 136), RGB(156, 136, 113), RGB(83, 59, 49), RGB(232, 64, 42), RGB(40, 120, 210), RGB(245, 208, 58), RGB(232, 64, 42), RGB(245, 208, 58),
     {RGB(242, 150, 40), RGB(196, 119, 33), RGB(167, 99, 28), RGB(232, 64, 42), RGB(70, 150, 236), RGB(70, 150, 236)},
     {RGB(232, 64, 42), RGB(245, 208, 58), RGB(40, 120, 210), RGB(148, 180, 205)},
     RGB(35, 26, 39), RGB(36, 44, 71), RGB(49, 24, 20),
     RGB(69, 45, 37), RGB(96, 73, 60), RGB(193, 175, 146), RGB(255, 240, 200), RGB(96, 73, 60), RGB(14, 3, 2), RGB(65, 40, 33)},
    {"LIME TAG", RGB(8, 10, 8), RGB(22, 25, 22), RGB(35, 39, 35), RGB(51, 73, 67), RGB(5, 6, 5), RGB(235, 255, 235), RGB(244, 255, 244), RGB(133, 145, 133), RGB(58, 64, 58), RGB(184, 240, 0), RGB(62, 216, 200), RGB(255, 255, 255), RGB(184, 240, 0), RGB(255, 255, 255),
     {RGB(184, 240, 0), RGB(145, 189, 2), RGB(121, 157, 3), RGB(62, 216, 200), RGB(150, 240, 220), RGB(150, 240, 220)},
     {RGB(184, 240, 0), RGB(255, 255, 255), RGB(62, 216, 200), RGB(148, 236, 218)},
     RGB(17, 43, 39), RGB(25, 76, 69), RGB(24, 27, 24),
     RGB(44, 49, 44), RGB(72, 79, 72), RGB(171, 186, 171), RGB(235, 255, 235), RGB(72, 79, 72), RGB(3, 4, 3), RGB(40, 44, 40)},
    {"PSYKISTA", RGB(30, 16, 70), RGB(43, 29, 77), RGB(56, 43, 85), RGB(72, 69, 109), RGB(20, 10, 46), RGB(245, 240, 192), RGB(245, 240, 192), RGB(148, 139, 137), RGB(77, 65, 97), RGB(168, 240, 176), RGB(96, 166, 230), RGB(245, 240, 192), RGB(168, 240, 176), RGB(245, 240, 192),
     {RGB(168, 240, 176), RGB(138, 191, 153), RGB(118, 159, 138), RGB(150, 110, 236), RGB(96, 166, 230), RGB(96, 166, 230)},
     {RGB(168, 240, 176), RGB(245, 240, 192), RGB(96, 166, 230), RGB(170, 203, 211)},
     RGB(41, 40, 96), RGB(51, 64, 121), RGB(45, 32, 79),
     RGB(64, 52, 90), RGB(90, 79, 104), RGB(185, 177, 158), RGB(245, 240, 192), RGB(90, 79, 104), RGB(12, 6, 28), RGB(60, 47, 87)},
    {"REDLINE", RGB(10, 10, 10), RGB(24, 24, 24), RGB(37, 37, 37), RGB(62, 62, 62), RGB(6, 6, 6), RGB(235, 235, 235), RGB(224, 224, 224), RGB(134, 134, 134), RGB(60, 60, 60), RGB(224, 40, 42), RGB(154, 154, 154), RGB(210, 210, 210), RGB(224, 40, 42), RGB(210, 210, 210),
     {RGB(232, 96, 64), RGB(183, 77, 52), RGB(152, 65, 45), RGB(224, 40, 42), RGB(180, 180, 180), RGB(180, 180, 180)},
     {RGB(224, 40, 42), RGB(210, 210, 210), RGB(154, 154, 154), RGB(194, 194, 194)},
     RGB(33, 33, 33), RGB(56, 56, 56), RGB(26, 26, 26),
     RGB(46, 46, 46), RGB(73, 73, 73), RGB(172, 172, 172), RGB(235, 235, 235), RGB(73, 73, 73), RGB(4, 4, 4), RGB(42, 42, 42)}
};
/* (XENOISE) CUSTOM, the theme after the list: the Dock's THEME LAB makes it (editor 99 XN_THEME), wb_ui.c keeps it in
 * the flash sector after the store (xn_theme_load / xn_theme_store); until one comes, CONCRETE's colours */
#define XN_NTHEME (WB_NTHEME + 1u)
#define XN_TH_NCOL 34u                          /* the colours from bg to bkey_off, in the struct's order */
static char xn_custom_name[12] = "CUSTOM";
static wb_theme_t xn_custom;
static uint16_t *xn_theme_cols(wb_theme_t *t) { return &t->bg; }
typedef char xn_theme_cols_packed[__builtin_offsetof(wb_theme_t, bkey_off) + 2u - __builtin_offsetof(wb_theme_t, bg) == XN_TH_NCOL * 2u ? 1 : -1];
static uint32_t xn_theme_index(void) { return (uint32_t)(favorites.factory[15][31] & 15u) % XN_NTHEME; }
static const wb_theme_t *xn_theme_ptr(uint32_t i)
{
    if (i < WB_NTHEME) return &WB_THEMES[i];
    if (!xn_custom.tname) { xn_custom = WB_THEMES[0]; xn_custom.tname = xn_custom_name; }
    return &xn_custom;
}
#endif
