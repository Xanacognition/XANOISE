#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# XENOISE-H in the browser: XENOISE's front end on HYP-1's instrument (xh/xh_web.c, xh/engine/amb.c), on XENOISE's
# emulator page (web/emu: the same page and worklet, its own wasm). Needs build/gen (tools/build.py's generated
# headers, as web/emu/build.sh) and emcc.
#   xh/build.sh  ->  build/xh/{index.html, worklet.js, xenoise_h.wasm, fonts/, standalone.html}
set -e
cd "$(dirname "$0")/.."
mkdir -p build/xh/fonts
emcc -O2 -ffp-contract=off -std=gnu99 -w -Ibuild/gen -Ixh/engine -DEN_HOST -DEN_WEB --no-entry -sSTANDALONE_WASM \
    -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 -o build/xh/xenoise_h.wasm xh/xh_web.c xh/engine/amb.c
sed -e 's/felucca\.wasm/xenoise_h.wasm/g' web/emu/index.html > build/xh/index.html
cp web/emu/worklet.js build/xh/
cp web/emu/fonts/* build/xh/fonts/
python3 web/emu/inline.py build/xh xenoise_h.wasm XENOISE-H
echo "xh: build/xh ($(wc -c < build/xh/xenoise_h.wasm) B wasm)"
