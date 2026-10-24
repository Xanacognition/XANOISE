#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""XANOISE: the emulator as one self-contained page (build/emu/standalone.html): the firmware (felucca.wasm), the audio
worklet and the font inside index.html, for hosts that serve no other files at run time (an artifact viewer, a phone).
Run after web/emu/build.sh."""
import base64
import json
from pathlib import Path

E = Path(__file__).resolve().parents[2] / "build" / "emu"
s = (E / "index.html").read_text()
wasm = base64.b64encode((E / "felucca.wasm").read_bytes()).decode()
font = base64.b64encode((E / "fonts" / "DotGothic16-subset.woff").read_bytes()).decode()
old = ('      fetch("felucca.wasm", { cache: "no-cache" }).then((r) => { if (!r.ok) throw new Error(`felucca.wasm: ${r.status}`); '
       'return r.arrayBuffer(); }),\n      ac.audioWorklet.addModule("worklet.js?v=" + Date.now())]);')
new = ('      Promise.resolve(WASM_B64).then((b) => { const s = atob(b), u = new Uint8Array(s.length); '
       'for (let i = 0; i < s.length; i++) u[i] = s.charCodeAt(i); return u.buffer; }),\n'
       '      ac.audioWorklet.addModule(URL.createObjectURL(new Blob([WORKLET_SRC], { type: "text/javascript" })))]);')
assert old in s, "index.html: the wasm / worklet loading changed"
s = s.replace(old, new)
i = s.index("async function switchOn()")
s = (s[:i] + "/* (XANOISE) the firmware and the audio worklet inside the page */\nconst WASM_B64 = \"" + wasm + "\";\n"
     "const WORKLET_SRC = " + json.dumps((E / "worklet.js").read_text()) + ";\n" + s[i:])
s = s.replace('src: url("fonts/DotGothic16-subset.woff")', 'src: url("data:font/woff;base64,' + font + '")')
s = s.replace("<title>JIANT FM</title>", "<title>XANOISE</title>")
(E / "standalone.html").write_text(s)
print("emu: build/emu/standalone.html", len(s), "B")
