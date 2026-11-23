#!/usr/bin/python3
# eotsubset-font.py OUT.ttf FAMILY FSTYPE: the font of test/eotsubset-gate.sh (wine-sg 9502). Glyphs:
# .notdef (a frame), A (a tall box), B (a wide box), C (a composite of A and a bar), D (a square) mapped
# from U+1F600 too, so the cmap has a format 12 subtable beside format 4.
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphComponent

order = [".notdef", "space", "A", "B", "C", "D", "bar"]
cmap = {0x20: "space", 0x41: "A", 0x42: "B", 0x43: "C", 0x44: "D", 0x1F600: "D"}


def box(pen, x0, y0, x1, y1):
    pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()


def simple(*boxes):
    pen = TTGlyphPen(None)
    for b in boxes:
        box(pen, *b)
    return pen.glyph()


glyphs = {".notdef": simple((50, 0, 550, 700), (100, 50, 500, 650)), "space": TTGlyphPen(None).glyph(),
          "A": simple((100, 0, 300, 700)), "B": simple((100, 0, 700, 200)), "D": simple((100, 0, 600, 500)),
          "bar": simple((0, 300, 600, 400))}
c = Glyph()
c.numberOfContours = -1
c.components = []
for name, x in (("A", 0), ("bar", 0)):
    comp = GlyphComponent()
    comp.glyphName, comp.x, comp.y, comp.flags = name, x, 0, 0x4
    c.components.append(comp)
glyphs["C"] = c
fb = FontBuilder(1000, isTTF=True)
fb.setupGlyphOrder(order)
fb.setupCharacterMap(cmap)
fb.setupGlyf(glyphs)
fb.setupHorizontalMetrics({g: (800, 0) for g in order})
fb.setupHorizontalHeader(ascent=800, descent=-200)
fb.setupNameTable({"familyName": sys.argv[2], "styleName": "Regular"})
fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200, fsType=int(sys.argv[3], 0))
fb.setupPost()
fb.save(sys.argv[1])
