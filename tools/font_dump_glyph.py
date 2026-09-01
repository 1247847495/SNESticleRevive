#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Dump glyphs from font_cjk_data.cpp and the built ELF as ASCII art.

Verifies that:
  1. the generated atlas itself contains the RIGHT hanzi at the indices
     the runtime binary search will produce;
  2. the ELF actually contains the same atlas + UCS table (no stale
     object files linked in).

Run:  python tools/font_dump_glyph.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPP  = os.path.join(ROOT, "src", "common", "base", "font_cjk_data.cpp")
ELF  = os.path.join(ROOT, "build", "SNESticle.elf")

CELLS_ROW   = 64
GLYPH       = 16
ATLAS_BYTES = 1024 * 1024 // 2

TEST = "\u4e2d\u6587\u6d4b\u8bd5\u590d\u6d3b\u83dc\u5355"


def parse_cpp(path):
    with open(path, "r") as f:
        txt = f.read()

    m = re.search(r"_CjkUcs\[CJK_GLYPH_COUNT\]\s*=\s*\{(.*?)\};", txt, re.S)
    ucs = [int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]

    m = re.search(r"_CjkPix\[%d\]\s*=\s*\{(.*?)\};" % ATLAS_BYTES, txt, re.S)
    pix = bytes(int(t, 16) for t in
                re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
    return ucs, pix


def cell_pixels(pix, i):
    """16x16 0/1 bitmap of atlas cell i.  PSMT4: low nibble = left px,
    ink = CLUT index 4 (bit2 of the nibble value)."""
    base = (i // CELLS_ROW) * GLYPH * 512 + (i % CELLS_ROW) * 8
    rows = []
    for r in range(GLYPH):
        row = 0
        for cx in range(8):
            b = pix[base + r * 512 + cx]
            row = (row << 4) | (b & 0xF)          # even x (left) pixel
            row = (row << 4) | ((b >> 4) & 0xF)   # odd x pixel
        rows.append(row)
    return rows


def render(rows):
    out = []
    for row in rows:
        line = ""
        for x in range(16):
            line += "#" if (row >> (62 - 4 * x)) & 1 else "."
        out.append(line)
    return out


def show(label, ucs, pix, chars):
    print("==== %s ====" % label)
    print("glyph count: %d" % len(ucs))
    ok_sorted = all(ucs[k] < ucs[k + 1] for k in range(len(ucs) - 1))
    print("table sorted: %s" % ok_sorted)
    for ch in chars:
        cp = ord(ch)
        lo, hi = 0, len(ucs) - 1
        idx = -1
        while lo <= hi:
            mid = (lo + hi) // 2
            if ucs[mid] == cp:
                idx = mid
                break
            if ucs[mid] < cp:
                lo = mid + 1
            else:
                hi = mid - 1
        if idx < 0:
            print("U+%04X %s -> MISSING" % (cp, ch))
            continue
        print("U+%04X %s -> cell %d (row %d col %d)" %
              (cp, ch, idx, idx // CELLS_ROW, idx % CELLS_ROW))
        for line in render(cell_pixels(pix, idx)):
            print("  " + line)
    print()


def main():
    ucs, pix = parse_cpp(CPP)
    show("font_cjk_data.cpp", ucs, pix, TEST)

    if not os.path.isfile(ELF):
        print("ELF not found: %s" % ELF)
        return

    with open(ELF, "rb") as f:
        elf = f.read()

    # locate atlas in the ELF via a DISTINCTIVE cell (cell 202 = U+4E2D)
    # to avoid false hits on repetitive patterns (blank/tofu cells).
    probe = pix[202 * 128: 202 * 128 + 128]
    hits = []
    start = 0
    while True:
        off = elf.find(probe, start)
        if off < 0:
            break
        hits.append(off)
        start = off + 1
    if not hits:
        print("ATLAS PROBE (cell 202) NOT FOUND IN ELF (stale build?)")
        return
    print("atlas probe hits: %s" %
          ", ".join("0x%X" % h for h in hits[:8]))
    off = hits[0] - 202 * 128          # atlas base in the file
    mism = 0
    for k in range(0, ATLAS_BYTES, 4096):
        if elf[off + k: off + k + 4096] != pix[k: k + 4096]:
            mism += 1
    print("atlas 4KB-block mismatches vs cpp: %d" % mism)

    # locate the UCS table via the first 64 little-endian entries
    uprobe = b"".join(bytes([u & 0xFF, u >> 8]) for u in ucs[:64])
    uoff = elf.find(uprobe)
    if uoff < 0:
        print("UCS TABLE PROBE NOT FOUND IN ELF")
        return
    print("UCS table probe hit at ELF file offset 0x%X" % uoff)
    eucs = [elf[uoff + 2 * k] | (elf[uoff + 2 * k + 1] << 8)
            for k in range(len(ucs))]
    diff = sum(1 for a, b in zip(eucs, ucs) if a != b)
    print("UCS entry mismatches vs cpp: %d" % diff)

    epix = elf[off: off + ATLAS_BYTES]
    show("ELF copy", eucs, epix, TEST)


if __name__ == "__main__":
    main()
