#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Final provenance check for the CJK atlas data.

1. decode the raw HZK16 glyph for U+4E2D and render it,
2. hexdump the same cell from the on-disk font_cjk_data.cpp,
3. regenerate with the CURRENT tools/font_gen_cjk.py into a temp file
   and hexdump its cell for U+4E2D.
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HZK  = os.path.join(ROOT, "tools", "hzk16h")
CPP  = os.path.join(ROOT, "src", "common", "base", "font_cjk_data.cpp")
TMP  = os.path.join(ROOT, "tools", "_regen_check.cpp")


def render_bits(rows, label):
    print(label)
    for row in rows:
        print("  " + row)


def hzk_glyph(cp_hi, cp_lo):
    qh, wh = cp_hi - 0xA0, cp_lo - 0xA0
    off = (94 * (qh - 1) + (wh - 1)) * 32
    with open(HZK, "rb") as f:
        f.seek(off)
        return f.read(32)


def hzk_rows(g):
    rows = []
    for y in range(16):
        line = ""
        for x in range(16):
            bit = (g[y * 2 + (x >> 3)] >> (7 - (x & 7))) & 1
            line += "#" if bit else "."
        rows.append(line)
    return rows


def cpp_cells(path):
    with open(path, "r") as f:
        txt = f.read()
    m = re.search(r"_CjkUcs\[CJK_GLYPH_COUNT\]\s*=\s*\{(.*?)\};", txt, re.S)
    ucs = [int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]
    m = re.search(r"_CjkPix\[524288\]\s*=\s*\{(.*?)\};", txt, re.S)
    pix = bytes(int(t, 16) for t in
                re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
    return ucs, pix


def cell_hex(pix, i):
    base = (i // 64) * 16 * 512 + (i % 64) * 8
    rows = []
    for r in range(16):
        rows.append(" ".join("%02X" % pix[base + r * 512 + c]
                             for c in range(8)))
    return rows


def cell_rows_from_hex(rows):
    """interpret nibble value 4 (bit2) as ink, low nibble = left pixel"""
    out = []
    for line in rows:
        bs = [int(t, 16) for t in line.split()]
        s = ""
        for b in bs:
            lo, hi = b & 0xF, (b >> 4) & 0xF
            s += "#" if lo & 4 else "."
            s += "#" if hi & 4 else "."
        out.append(s)
    return out


def main():
    # 1. raw HZK16 glyph for U+4E2D = GB2312 0xD6D0
    g = hzk_glyph(0xD6, 0xD0)
    render_bits(hzk_rows(g), "HZK16 raw glyph 0xD6D0 (U+4E2D):")

    # 2. on-disk cpp
    ucs, pix = cpp_cells(CPP)
    idx = ucs.index(0x4E2D)
    print("on-disk cpp: cell %d for U+4E2D" % idx)
    hx = cell_hex(pix, idx)
    for line in hx[:4]:
        print("  " + line)
    print("  ...")
    render_bits(cell_rows_from_hex(hx), "on-disk cpp cell (bit2=ink):")

    # nibble value histogram over the whole on-disk atlas
    hist = {}
    for b in pix[0:65536]:
        for n in (b & 0xF, b >> 4):
            hist[n] = hist.get(n, 0) + 1
    print("on-disk cpp nibble histogram (first 64KB):", dict(sorted(hist.items())))

    # 3. regenerate with the CURRENT generator and compare
    gen = os.path.join(ROOT, "tools", "font_gen_cjk.py")
    r = subprocess.run([sys.executable, gen, HZK, TMP],
                       capture_output=True, text=True)
    print("regen:", (r.stdout or r.stderr).strip())
    if os.path.isfile(TMP):
        ucs2, pix2 = cpp_cells(TMP)
        idx2 = ucs2.index(0x4E2D)
        print("regen cpp: cell %d for U+4E2D" % idx2)
        hx2 = cell_hex(pix2, idx2)
        render_bits(cell_rows_from_hex(hx2), "regen cell (bit2=ink):")
        hist2 = {}
        for b in pix2[0:65536]:
            for n in (b & 0xF, b >> 4):
                hist2[n] = hist2.get(n, 0) + 1
        print("regen nibble histogram (first 64KB):", dict(sorted(hist2.items())))
        same = (ucs == ucs2 and pix == pix2)
        print("on-disk cpp == regen cpp:", same)


if __name__ == "__main__":
    main()
