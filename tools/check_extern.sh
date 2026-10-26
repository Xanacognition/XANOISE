#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Xanacognition (XENOISE)
# The firmware's source as LLVM IR with the host's clang: every function it calls but does not define must be one the
# assembly gives (the ISRs, the second core's entry). A libc function (abs, memcpy ..) the FM-1 has not shows here,
# with its name, before JieLi's linker only says "ld failed".   tools/check_extern.sh  (after build.py's generated headers)
set -e
out=$(mktemp)
clang -S -emit-llvm -O1 -w -ffreestanding -fno-builtin -Werror=implicit-function-declaration -Ifirmware/hal \
    -Ifirmware/src -Ibuild/gen -DFELUCCA_ID='"FM-1_900"' firmware/src/felucca.c -o "$out"
bad=$(grep '^declare' "$out" | grep -v 'llvm\.' | grep -v -E '@(isr_alnk0|isr_timer5|fm1_c1_entry)\(' || true)
rm -f "$out"
if [ -n "$bad" ]; then echo "check_extern: functions the FM-1 has not:"; echo "$bad"; exit 1; fi
echo "check_extern: ok"
