#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""(XENOISE) the image self-check (hal/fm1_bootcheck.h): build.py's seal and the device's sum agree, and the cases an
interrupted update leaves are caught. Runs tests/bootcheck_sum.c (the device's code, compiled for the host).

  tests/bootcheck_test.py BOOTCHECK_SUM [build/felucca.bin]
"""
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import build  # noqa: E402

CHK = sys.argv[1]


def verdict(img):
    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        f.write(img)
    r = subprocess.run([CHK, f.name], capture_output=True, text=True)
    Path(f.name).unlink()
    return r.returncode


def image(seed, size=0x60000, at=0x40):
    rnd = random.Random(seed)
    img = bytearray(rnd.getrandbits(8) for _ in range(size))
    struct.pack_into("<4I", img, at, build.IMG_MAGIC, 0, 0, 0)
    build.img_seal(img, build.APP_XIP + at)
    return img


fails = []


def expect(name, img, want):
    got = verdict(bytes(img))
    if got != want:
        fails.append(f"{name}: exit {got}, wanted {want}")
    print(f"  {'ok' if got == want else 'FAIL':5}{name}")


new, old = image(1), image(2)
expect("a sealed image starts", new, 0)
d = bytearray(new)
d[0x31234] ^= 1
expect("one bit changed: UBOOT", d, 1)
mix = bytearray(new[:0x1000 - 0x120 + 0x8000] + old[0x1000 - 0x120 + 0x8000:])   # new sectors first, old rest
expect("an update cut off half way (new first sectors, old rest): UBOOT", mix, 1)
er = bytearray(new)
er[0x20000:0x21000] = b"\xff" * 0x1000
expect("an erased sector: UBOOT", er, 1)
un = bytearray(new)
struct.pack_into("<3I", un, 0x44, 0, 0, 0)
expect("an unsealed record: UBOOT", un, 1)
tr = bytearray(new[:0x50000])
expect("a truncated image: UBOOT", tr, 1)
if len(sys.argv) > 2 and Path(sys.argv[2]).exists():
    expect(f"the firmware build ({sys.argv[2]}) starts", Path(sys.argv[2]).read_bytes(), 0)
if fails:
    raise SystemExit("\n".join(fails))
print("bootcheck tests passed")
