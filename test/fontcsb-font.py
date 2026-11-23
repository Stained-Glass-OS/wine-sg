#!/usr/bin/python3
# fontcsb-font.py OUT.ttf: the font of test/fontcsb-gate.sh (wine-sg 9500). An Arabic font whose OS/2 table
# (version 4) has an empty ulCodePageRange1 -- only ulCodePageRange2's ASMO 708 bit -- and only the Arabic
# Unicode range bit, as Noto Kufi Arabic ships. Glyphs: .notdef and space empty, alef (U+0627) and beh
# (U+0628) solid boxes, so text drawn with the wrong code page leaves no ink.
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

UPEM = 1000
order = [".notdef", "space", "alef", "beh"]
cmap = {0x20: "space", 0x627: "alef", 0x628: "beh"}


def box(x0, y0, x1, y1):
    pen = TTGlyphPen(None)
    pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
    return pen.glyph()


glyphs = {".notdef": TTGlyphPen(None).glyph(), "space": TTGlyphPen(None).glyph(),
          "alef": box(100, 0, 500, 700), "beh": box(100, 0, 800, 300)}
fb = FontBuilder(UPEM, isTTF=True)
fb.setupGlyphOrder(order)
fb.setupCharacterMap(cmap)
fb.setupGlyf(glyphs)
fb.setupHorizontalMetrics({".notdef": (600, 0), "space": (300, 0), "alef": (600, 100), "beh": (900, 100)})
fb.setupHorizontalHeader(ascent=800, descent=-200)
fb.setupNameTable({"familyName": "SG NoCodepage", "styleName": "Regular"})
fb.setupOS2(version=4, sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200,
            ulUnicodeRange1=1 << 13, ulUnicodeRange2=0, ulUnicodeRange3=0, ulUnicodeRange4=0,
            ulCodePageRange1=0, ulCodePageRange2=1 << 19, usFirstCharIndex=0x20, usLastCharIndex=0x628)
fb.setupPost()
fb.font["OS/2"].ulCodePageRange1 = 0
fb.font["OS/2"].ulCodePageRange2 = 1 << 19
fb.font.recalcBBoxes = True
fb.save(sys.argv[1])
