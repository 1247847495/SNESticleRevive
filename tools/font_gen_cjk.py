#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Generate the embedded CJK glyph data for SNESticle Revive.

Input : tools/hzk16h  (16x16 GB2312-80 dot-matrix font, 87 zones x 94 chars
                       x 32 bytes = 261696 bytes; row-major, MSB-first bits)
Output: src/common/base/font_cjk_data.cpp
        - _CjkUcs[]   : sorted UCS-2 codepoints (binary-searched at runtime)
        - _CjkPix[]   : flat 1024x1024 PSMT8 atlas, 8bpp, 16x16 glyphs
                        placed at each cell (64 cells per row, 64 rows)
        - _CjkClut[]  : 256-entry GS RGBA8 palette (index 0 = transparent,
                        index 1 = opaque white, rest = opaque white)

Glyphs are the NATIVE 16x16 HZK16 dot matrix (no downsampling).  Each dot
maps to CLUT index 1 (alpha 0xFF) when set, index 0 (transparent) when
clear.  At 480i the renderer displays glyphs 1:1 (16x16 texels -> 16x16
pixels) for a crisp OPL-class result; at 240p they are minified 2:1.

PSMT8 (8bpp) is used instead of PSMT4 because gsKit's texture upload
correctly handles the 64x64-pixel page layout of PSMT8 (same pixel
grid as PSMCT32, which the UI font already uses successfully), whereas
PSMT4's 128x64-pixel pages have a different internal block ordering that
causes glyph misplacement.

The palette stores WHITE texels; the GS modulates them with the vertex
RGBAQ so FontColor4f tinting keeps working.

Charset: GB2312 zones 1 (CJK punctuation), 3 (fullwidth ASCII),
         5 (katakana), 16..55 (Level-1 hanzi, 3755 chars).
         Zone 2/6/7/8/9 and Level-2 (56..87) are skipped so the set fits
         the 4096-slot 1024x1024 atlas.  Fully-blank glyphs (unassigned
         GB2312 positions) are dropped.

Run once; the generated file is committed.
"""

import sys

HZK_PATH = sys.argv[1] if len(sys.argv) > 1 else "tools/hzk16h"
OUT_PATH = sys.argv[2] if len(sys.argv) > 2 else "src/common/base/font_cjk_data.cpp"

GLYPH_PX   = 16                 # native HZK16 dot matrix (no downsample)
ATLAS_W    = 1024               # 64 cells per row
ATLAS_H    = 1024               # 64 rows -> 4096 slots
CELLS_ROW  = ATLAS_W // GLYPH_PX
MAX_SLOTS  = (ATLAS_W // GLYPH_PX) * (ATLAS_H // GLYPH_PX)

# GS_SET_RGBA(r,g,b,a): word 0xAABBGGRR.
# Index 0: fully transparent (alpha 0x00)
# Index 1: fully opaque white (alpha 0xFF)
# Index 2..255: opaque white (unused but safe)
CLUT_SIZE = 256
CLUT_ENTRIES = [(0x00, "transparent"), (0xFF, "opaque white")]
CLUT_ENTRIES += [(0xFF, "")] * (CLUT_SIZE - 2)

# GB2312 zones to include (1-based zone numbers).
ZONES = [1, 3, 5] + list(range(16, 56))


def gb_hzk_byte(glyph, x, y):
    """Dot (0/1) of the 16x16 1bpp glyph at pixel (x, y)."""
    return (glyph[y * 2 + (x >> 3)] >> (7 - (x & 7))) & 1


def pack_16x16_native(glyph):
    """Pack the native 16x16 1bpp glyph into 256 bytes of PSMT8.

    Set dots -> CLUT index 1 (opaque), clear dots -> index 0 (transparent).
    Returns (bytes, has_ink).
    """
    out = bytearray(256)
    ink = 0
    for y in range(16):
        for x in range(16):
            if gb_hzk_byte(glyph, x, y):
                ink = 1
                out[y * 16 + x] = 1
    return bytes(out), ink != 0


def tofu_glyph_16():
    """Crisp hollow box at the native 16x16 resolution (2px border)."""
    out = bytearray(256)
    for y in range(16):
        for x in range(16):
            if y in (0, 1, 14, 15) or x in (0, 1, 14, 15):
                out[y * 16 + x] = 1
    return bytes(out)


def main():
    with open(HZK_PATH, "rb") as f:
        data = f.read()
    if len(data) < 87 * 94 * 32:
        sys.exit("HZK16 file too small: %d bytes" % len(data))

    entries = []            # (ucs2, packed 16x16 8bpp glyph)
    seen_ucs = set()

    # Tofu glyph (resolved by codepoint U+25A1 at runtime).
    entries.append((0x25A1, tofu_glyph_16()))

    # Fullwidth space U+3000 -> blank glyph (fixed width advance handled
    # by the font engine; keep an all-zero bitmap).
    entries.append((0x3000, bytes(256)))

    for zone in ZONES:
        for pos in range(1, 95):
            qh, wh = zone, pos
            off = (94 * (qh - 1) + (wh - 1)) * 32
            glyph = data[off:off + 32]
            if glyph == bytes(32):              # unassigned position
                continue
            try:
                ch = bytes([qh + 0xA0, wh + 0xA0]).decode("gb2312")
            except UnicodeDecodeError:
                continue
            ucs = ord(ch)
            if ucs in seen_ucs:
                continue
            seen_ucs.add(ucs)
            packed, ink = pack_16x16_native(glyph)
            if not ink:
                continue                        # blank glyph: leave cell blank
            entries.append((ucs, packed))

    # Deduplicate, sort by unicode (binary search requirement).
    entries = sorted({u: g for u, g in entries}.items())
    if len(entries) > MAX_SLOTS:
        sys.exit("charset %d > %d slots, drop zones" % (len(entries), MAX_SLOTS))

    # Place every glyph into the flat PSMT8 atlas buffer.
    atlas = bytearray(ATLAS_W * ATLAS_H)          # 1048576 bytes
    row_stride = ATLAS_W                           # 1024 bytes per texel row
    cell_stride = GLYPH_PX                         # 16 bytes per cell row
    for i, (ucs, packed) in enumerate(entries):
        cell_row = i // CELLS_ROW
        cell_col = i % CELLS_ROW
        base = cell_row * GLYPH_PX * row_stride + cell_col * cell_stride
        for r in range(GLYPH_PX):
            atlas[base + r * row_stride: base + r * row_stride + cell_stride] = \
                packed[r * cell_stride: r * cell_stride + cell_stride]

    lines = []
    lines.append("/* Auto-generated by tools/font_gen_cjk.py from tools/hzk16h")
    lines.append(" * (16x16 GB2312-80 dot-matrix font, hei style).")
    lines.append(" * %d glyphs (tofu + U+3000 + GB2312 zones %s)." %
                 (len(entries), ",".join(str(z) for z in ZONES[:4]) + ",16-55"))
    lines.append(" * Native 16x16 glyphs (no downsampling, dots -> CLUT index"),
    lines.append(" * packed into a %dx%d PSMT8 atlas (%d bytes)." %
                 (ATLAS_W, ATLAS_H, len(atlas)))
    lines.append(" * Do not edit by hand. */")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#define CJK_GLYPH_COUNT %d" % len(entries))
    lines.append("#define CJK_ATLAS_W %d" % ATLAS_W)
    lines.append("#define CJK_ATLAS_H %d" % ATLAS_H)
    lines.append("#define CJK_GLYPH_SIZE %d" % GLYPH_PX)
    lines.append("#define CJK_CELLS_ROW %d" % CELLS_ROW)
    lines.append("")
    lines.append("/* Definitions carry explicit extern linkage: in C++, plain")
    lines.append('   "const" globals default to internal linkage and would')
    lines.append("   never resolve the references from font.cpp. */")
    lines.append("extern const uint16_t _CjkUcs[CJK_GLYPH_COUNT] =")
    lines.append("{")
    row = []
    for ucs, _ in entries:
        row.append("0x%04X" % ucs)
        if len(row) == 12:
            lines.append("    " + ",".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ",".join(row) + ",")
    lines.append("};")
    lines.append("")
    lines.append("/* %dx%d PSMT8 atlas, 8bpp, one byte per pixel." %
                 (ATLAS_W, ATLAS_H))
    lines.append(" * Glyph i occupies cell (i %% %d, i / %d):" %
                 (CELLS_ROW, CELLS_ROW))
    lines.append(" *   byte offset = (i/%d)*%d + (i%%%d)*%d" %
                 (CELLS_ROW, GLYPH_PX * row_stride, CELLS_ROW, cell_stride))
    lines.append(" */")
    lines.append("extern const unsigned char _CjkPix[%d] =" % len(atlas))
    lines.append("{")
    CH = 32
    for off in range(0, len(atlas), CH):
        chunk = atlas[off:off + CH]
        lines.append("    " + ",".join("0x%02X" % b for b in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append("/* 256-entry CT32 palette: index 0 = transparent,")
    lines.append(" * index 1 = opaque white, rest = opaque white.")
    lines.append(" * Format = GS_SET_RGBA word (0xAABBGGRR). */")
    lines.append("extern const uint32_t _CjkClut[%d] =" % CLUT_SIZE)
    lines.append("{")
    for a, note in CLUT_ENTRIES:
        lines.append("    0x%08X%s," %
                     ((a << 24) | 0x00FFFFFF,
                      " /* %s */" % note if note else ""))
    lines.append("};")
    lines.append("")
    lines.append("extern const int _CjkGlyphCount = CJK_GLYPH_COUNT;")
    lines.append("")

    with open(OUT_PATH, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines))

    n_hanzi = sum(1 for u, _ in entries if 0x4E00 <= u <= 0x9FFF)
    print("OK: %d glyphs (%d hanzi), atlas %d bytes -> %s" %
          (len(entries), n_hanzi, len(atlas), OUT_PATH))


if __name__ == "__main__":
    main()
