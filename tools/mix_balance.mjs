#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-only
// (XENOISE) the mix balance of generated songs: each stem's level against the mix's, genre by genre, from the emulator
// (build/emu/felucca.wasm: run web/emu/build.sh first). xn_wb.c XS_DB / XS_KICK_LANE_Q8 were set from it.
//   node tools/mix_balance.mjs [felucca.wasm] [genres, e.g. 0,4,8]
import fs from 'fs';
const wasm = fs.readFileSync(process.argv[2] || new URL('../build/emu/felucca.wasm', import.meta.url));
const SECS = 20;
const B = { HOME: 6, SAVE: 7, PLAY: 10, OCTD: 12, OCTU: 13 };
async function inst() { const { instance } = await WebAssembly.instantiate(wasm, {}); return instance.exports; }
const runner = (d) => (ms) => { for (let k = 0, n = Math.round(ms * 44.1); k < n; k += 128) d.web_render(Math.min(128, n - k)); };
async function songFor(g) {
  const d = await inst(); const run = runner(d);
  d._initialize(); d.web_nor_erase(); d.web_seed(1234 + g); d.web_boot(); run(5600);
  const tap = (b, h = 60) => { d.web_buttons(1 << B[b]); run(h); d.web_buttons(0); run(250); };
  tap('SAVE', 700);                       // GEN
  d.web_enc(3, -80); run(80); d.web_enc(3, g); run(120);   // K1: the genre
  tap('OCTU'); tap('OCTU'); run(600); tap('HOME');
  const name = new TextDecoder().decode(new Uint8Array(d.memory.buffer, d.web_xn_song_buf() + 8, 15)).replace(/\0.*$/, '');
  return { mem: d.memory.buffer.slice(0), name };
}
async function render(mem, solo, lev) {
  const d = await inst(); const need = mem.byteLength - d.memory.buffer.byteLength;
  if (need > 0) d.memory.grow(Math.ceil(need / 65536));
  new Uint8Array(d.memory.buffer).set(new Uint8Array(mem));
  if (lev.data) { new Int32Array(d.memory.buffer, d.web_lev_rec(), lev.data.length).set(lev.data); d.web_lev_mode(2); d.web_lev_set_n(lev.n); }
  else d.web_lev_mode(1);
  const frames = Math.min(d.web_xn_export(solo), 44100 * SECS);
  let s2 = 0, pk = 0, clip = 0;
  for (let at = 0; at < frames; at += 128) {
    const n = Math.min(128, frames - at); d.web_render(n);
    const L = new Float32Array(d.memory.buffer, d.web_out_l(), n), R = new Float32Array(d.memory.buffer, d.web_out_r(), n);
    for (let i = 0; i < n; i++) { const a = L[i], b = R[i]; s2 += a * a + b * b; pk = Math.max(pk, Math.abs(a), Math.abs(b)); if (Math.abs(a) > 0.99 || Math.abs(b) > 0.99) clip++; }
  }
  if (!lev.data) { lev.n = d.web_lev_n(); lev.data = new Int32Array(d.memory.buffer, d.web_lev_rec(), lev.n * 3).slice(); }
  const tn = solo ? new TextDecoder().decode(new Uint8Array(d.memory.buffer, d.web_xn_track(solo - 1), 3)) : 'MIX';
  return { tn, rms: 10 * Math.log10(s2 / (2 * frames) + 1e-12), pk: 20 * Math.log10(pk + 1e-9), clip };
}
const gens = (process.argv[3] || '0,4,8,12,16,20,24,28,32,36,40,44,48,52').split(',').map(Number);
const out = [];
for (const g of gens) {
  const { mem, name } = await songFor(g);
  const lev = {}, row = [g, name];
  const mix = await render(mem, 0, lev);
  row.push(`MIX ${mix.rms.toFixed(1)} pk ${mix.pk.toFixed(1)} clip ${mix.clip}`);
  for (let s = 1; s <= 6; s++) { const r = await render(mem, s, lev); row.push(`${r.tn} ${(r.rms - mix.rms).toFixed(1)}`); }
  console.log(row.join(' | '));
}
