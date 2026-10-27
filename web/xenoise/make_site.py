#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""XENOISE: the site (GitHub Pages) and the page for claude.ai.

  web/xenoise/make_site.py PKG OUT_DIR [VERSION]

  OUT_DIR/index.html             the Pilot's Dock (web/xenoise/index.html): the installer (JIANT's fm1pkg.js,
                                 fm1ota.js, fm1backup.js and fm1diag.js inlined), the tutorial, the cheat sheet
  OUT_DIR/firmware/xenoise.fwsc  the package it installs (+ LICENSE, LICENSING.md, LICENSES/)
  OUT_DIR/emu/                   the browser emulator (build/emu: run web/emu/build.sh first)
  build/xenoise_dock.html        the same page for claude.ai (no package: it sends the viewer to the site to install)
"""
import json
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE.parent))
from make_site import product_of, strip_module  # noqa: E402  (web/make_site.py, JIANT's)

LIBS = ("fm1pkg.js", "fm1ota.js", "fm1backup.js", "fm1diag.js")
EMU = ("index.html", "worklet.js", "felucca.wasm", "stems.js", "fm1backup.js")


def page(meta):
    body = (HERE / "index.html").read_text(encoding="utf-8")
    lib = "\n".join(strip_module((HERE.parent / f).read_text(encoding="utf-8")) for f in LIBS)
    for mark in ("/*LIB*/", "/*META*/"):
        if body.count(mark) != 1:
            raise SystemExit(f"web/xenoise/index.html must contain {mark} once")
    return body.replace("/*LIB*/", lib).replace("/*META*/", json.dumps(meta))


def document(body):
    return ('<!doctype html>\n<html lang="en"><head><meta charset="utf-8">'
            '<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"></head>\n<body>\n'
            + body + "\n</body></html>\n")


def main(pkg, out, version="latest"):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not product.startswith("FM-1_9"):
        raise SystemExit(f"{pkg}: identity {product!r} is not a JIANT-family package (FM-1_9xx)")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no Felucca loader in it (a package patched from an official one carries vendor files)")
    fw, emu = out / "firmware", out / "emu"
    for d in (fw, emu / "fonts"):
        d.mkdir(parents=True, exist_ok=True)
    (out / "index.html").write_text(document(page({"version": version, "product": product, "pkg": "firmware/xenoise.fwsc"})),
                                    encoding="utf-8")
    shutil.copy(pkg, fw / "xenoise.fwsc")
    for f in ("LICENSE", "LICENSING.md"):
        shutil.copy(ROOT / f, fw / f)
    shutil.copytree(ROOT / "LICENSES", fw / "LICENSES", dirs_exist_ok=True)
    src = ROOT / "build" / "emu"
    if not (src / "felucca.wasm").exists():
        raise SystemExit("build/emu: run web/emu/build.sh first")
    for f in EMU:
        shutil.copy(src / f, emu / f)
    e = (emu / "index.html").read_text(encoding="utf-8")
    (emu / "index.html").write_text(e.replace("<title>JIANT FM</title>", "<title>XENOISE</title>"), encoding="utf-8")
    shutil.copytree(src / "fonts", emu / "fonts", dirs_exist_ok=True)
    (ROOT / "build" / "xenoise_dock.html").write_text(page({"version": version, "product": product, "pkg": ""}), encoding="utf-8")
    print(f"xenoise site: {out} ({product}, {len(raw)} B package, emulator); build/xenoise_dock.html")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    main(*sys.argv[1:4])
