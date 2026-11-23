#!/usr/bin/python3
# eotsubset-check.py ORIGINAL.ttf EOT GLYPH...: test/eotsubset-gate.sh (wine-sg 9502). The font data of an
# EOT t2embed wrote with TTEMBED_SUBSET keeps every glyph index; exactly the named glyphs have outlines,
# the same as the original's, and the font loads.
# SPDX-License-Identifier: LGPL-2.1-or-later
import io
import struct
import sys
from fontTools.ttLib import TTFont

orig_path, eot_path, keep = sys.argv[1], sys.argv[2], set(sys.argv[3:])
data = open(eot_path, "rb").read()
eot_size, font_size = struct.unpack_from("<II", data, 0)
font = TTFont(io.BytesIO(data[eot_size - font_size:eot_size]))
orig = TTFont(orig_path)
bad = []
if font.getGlyphOrder() != orig.getGlyphOrder():
    bad.append("glyph order differs")
for name in orig.getGlyphOrder():
    g, o = font["glyf"][name], orig["glyf"][name]
    has = g.numberOfContours != 0
    if has != (name in keep):
        bad.append("%s %s" % (name, "kept" if has else "emptied"))
    elif has:
        if g.isComposite():
            same = [c.glyphName for c in g.components] == [c.glyphName for c in o.components]
        else:
            same = list(g.getCoordinates(font["glyf"])[0]) == list(o.getCoordinates(orig["glyf"])[0])
        if not same:
            bad.append("%s outline differs" % name)
if bad:
    print("FAIL  %s: %s" % (eot_path.split("/")[-1], ", ".join(bad)))
    sys.exit(1)
print("PASS  %s: %d glyph indices kept, outlines only for %s" % (eot_path.split("/")[-1], len(orig.getGlyphOrder()),
                                                               " ".join(sorted(keep))))
