"""Diagnostic: render glyphs from a 16x16 dot-matrix font file as ASCII art.

Usage: python tools/font_preview.py [font-file] [zone] [pos] ...
Defaults render GB2312 zone-16 chars (should read "a-a-ai-ai-ai-ai-ai-ai"
hanzi) so the charset order (GB2312 vs JIS/Big5) can be identified.
"""

import sys

LSB_FIRST = "--lsb" in sys.argv
ARGS = [a for a in sys.argv[1:] if a != "--lsb"]


def render(data, qh, wh):
    off = (94 * (qh - 1) + (wh - 1)) * 32
    if off + 32 > len(data):
        return "  <out of range>"
    g = data[off:off + 32]
    lines = []
    for y in range(16):
        row = ""
        for x in range(16):
            b = g[y * 2 + (x >> 3)]
            mask = (1 << (x & 7)) if LSB_FIRST else (0x80 >> (x & 7))
            row += "##" if (b & mask) else ".."
        lines.append(row)
    return "\n".join(lines)


def main():
    path = ARGS[0] if ARGS else "tools/hzk16h"
    data = open(path, "rb").read()
    print("file=%s  size=%d  (%d chars of 32 bytes)  bit order=%s" % (
        path, len(data), len(data) // 32,
        "LSB-first" if LSB_FIRST else "MSB-first"))

    spots = []
    if len(ARGS) > 1:
        qh = int(ARGS[1])
        for a in ARGS[2:]:
            spots.append((qh, int(a)))
    else:
        # GB2312 zone16 pos1-8: first eight Level-1 hanzi
        for pos in range(1, 9):
            spots.append((16, pos))

    for qh, wh in spots:
        print("\n=== zone %d, pos %d (offset %d) ===" % (
            qh, wh, (94 * (qh - 1) + (wh - 1)) * 32))
        print(render(data, qh, wh))


if __name__ == "__main__":
    main()
