#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Xanacognition (XANOISE)
"""XANOISE: where the flash goes. The firmware's symbols by size (objdump -t), each section's total, and the totals by
family: a symbol's name prefix up to its first '_' (ui_, gen_, fm6_ ..), so what a source file costs shows up.
  size_map.py OBJDUMP felucca.elf"""
import re
import subprocess
import sys
from collections import defaultdict

out = subprocess.run([sys.argv[1], "-t", sys.argv[2]], capture_output=True, text=True, check=True).stdout
syms = []
for ln in out.splitlines():
    m = re.match(r"^([0-9a-f]+)\s.{7}\s(\S+)\s+([0-9a-f]+)\s+(\S+)$", ln)
    if m and int(m.group(3), 16):
        syms.append((int(m.group(3), 16), m.group(2), m.group(4)))
sect, fam = defaultdict(int), defaultdict(int)
for n, s, name in syms:
    sect[s] += n
    if s in (".text", ".data", ".ram_text", ".c1_text", ".hot_text"):
        fam[re.split(r"_", name.lstrip("_"), maxsplit=1)[0].upper()] += n
print("size_map: sections")
for s, n in sorted(sect.items(), key=lambda x: -x[1]):
    print(f"  {s:12} {n:8} B")
print("size_map: families (name prefix), in flash")
for f, n in sorted(fam.items(), key=lambda x: -x[1])[:80]:
    print(f"  {f:16} {n:8} B")
print("size_map: the 250 largest symbols")
for n, s, name in sorted(syms, reverse=True)[:250]:
    print(f"  {n:7} {s:10} {name}")
