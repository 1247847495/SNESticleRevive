#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Diff the source atlas (_CjkPix in font_cjk_data.cpp) against the ELF copy."""
import re
import sys

ROOT = r"d:\ps2游戏\ps2 3t\PS2HDDManager\SNESticleRevive-main"
ELF = ROOT + r"\build\SNESticle.elf"
DATA = ROOT + r"\src\common\base\font_cjk_data.cpp"

with open(DATA, "r") as f:
    src = f.read()
m = re.search(r"_CjkUcs\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
ucs = [int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]
m = re.search(r"_CjkPix\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
pix = bytes(int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))

with open(ELF, "rb") as f:
    elf = f.read()

base = 0x166F68  # from readelf: .rodata off 0x157128 + (_CjkPix addr 0x256f68 - .rodata addr 0x247128)
seg = elf[base:base + len(pix)]
print("src pix: %d bytes, elf seg: %d bytes" % (len(pix), len(seg)))

diffs = [i for i in range(len(pix)) if seg[i] != pix[i]]
print("total diff bytes: %d / %d" % (len(diffs), len(pix)))
if diffs:
    # cluster the diffs
    clusters = []
    start = prev = diffs[0]
    for d in diffs[1:]:
        if d - prev > 16:
            clusters.append((start, prev))
            start = d
        prev = d
    clusters.append((start, prev))
    print("diff clusters (atlas offsets): %d" % len(clusters))
    for c in clusters[:20]:
        lo, hi = c
        glyph = lo // 128
        print("  0x%06X-0x%06X  glyph %d (cell row %d)  src=%s elf=%s"
              % (lo, hi, glyph, glyph // 64,
                 pix[lo:hi + 1][:16].hex(), seg[lo:hi + 1][:16].hex()))
        if glyph < len(ucs):
            print("    ucs[%d] = U+%04X %s" % (glyph, ucs[glyph],
                  chr(ucs[glyph]) if 0x20 <= ucs[glyph] < 0xFFFE else "?"))
