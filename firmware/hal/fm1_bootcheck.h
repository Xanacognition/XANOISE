/* SPDX-License-Identifier: GPL-3.0-only */
/* (XENOISE) The image's self-check, first thing at every start: crt0.S calls fm1_boot_check before fm1_cstart.
 *
 * Why: an update writes the app area sector by sector from its start (loader/ldr_core.c). Cut off half way (a battery
 * brown-out, a USB drop), the flash holds the new first sectors and the old rest; the SPL starts that mix, and its first
 * call into an old sector hangs the chip before the boot guard or the OCT hold ever run. No USB device, no rescue: only
 * an FM-1 Transporter could reach it. A check in the first sector, which an update writes first, catches the mix: the
 * whole image is summed through the XIP window before anything else runs and, unless the sum is the one tools/build.py
 * stored, the chip goes to its mask-ROM UBOOT (USB disk WL82 UBOOT1.00), where a plain USB cable writes it again
 * (MvaveFM1Unbricker, jl-uboot-tool).
 *
 * The rules that keep it able to run in a damaged image (tools/build.py checks them):
 *   - fm1_boot_check and fm1_img_check sit in the first 0xEE0 bytes of app.bin (flash 0x4120..0x4FFF: sector 0x4000);
 *   - fm1_boot_check calls nothing (every helper below inlines) and uses no .data or .bss (not set up yet);
 *   - the sum is over app.bin's words [0, len), the stored sum and its complement counted as 0.
 * The sum: a rotate-xor-add over the words (no multiply: no libcall). It is no cryptographic hash; it tells one build's
 * sectors from another's. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#define FM1_IMG_MAGIC 0x584E494Du               /* "MINX" little-endian: XENOISE image record */
#define FM1_IMG_XIP 0x02000120u                 /* app.bin offset 0 (app.ld XIP ORIGIN) */
#define FM1_IMG_SLOT 0x8EA90u                   /* app.ld XIP LENGTH (tools/fm1pkg_make.py APP_SLOT) */

/* { magic, image length in bytes, sum, ~sum }: build.py writes the last three into the image */
__attribute__((section(".rodata.crt1"), used)) const volatile uint32_t fm1_img_check[4] = {FM1_IMG_MAGIC, 0, 0, 0};

FM1_INLINE uint32_t fm1_img_step(uint32_t h, uint32_t v, uint32_t i)
{
    h ^= v;
    return ((h << 5) | (h >> 27)) + i + 0x9E3779B9u;
}

/* the sum of n words at w; the words at skip and skip + 1 count as 0 (tests/bootcheck_test.c runs it on build/felucca.bin) */
FM1_INLINE uint32_t fm1_img_sum(const volatile uint32_t *w, uint32_t n, uint32_t skip)
{
    uint32_t h = 0x584E4F49u, i;
    for (i = 0; i < n; i++)
        h = fm1_img_step(h, (i == skip || i == skip + 1u) ? 0u : w[i], i);
    return h;
}

#ifndef FM1_BOOTCHECK_SUM_ONLY
/* P33 (the power domain's serial bus), inline: fm1_sys.h's helpers are calls */
FM1_INLINE void fm1__bc_p33_xfer(uint8_t b)
{
    uint32_t n = 100000u;
    *(volatile uint32_t *)0x13E0Cu = b;
    *(volatile uint32_t *)0x13E08u |= 1u << 4;
    while ((*(volatile uint32_t *)0x13E08u & (1u << 1)) && --n)
        ;
}
FM1_INLINE void fm1__bc_p33_or(uint32_t a, uint8_t d)
{
    *(volatile uint32_t *)0x13E08u &= ~(1u << 8);
    *(volatile uint32_t *)0x13E08u |= 1u << 0;
    fm1__bc_p33_xfer((uint8_t)((1u << 5) | ((a >> 8) & 3u)));
    fm1__bc_p33_xfer((uint8_t)a);
    fm1__bc_p33_xfer(d);
    *(volatile uint32_t *)0x13E08u &= ~((1u << 0) | (1u << 8));
}

__attribute__((section(".text.crt1"), used, noinline)) void fm1_boot_check(void)
{
    const volatile uint32_t *img = (const volatile uint32_t *)FM1_IMG_XIP;
    uint32_t len = fm1_img_check[1], sum = fm1_img_check[2], skip, i, h;
    if (fm1_img_check[0] == FM1_IMG_MAGIC && len && len <= FM1_IMG_SLOT && !(len & 3u) && fm1_img_check[3] == ~sum) {
        skip = (uint32_t)((uintptr_t)&fm1_img_check[2] - FM1_IMG_XIP) >> 2;
        for (i = 0; i < 2u; i++) {              /* twice before giving up: a read glitch is not a damaged image */
            fm1__bc_p33_or(0x80u, 0x40);        /* the watchdog, if the SPL left it running: fed */
            h = fm1_img_sum(img, len >> 2, skip);
            if (h == sum)
                return;
        }
    }
    /* damaged (or not an image build.py finished): the mask-ROM UBOOT, as fm1_sys.h fm1_enter_uboot */
    __asm__ volatile("cli");
    *(volatile uint32_t *)0x1EEE004u |= 2u;     /* the second core held */
    *(volatile uint32_t *)0x1EEE004u &= ~8u;
    if (!(*(volatile uint32_t *)0x1EEE240u & 1u))
        *(volatile uint32_t *)0x1EEE240u = 0xE7u;
    *(volatile uint32_t *)0x1EEE348u = 0;
    /* "usb_update_mode" as words (immediates: a string would sit in .rodata, past the first sector) */
    ((volatile uint32_t *)0x01C7FD80u)[0] = 0x5F627375u;
    ((volatile uint32_t *)0x01C7FD80u)[1] = 0x61647075u;
    ((volatile uint32_t *)0x01C7FD80u)[2] = 0x6D5F6574u;
    ((volatile uint32_t *)0x01C7FD80u)[3] = 0x0065646Fu;
    fm1__bc_p33_or(0xA0u, 0x10);                /* chip reset: the ROM finds the mailbox */
    for (;;)
        ;
}
#endif
