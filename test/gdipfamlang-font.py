#!/usr/bin/python3
# gdipfamlang-font.py OUT.ttf: the font of test/gdipfamlang-gate.sh (wine-sg 9508): one glyph, its family
# named in English (0x409, "SG Lang Test"), German (0x407, "SG Sprachtest") and Japanese (0x411).
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen

pen = TTGlyphPen(None)
pen.moveTo((100, 0)); pen.lineTo((100, 700)); pen.lineTo((500, 700)); pen.lineTo((500, 0)); pen.closePath()
fb = FontBuilder(1000, isTTF=True)
fb.setupGlyphOrder([".notdef", "A"])
fb.setupCharacterMap({0x41: "A"})
fb.setupGlyf({".notdef": TTGlyphPen(None).glyph(), "A": pen.glyph()})
fb.setupHorizontalMetrics({".notdef": (600, 0), "A": (600, 100)})
fb.setupHorizontalHeader(ascent=800, descent=-200)
fb.setupNameTable({"familyName": {"en": "SG Lang Test", "de": "SG Sprachtest", "ja": "SG 言語テスト"},
                   "styleName": "Regular", "fullName": "SG Lang Test", "psName": "SGLangTest",
                   "uniqueFontIdentifier": "SG Lang Test 1.0"}, mac=False)
fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
fb.setupPost()
fb.save(sys.argv[1])
