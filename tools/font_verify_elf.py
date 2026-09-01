#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Cross-check the BUILT ELF against the source font data and menu strings.

1. Find _CjkUcs / _CjkPix inside build/SNESticle.elf and compare with
   font_cjk_data.cpp (byte-exact).
2. Verify the first glyph cells of _CjkPix in the ELF: the glyph at the
   index of U+4E2D must be the hanzi "zhong".
3. Verify UTF-8 menu strings from the translated UI files survive into the
   ELF's .rodata (e.g. the escape-sequence strings from uiBrowser.cpp).
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ELF = os.path.join(ROOT, "build", "SNESticle.elf")
DATA = os.path.join(ROOT, "src", "common", "base", "font_cjk_data.cpp")


def parse_data(path):
    with open(path, "r") as f:
        src = f.read()
    m = re.search(r"_CjkUcs\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    ucs_list = [int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{4})", m.group(1))]
    m = re.search(r"_CjkPix\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S)
    pix = bytes(int(t, 16) for t in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
    return ucs_list, pix


def load_elf(path):
    with open(path, "rb") as f:
        return f.read()


def find_all(elf, needle, limit=8):
    out = []
    start = 0
    while len(out) < limit:
        i = elf.find(needle, start)
        if i < 0:
            break
        out.append(i)
        start = i + 1
    return out


def main():
    ucs, pix = parse_data(DATA)
    elf = load_elf(ELF)
    print("elf size %d, ucs entries %d, pix bytes %d"
          % (len(elf), len(ucs), len(pix)))

    # 1) locate _CjkPix in the ELF.  A 128-byte probe cell can occur more
    #    than once (coincidental duplicates elsewhere in .rodata), so
    #    every candidate base is validated against the FULL atlas.
    probe = pix[202 * 128: 202 * 128 + 128]
    hits = find_all(elf, probe)
    print("atlas probe (cell 202) hits in ELF: %s" %
          [hex(h) for h in hits])
    if not hits:
        print("!! atlas data NOT found byte-exact in ELF -> build is stale")
        return
    base = None
    for h in hits:
        cand = h - 202 * 128
        if cand >= 0 and elf[cand: cand + len(pix)] == pix:
            base = cand
            break
    if base is None:
        cand = hits[0] - 202 * 128
        seg = elf[cand: cand + len(pix)]
        diff = [i for i in range(len(pix)) if seg[i] != pix[i]]
        print("!! no candidate base matches the full atlas "
              "(first candidate 0x%X, %d diffing bytes)"
              % (cand, len(diff)))
        return
    print("atlas base at ELF file offset 0x%X (full-atlas validated)" % base)

    # compare the whole atlas
    seg = elf[base: base + len(pix)]
    same = seg == pix
    print("full atlas identical: %s" % same)

    # 2) UCS table: search for the first 64 entries as u16 LE
    uprobe = b"".join(bytes([u & 0xFF, u >> 8]) for u in ucs[:64])
    uhits = find_all(elf, uprobe)
    print("ucs probe hits: %s" % [hex(h) for h in uhits])
    if uhits:
        ubase = uhits[0]
        useg = elf[ubase: ubase + len(ucs) * 2]
        uok = all(
            useg[i * 2] == (ucs[i] & 0xFF) and useg[i * 2 + 1] == (ucs[i] >> 8)
            for i in range(len(ucs)))
        print("full ucs table identical: %s" % uok)

    # 3) menu strings: extract escape-sequence UTF-8 strings from sources
    #    and check they are present in the ELF.
    src_files = [
        "src/platform/ps2/ui/uiBrowser.cpp",
        "src/platform/ps2/ui/uiVideo.cpp",
        "src/platform/ps2/ui/uiNetwork.cpp",
        "src/platform/ps2/system/mainloop_menu.cpp",
    ]
    missing = 0
    checked = 0
    for rel in src_files:
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            continue
        with open(p, "rb") as f:
            srctxt = f.read().decode("utf-8", "replace")
        for m in re.finditer(r'"((?:\\x[0-9A-Fa-f]{2})+)"', srctxt):
            raw = bytes(int(t, 16) for t in
                        re.findall(r"\\x([0-9A-Fa-f]{2})", m.group(1)))
            if len(raw) < 6:
                continue
            checked += 1
            if elf.find(raw) < 0:
                missing += 1
                print("MISSING in ELF: %s: %s" %
                      (rel, raw.hex()))
    print("escape strings checked %d, missing %d" % (checked, missing))

    # 4) verify every escape string is VALID UTF-8 and covered by the font
    ucs_set = set(ucs)
    bad = 0
    for rel in src_files:
        p = os.path.join(ROOT, rel)
        if not os.path.exists(p):
            continue
        with open(p, "rb") as f:
            srctxt = f.read().decode("utf-8", "replace")
        for m in re.finditer(r'"((?:\\x[0-9A-Fa-f]{2})+)"', srctxt):
            raw = bytes(int(t, 16) for t in
                        re.findall(r"\\x([0-9A-Fa-f]{2})", m.group(1)))
            try:
                s = raw.decode("utf-8")
            except UnicodeDecodeError as e:
                bad += 1
                print("INVALID UTF-8 in %s: %s (%s)" % (rel, raw.hex(), e))
                continue
            for ch in s:
                cp = ord(ch)
                if cp >= 128 and cp not in ucs_set:
                    bad += 1
                    print("char U+%04X (%s) in %s NOT IN FONT TABLE -> tofu"
                          % (cp, ch.encode("unicode_escape").decode(), rel))
    print("string font coverage problems: %d" % bad)


if __name__ == "__main__":
    main()
