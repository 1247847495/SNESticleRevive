"""Identify which chars are actually stored in a dot-matrix font file.

Renders candidate chars with a system TrueType font (Pillow), binarizes,
and correlates with the file glyphs (IoU).  Helps identify the charset
ordering of an unknown 16x16 font file.
"""

import sys
from PIL import Image, ImageDraw, ImageFont

FONT_PATH = r"C:\Windows\Fonts\simsun.ttc"


def load_font():
    return ImageFont.truetype(FONT_PATH, 16)


def render_char(font, ch):
    """Render one char into a 16x16 binary bitmap (set of (x,y))."""
    img = Image.new("1", (16, 16), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), ch, fill=1, font=font)
    px = img.load()
    return {(x, y) for y in range(16) for x in range(16) if px[x, y]}


def file_glyph(data, idx):
    off = idx * 32
    g = data[off:off + 32]
    s = set()
    for y in range(16):
        for x in range(16):
            if g[y * 2 + (x >> 3)] & (0x80 >> (x & 7)):
                s.add((x, y))
    return s


def iou(a, b):
    if not a and not b:
        return 0.0
    inter = len(a & b)
    union = len(a | b)
    return inter / union if union else 0.0


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "tools/hzk16h"
    data = open(path, "rb").read()
    font = load_font()

    # Candidate char sets
    cands = []
    # GB2312 full charset
    for hb in range(0xA1, 0xF8):
        for lb in range(0xA1, 0xFF):
            try:
                ch = bytes([hb, lb]).decode("gb2312")
                cands.append(ch)
            except UnicodeDecodeError:
                pass

    cache = {}
    for ch in cands:
        cache[ch] = render_char(font, ch)

    # indices to identify: GB2312 16-1..8 => 1410..1417
    idxs = [int(a) for a in sys.argv[2:]] or list(range(1410, 1418))
    for idx in idxs:
        g = file_glyph(data, idx)
        scored = sorted(((iou(g, b), ch) for ch, b in cache.items()),
                        reverse=True)
        top = scored[:3]
        print("idx %d (zone %d pos %d): %s" % (
            idx, idx // 94 + 1, idx % 94 + 1,
            "  ".join("%s(U+%04X,%.2f)" % (ch, ord(ch), s) for s, ch in top)))


if __name__ == "__main__":
    main()
