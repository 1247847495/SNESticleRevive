#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verify hzk16h encoding map and the generated CJK atlas data.

1. Draw the HZK16 glyph at zone 16 pos 1 ("啊" in GB2312 Level-1) as ASCII art.
2. Parse font_cjk_data.cpp, locate U+554A ("啊") in _CjkUcs, extract its
   16x16 cell from the atlas, and draw it as ASCII art.
3. Compare the two bitmaps.

Usage: python tools/font_verify_cjk.py
"""
import sys
import re

HZK_PATH = "tools/hzk16h"
DATA_PATH = "src/common/base/font_cjk_data.cpp"


def hzk_glyph(zone, pos):
    with open(HZK_PATH, "rb") as f:
        data = f.read()
    off = (94 * (zone - 1) + (pos - 1)) * 32
    return data[off:off + 32]


def draw_1bpp(glyph):
    rows = []
    for y in range(16):
        row = ""
        for x in range(16):
            bit = (glyph[y * 2 + (x >> 3)] >> (7 - (x & 7))) & 1
            row += "#" if bit else "."
        rows.append(row)
    return rows


def parse_data_cpp():
    with open(DATA_PATH, "r", encoding="ascii") as f:
        text = f.read()

    m = re.search(r"extern const uint16_t _CjkUcs\[CJK_GLYPH_COUNT\]\s*=\s*\{(.*?)\};",
                  text, re.S)
    ucs = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]

    m = re.search(r"extern const unsigned char _CjkPix\[\d+\]\s*=\s*\{(.*?)\};",
                  text, re.S)
    pix = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))]
    return ucs, pix


def main():
    target_ucs = 0x554A  # "啊"

    print("=== 1. HZK16 zone 16 pos 1 (should be U+554A '啊') ===")
    ch = bytes([16 + 0xA0, 1 + 0xA0]).decode("gb2312")
    print("GB2312 decode of (0xB0,0xA1): U+%04X %r" % (ord(ch), ch))
    g = hzk_glyph(16, 1)
    art1 = draw_1bpp(g)
    for r in art1:
        print("  " + r)

    print()
    print("=== 2. Atlas cell for U+%04X in generated data ===" % target_ucs)
    ucs, pix = parse_data_cpp()
    print("glyph count: %d, atlas bytes: %d" % (len(ucs), len(pix)))
    if target_ucs not in ucs:
        print("!! U+%04X NOT FOUND in _CjkUcs" % target_ucs)
        return
    idx = ucs.index(target_ucs)
    print("glyph index: %d (cell row %d, col %d)" % (idx, idx // 64, idx % 64))

    # PSMT8 atlas: 1 byte per pixel, row stride 1024
    cell_col = idx % 64
    cell_row = idx // 64
    art2 = []
    for y in range(16):
        base = (cell_row * 16 + y) * 1024 + cell_col * 16
        row = ""
        for x in range(16):
            row += "#" if pix[base + x] else "."
        art2.append(row)
    for r in art2:
        print("  " + r)

    print()
    print("=== 3. Compare ===")
    print("MATCH" if art1 == art2 else "MISMATCH")


if __name__ == "__main__":
    main()
