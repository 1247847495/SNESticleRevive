#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Verify the atlas data chain: hzk16h -> font_cjk_data.cpp -> runtime lookup.

For each sample hanzi:
  1. Read the native 16x16 glyph straight from tools/hzk16h (GB2312 order).
  2. Pack it exactly like tools/font_gen_cjk.py does (dots -> CLUT index 4,
     PSMT4 low nibble = left pixel).
  3. Simulate the runtime: binary-search the parsed _CjkUcs table for the
     char's UCS codepoint, extract the atlas cell at (i % 64, i / 64).
  4. Compare.  On mismatch, scan the whole atlas to find which index really
     holds the expected glyph and report the wrong identity mapping.

Also scans every .c/.cpp/.h under src/ for non-UTF-8 bytes (GBK menu
strings would garble at runtime because the renderer decodes UTF-8).

Output is ASCII-only so it is safe on any console.
"""

import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HZK_PATH = os.path.join(ROOT, "tools", "hzk16h")
DATA_PATH = os.path.join(ROOT, "src", "common", "base", "font_cjk_data.cpp")

TEST_CHARS = (
    u"\u4e2d\u6587\u6d4b\u8bd5\u6e38\u620f\u6587\u4ef6\u76ee\u5f55"
    u"\u5220\u9664\u590d\u5236\u7c98\u8d34\u83dc\u5355\u8bbe\u7f6e"
    u"\u4fdd\u5b58\u8bfb\u53d6\u683c\u5f0f\u5316\u9000\u51fa\u786e"
    u"\u5b9a\u53d6\u6d88\u542f\u52a8\u52a0\u8f7d\u540d\u79f0\u8def"
)


def parse_data(path):
    with open(path, "r") as f:
        src = f.read()
    m = re.search(r"_CjkUcs\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        sys.exit("cannot find _CjkUcs")
    ucs = [int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]
    m = re.search(r"_CjkPix\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        sys.exit("cannot find _CjkPix")
    pix = bytearray(
        int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))
    )
    m = re.search(r"_CjkClut\[16\]\s*=\s*\{(.*?)\};", src, re.S)
    clut = [int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{8})", m.group(1))]
    return ucs, pix, clut


def hzk_glyph(data, ucp):
    """32 raw bytes of the GB2312 glyph for ucp, or None."""
    try:
        gb = chr(ucp).encode("gb2312")
    except UnicodeEncodeError:
        return None
    qh, wh = gb[0] - 0xA0, gb[1] - 0xA0
    if not (1 <= qh <= 87 and 1 <= wh <= 94):
        return None
    off = (94 * (qh - 1) + (wh - 1)) * 32
    g = data[off:off + 32]
    if g == bytes(32):
        return None
    return g


def pack_cell(glyph):
    """Replicate font_gen_cjk.pack_16x16_native: 128 bytes, idx 4 = ink."""
    out = bytearray(128)
    for y in range(16):
        for x in range(16):
            if (glyph[y * 2 + (x >> 3)] >> (7 - (x & 7))) & 1:
                if x & 1:
                    out[y * 8 + (x >> 1)] |= 4 << 4
                else:
                    out[y * 8 + (x >> 1)] |= 4
    return bytes(out)


def render_cell(cell):
    lines = []
    for y in range(16):
        row = ""
        for x in range(16):
            b = cell[y * 8 + (x >> 1)]
            v = (b >> 4) if (x & 1) == 0 else (b & 0x0F)
            row += "##" if v else ".."
        lines.append(row)
    return "\n".join(lines)


def bsearch(ucs, cp):
    lo, hi = 0, len(ucs) - 1
    while lo <= hi:
        mid = (lo + hi) >> 1
        if ucs[mid] == cp:
            return mid
        if ucs[mid] < cp:
            lo = mid + 1
        else:
            hi = mid - 1
    return -1


def cell_bytes(pix, i):
    row, col = i // 64, i % 64
    base = row * 16 * 512 + col * 8
    out = bytearray()
    for r in range(16):
        out += pix[base + r * 512: base + r * 512 + 8]
    return bytes(out)


def scan_encoding():
    print("=== source file encoding scan (src/) ===")
    bad = 0
    for ext in ("c", "cpp", "h"):
        for path in glob.glob(os.path.join(ROOT, "src", "**", "*." + ext),
                              recursive=True):
            with open(path, "rb") as f:
                raw = f.read()
            try:
                raw.decode("utf-8")
            except UnicodeDecodeError:
                enc = "gbk?"
                try:
                    raw.decode("gbk")
                except UnicodeDecodeError:
                    enc = "unknown"
                bad += 1
                rel = path.replace(ROOT + os.sep, "")
                print("NOT UTF-8 [%s]: %s" % (enc, rel))
    if not bad:
        print("all source files are valid UTF-8")
    print()


def main():
    scan_encoding()

    with open(HZK_PATH, "rb") as f:
        hzk = f.read()
    ucs, pix, clut = parse_data(DATA_PATH)
    print("parsed: %d ucs entries, %d atlas bytes, %d clut entries"
          % (len(ucs), len(pix), len(clut)))
    print("clut[4] = 0x%08X (expect fully opaque: alpha byte 0xFF)"
          % clut[4])
    print()

    ok = bad = tofu = 0
    for ch in TEST_CHARS:
        cp = ord(ch)
        label = "U+%04X" % cp
        g = hzk_glyph(hzk, cp)
        if g is None:
            tofu += 1
            print("%s: not in hzk16h (would be tofu)" % label)
            continue
        expected = pack_cell(g)
        i = bsearch(ucs, cp)
        if i < 0:
            bad += 1
            print("%s: MISSING from _CjkUcs (runtime -> tofu!)" % label)
            continue
        got = cell_bytes(pix, i)
        if got == expected:
            ok += 1
            continue
        bad += 1
        where = None
        for j in range(len(ucs)):
            if cell_bytes(pix, j) == expected:
                where = j
                break
        if where is None:
            print("%s: glyph NOT FOUND anywhere in atlas (cell %d corrupted)"
                  % (label, i))
        else:
            print("%s: runtime index %d, but glyph sits at index %d "
                  "(=_CjkUcs[%d] = U+%04X) -> shows WRONG char!"
                  % (label, i, where, where, ucs[where]))
    print()
    print("RESULT: ok=%d bad=%d tofu=%d / %d chars"
          % (ok, bad, tofu, len(TEST_CHARS)))

    for ch in TEST_CHARS[:3]:
        cp = ord(ch)
        i = bsearch(ucs, cp)
        if i < 0:
            continue
        print()
        print("--- U+%04X (runtime atlas cell %d) ---" % (cp, i))
        print(render_cell(cell_bytes(pix, i)))


if __name__ == "__main__":
    main()
