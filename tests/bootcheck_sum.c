/* SPDX-License-Identifier: GPL-3.0-only */
/* (XENOISE) hal/fm1_bootcheck.h's check on a file: what fm1_boot_check decides at a start, for an app.bin.
 *   bootcheck_sum IMAGE   exit 0: the start goes on; 1: UBOOT (damaged or unsealed); 2: usage
 * tests/bootcheck_test.py runs it against images build.py sealed, then damaged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FM1_BOOTCHECK_SUM_ONLY 1
#include "fm1_bootcheck.h"

int main(int argc, char **argv)
{
    static uint32_t w[FM1_IMG_SLOT / 4u + 1u];
    FILE *f;
    size_t n, i;
    if (argc != 2 || !(f = fopen(argv[1], "rb")))
        return 2;
    n = fread(w, 1, sizeof w, f);
    fclose(f);
    for (i = 0; i + 4u <= n / 4u; i++) {        /* the record, as fm1_img_check sits in the image */
        uint32_t len = w[i + 1], sum = w[i + 2];
        if (w[i] != FM1_IMG_MAGIC)
            continue;
        if (!len || len > FM1_IMG_SLOT || (len & 3u) || len > n || w[i + 3] != ~sum)
            break;
        if (fm1_img_sum(w, len >> 2, (uint32_t)i + 2u) != sum)
            break;
        printf("sealed: %u B, sum %08x, record at +%#x\n", (unsigned)len, (unsigned)sum, (unsigned)(i * 4u));
        return 0;
    }
    printf("UBOOT: no valid image record\n");
    return 1;
}
