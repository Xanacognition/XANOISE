# XENOISE-H (prototype)

XENOISE's front end (`firmware/src/wb/`: the tracker, generator, pages and Pilot System) on **HYP-1**'s instrument
instead of JIANT's engines. A browser prototype for now: `xh/build.sh` (Emscripten) builds `build/xh/`, XENOISE's
emulator page with the new engine.

| XENOISE track | HYP-1 |
| --- | --- |
| BRK | the drum lanes: each break slice as its hits, synthesized (no samples) |
| KIK, PER | the drum lanes (kick; snare, clap, hats, perc), fired hit by hit (`en_hit`, added here) |
| BAS | the drone |
| KEY (and its counter voice) | the pad, four voices |
| SYN | the acid voice |

XENOISE's 27 sounds map to HYP-1's 16 per voice, and its kicks to HYP-1's eight kits (`xh_wb.c`). Not mapped yet:
the LFO and ARP pages, the envelope knobs, AGE and CRUSH, the meters, MIDI, the page's export buttons.

## Credits

- `xh/engine/` is HYP-1's instrument (`firmware/src/dsp/amb.c`, `amb.h`, `fastmath.h`, `pats.h`) from
  [RustyPawz/fm1-HYP1](https://github.com/RustyPawz/fm1-HYP1) at commit `57c456db61826331d356458c9c1a136517da7cbf`:
  HYP-1 by RedtailWorks, a fork of [AMB-1](https://github.com/charlesvestal/fm1-amb) by Charles Vestal, with Dattorro's plate from
  [FoMni](https://github.com/charlesvestal/fm1-fomni). GPL-3.0-only, as XENOISE.
- The one change to it: `en_hit` (a drum lane fired now), marked `(XENOISE-H)`.
