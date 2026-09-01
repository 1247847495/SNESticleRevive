# -*- coding: gbk -*-
"""Verify the GENERATED atlas data: decode font_cjk_data.cpp, look up a few
hanzi by UCS, and render their atlas cells as ASCII art.

Usage: python tools/font_check.py [char ...]   (default checks a fixed set)
"""

import re
import sys

SRC = "src/common/base/font_cjk_data.cpp"


def parse():
    text = open(SRC, "r").read()
    m = re.search(r"_CjkUcs\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S)
    ucs = [int(x, 16) for x in re.findall(r"0x([0-9A-F]{4})", m.group(1))]
    m = re.search(r"_CjkPix\[(\d+)\]\s*=\s*\{(.*?)\};", text, re.S)
    n = int(m.group(1))
    pix = bytearray(int(x, 16) for x in re.findall(r"0x([0-9A-F]{2})", m.group(2)))
    assert len(pix) == n, (len(pix), n)
    return ucs, pix


def render_cell(pix, idx):
    row_stride = 512
    base = (idx // 64) * 16 * row_stride + (idx % 64) * 8
    lines = []
    for y in range(16):
        row = ""
        for x in range(16):
            b = pix[base + y * row_stride + (x >> 1)]
            v = (b >> 4) if (x & 1) == 0 else (b & 0x0F)
            row += "##" if v else ".."
        lines.append(row)
    return "\n".join(lines)


def main():
    ucs, pix = parse()
    print("glyphs=%d  atlas bytes=%d" % (len(ucs), len(pix)))

    chars = sys.argv[1:] or ["啊", "阿", "埃", "复", "制", "文", "件",
                             "视", "频", "设", "置", "删", "除"]
    font = None
    try:
        from PIL import Image, ImageDraw, ImageFont
        font = ImageFont.truetype(r"C:\Windows\Fonts\simsun.ttc", 16)
        def ref(ch):
            img = Image.new("1", (16, 16), 0)
            ImageDraw.Draw(img).text((0, -1), ch, fill=1, font=font)
            p = img.load()
            return ["".join("#" if p[x, y] else "." for x in range(16))
                    for y in range(16)]
    except Exception:
        def ref(ch):
            return None

    for ch in chars:
        u = ord(ch)
        if u in ucs:
            idx = ucs.index(u)
            art = render_cell(pix, idx).split("\n")
            print("\n=== U+%04X %s  cell %d ===" % (u, ch, idx))
            r = ref(ch)
            for y in range(16):
                if r:
                    print(art[y] + "   " + r[y])
                else:
                    print(art[y])
        else:
            print("\n=== U+%04X %s  NOT IN TABLE ===" % (u, ch))


if __name__ == "__main__":
    main()
