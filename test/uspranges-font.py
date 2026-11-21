#!/usr/bin/python3 -I
# uspranges-font.py OUT.ttf: the font of test/uspranges-gate.sh (wine-sg 8233), family "SG Range Test".
# Glyph ids by position: 0 .notdef, 1 space, 2 A, 3 B, 4 C, 5 a, 6 a.sc, 7 D, 8 a.alt1, 9 a.alt2, 10 a.alt3.
# "liga" (default) A B -> C; "smcp" a -> a.sc; "salt" a -> one of a.alt1..3; "kern" (default) A D -100.
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString

def box(w):
    pen = TTGlyphPen(None)
    pen.moveTo((50, 0)); pen.lineTo((50, 700)); pen.lineTo((w - 50, 700)); pen.lineTo((w - 50, 0)); pen.closePath()
    return pen.glyph()

order = ['.notdef', 'space', 'A', 'B', 'C', 'a', 'a.sc', 'D', 'a.alt1', 'a.alt2', 'a.alt3']
fb = FontBuilder(1000, isTTF=True)
fb.setupGlyphOrder(order)
fb.setupCharacterMap({0x20: 'space', 0x41: 'A', 0x42: 'B', 0x43: 'C', 0x61: 'a', 0x44: 'D'})
fb.setupGlyf({g: (TTGlyphPen(None).glyph() if g in ('.notdef', 'space') else box(600)) for g in order})
fb.setupHorizontalMetrics({g: (600, 0) for g in order})
fb.setupHorizontalHeader(ascent=800, descent=-200)
fb.setupNameTable({'familyName': 'SG Range Test', 'styleName': 'Regular'})
fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200, fsType=0)
fb.setupPost()
addOpenTypeFeaturesFromString(fb.font, """
languagesystem DFLT dflt;
languagesystem latn dflt;
feature liga { sub A B by C; } liga;
feature smcp { sub a by a.sc; } smcp;
feature salt { sub a from [a.alt1 a.alt2 a.alt3]; } salt;
feature kern { pos A D -100; } kern;
""")
fb.font.save(sys.argv[1])
