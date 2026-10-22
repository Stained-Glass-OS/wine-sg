#!/usr/bin/python3 -I
# uspfeat-font.py OUT.ttf [FSTYPE FAMILY]: a tiny TrueType font for the uspfeat (and eot) gates,
# family "SG Feature Test" (or FAMILY), embedding permissions fsType 0 (or FSTYPE):
# "liga" (on by default for Latin) makes A B one glyph C; "smcp" (off by default) makes a "a.sc";
# "kern" (a default position feature) moves B 100 units nearer A.
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.feaLib.builder import addOpenTypeFeaturesFromString

def box(w):
    pen = TTGlyphPen(None)
    pen.moveTo((50, 0)); pen.lineTo((50, 700)); pen.lineTo((w - 50, 700)); pen.lineTo((w - 50, 0)); pen.closePath()
    return pen.glyph()

order = ['.notdef', 'space', 'A', 'B', 'C', 'a', 'a.sc', 'D']
fb = FontBuilder(1000, isTTF=True)
fb.setupGlyphOrder(order)
fb.setupCharacterMap({0x20: 'space', 0x41: 'A', 0x42: 'B', 0x43: 'C', 0x61: 'a', 0x44: 'D'})
fb.setupGlyf({g: (TTGlyphPen(None).glyph() if g in ('.notdef', 'space') else box(600)) for g in order})
fb.setupHorizontalMetrics({g: (600, 0) for g in order})
fb.setupHorizontalHeader(ascent=800, descent=-200)
family = sys.argv[3] if len(sys.argv) > 3 else 'SG Feature Test'
fb.setupNameTable({'familyName': family, 'styleName': 'Regular'})
fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200,
            fsType=int(sys.argv[2]) if len(sys.argv) > 2 else 0)
fb.setupPost()
addOpenTypeFeaturesFromString(fb.font, """
languagesystem DFLT dflt;
languagesystem latn dflt;
feature liga { sub A B by C; } liga;
feature smcp { sub a by a.sc; } smcp;
feature kern { pos A D -100; } kern;
""")
fb.font.save(sys.argv[1])
