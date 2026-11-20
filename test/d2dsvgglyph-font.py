# d2dsvgglyph-font.py OUT.ttf: a test font for test/d2dsvgglyph-gate.sh (wine-sg 8227): glyphs A and B are
# squares; A also has an OpenType SVG glyph, a red rectangle 600x800 units on the baseline. 1000 units per em.
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable

def square(x0, y0, x1, y1):
    pen = TTGlyphPen(None)
    pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()
    return pen.glyph()

fb = FontBuilder(1000, isTTF=True)
order = [".notdef", "A", "B"]
fb.setupGlyphOrder(order)
fb.setupCharacterMap({0x41: "A", 0x42: "B"})
fb.setupGlyf({".notdef": square(0, 0, 500, 700), "A": square(100, 0, 500, 700), "B": square(100, 0, 500, 700)})
fb.setupHorizontalMetrics({".notdef": (600, 0), "A": (600, 100), "B": (600, 100)})
fb.setupHorizontalHeader(ascent=800, descent=-200)
fb.setupNameTable({"familyName": "SG Svg Glyph Test", "styleName": "Regular"})
fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
fb.setupPost()
svg = newTable("SVG ")
svg.docList = [('<svg xmlns="http://www.w3.org/2000/svg"><g id="glyph1">'
                '<rect x="0" y="-800" width="600" height="800" fill="#ff0000"/></g></svg>', 1, 1)]
svg.compressed = False
fb.font["SVG "] = svg
fb.save(sys.argv[1])
