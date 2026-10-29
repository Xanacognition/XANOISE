#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""XENOISE: the emulator as one self-contained page (build/emu/standalone.html): the firmware (felucca.wasm), the audio
worklet and the font inside index.html, for hosts that serve no other files at run time (an artifact viewer, a phone).
Run after web/emu/build.sh."""
import base64
import json
import sys
from pathlib import Path

E = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "build" / "emu"   # (XENOISE-H: xh/build.sh
WASM = sys.argv[2] if len(sys.argv) > 2 else "felucca.wasm"                                             # gives its own)
TITLE = sys.argv[3] if len(sys.argv) > 3 else "XENOISE"
s = (E / "index.html").read_text()
wasm = base64.b64encode((E / WASM).read_bytes()).decode()
font = base64.b64encode((E / "fonts" / "DotGothic16-subset.woff").read_bytes()).decode()
old = ('      fetch("felucca.wasm", { cache: "no-cache" }).then((r) => { if (!r.ok) throw new Error(`felucca.wasm: ${r.status}`); '
       'return r.arrayBuffer(); }),\n      ac.audioWorklet.addModule("worklet.js?v=" + Date.now())]);').replace("felucca.wasm", WASM)
new = ('      Promise.resolve(WASM_B64).then((b) => { const s = atob(b), u = new Uint8Array(s.length); '
       'for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return u.buffer; }),\n'
       '      ac.audioWorklet.addModule(URL.createObjectURL(new Blob([WORKLET_SRC], { type: "text/javascript" })))]);')
assert old in s, "index.html: the wasm / worklet loading changed"
s = s.replace(old, new)
i = s.index("async function switchOn()")
s = (s[:i] + "/* (XENOISE) the firmware and the audio worklet inside the page */\nconst WASM_B64 = \"" + wasm + "\";\n"
     "const WORKLET_SRC = " + json.dumps((E / "worklet.js").read_text()) + ";\n" + s[i:])
s = s.replace('src: url("fonts/DotGothic16-subset.woff")', 'src: url("data:font/woff;base64,' + font + '")')
s = s.replace("<title>JIANT FM</title>", f"<title>{TITLE}</title>")
(E / "standalone.html").write_text(s)
print("emu: build/emu/standalone.html", len(s), "B")
