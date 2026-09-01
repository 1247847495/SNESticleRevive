#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Convert GBK-encoded source lines to UTF-8, line by line.

A line is converted ONLY if it fails strict UTF-8 decode AND decodes as
GBK.  UTF-8 lines (even mojibake-prone ones) are never touched, so mixed
files stay safe.  Prints every converted line for review.
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FILES = [
    "src/common/base/font.cpp",
    "src/platform/ps2/system/embedded_irx.cpp",
    "src/platform/ps2/ui/uiBrowser.cpp",
    "src/platform/ps2/ui/uiVideo.cpp",
]
APPLY = "--apply" in sys.argv


def convert_path(path):
    with open(path, "rb") as f:
        raw = f.read()
    lines = raw.split(b"\n")
    changed = 0
    for i, line in enumerate(lines):
        try:
            line.decode("utf-8")
            continue                      # already fine
        except UnicodeDecodeError:
            pass
        try:
            text = line.decode("gbk")
        except UnicodeDecodeError:
            print("SKIP (not GBK either) %s:%d" % (path, i + 1))
            continue
        printable = text.encode("unicode_escape").decode("ascii")
        print("%s:%d GBK->UTF8: %s" % (os.path.basename(path), i + 1,
                                       printable))
        lines[i] = text.encode("utf-8")
        changed += 1
    if changed and APPLY:
        with open(path, "wb") as f:
            f.write(b"\n".join(lines))
    return changed


def main():
    total = 0
    for rel in FILES:
        path = os.path.join(ROOT, rel)
        total += convert_path(path)
    print()
    print("total lines to convert: %d (%s)"
          % (total, "APPLIED" if APPLY else "dry-run, use --apply"))


if __name__ == "__main__":
    main()
