#!/usr/bin/python3
# t2mtx-font.py OUT.ttf: the TrueType font of test/t2mtx-gate.sh (wine-sg 8231), built from this script
# alone so the gate compares what t2embed decompresses with the font the MTX test data was made from
# (test/t2mtx-font.eot.b64, compressed with an open-source MTX encoder, see test/t2mtx-gate.sh).
# It exercises the Compact Table Format: simple glyphs with every triplet size (small, 8-, 12- and 16-bit
# coordinate steps, on- and off-curve points), glyph programs whose pushes are bytes, words, negative
# values and repeated values (the push data hop codes), a composite glyph with its own program, an empty
# glyph, a cvt with values of every encoding, an hdmx with widths that differ from the scaled advances
# and a VDMX.
# SPDX-License-Identifier: LGPL-2.1-or-later
import array
import sys
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables import ttProgram
from fontTools.ttLib.tables._g_l_y_f import GlyphComponent

UPEM = 2048
order = [".notdef", "space", "A", "B", "C", "D", "E", "F"]
cmap = {0x20: "space", 0x41: "A", 0x42: "B", 0x43: "C", 0x44: "D", 0x45: "E", 0x46: "F"}


def box(pen, x0, y0, x1, y1):
    pen.moveTo((x0, y0)); pen.lineTo((x0, y1)); pen.lineTo((x1, y1)); pen.lineTo((x1, y0)); pen.closePath()


def prog(code):
    p = ttProgram.Program()
    p.fromBytecode(bytes(code))
    return p


glyphs = {}
pen = TTGlyphPen(None); box(pen, 100, 0, 900, 1400); glyphs[".notdef"] = pen.glyph()
glyphs["space"] = TTGlyphPen(None).glyph()
# A: small steps, quadratic off-curve points, a hole
pen = TTGlyphPen(None)
pen.moveTo((0, 0)); pen.lineTo((40, 0)); pen.lineTo((600, 1400)); pen.lineTo((660, 1400)); pen.lineTo((1200, 0))
pen.lineTo((1250, 0)); pen.lineTo((1255, 3)); pen.closePath()
pen.moveTo((400, 500)); pen.qCurveTo((620, 900), (800, 500)); pen.lineTo((790, 499)); pen.closePath()
glyphs["A"] = pen.glyph()
# B: 8-bit and 12-bit steps (up to 4095), negative ones
pen = TTGlyphPen(None)
pen.moveTo((-3000, -900)); pen.lineTo((-3000, 2000)); pen.qCurveTo((1000, 2600), (1095, 2000))
pen.lineTo((1000, 1200)); pen.lineTo((300, 1100)); pen.closePath()
glyphs["B"] = pen.glyph()
# C: 16-bit steps (more than 4095)
pen = TTGlyphPen(None)
pen.moveTo((-8000, -8000)); pen.lineTo((-8000, 9000)); pen.lineTo((9000, 9000)); pen.lineTo((8999, -8000))
pen.closePath()
glyphs["C"] = pen.glyph()
# D: only horizontal and vertical steps (the x-only and y-only triplets)
pen = TTGlyphPen(None)
pen.moveTo((0, 0)); pen.lineTo((0, 1279)); pen.lineTo((1279, 1279)); pen.lineTo((1279, 1)); pen.lineTo((1, 1))
pen.closePath()
glyphs["D"] = pen.glyph()
glyphs["E"] = TTGlyphPen(None).glyph()      # replaced by the composite below
glyphs["F"] = TTGlyphPen(None).glyph()      # an empty glyph that is not a space

fb = FontBuilder(UPEM, isTTF=True)
fb.setupGlyphOrder(order)
fb.setupCharacterMap(cmap)
fb.setupGlyf(glyphs)
advances = {".notdef": 1000, "space": 600, "A": 1300, "B": 1150, "C": 1700, "D": 1300, "E": 1300, "F": 777}
fb.setupHorizontalMetrics({g: (advances[g], 0) for g in order})
fb.setupHorizontalHeader(ascent=1900, descent=-500)
fb.setupNameTable({"familyName": "SG MTX Test", "styleName": "Regular"})
fb.setupOS2(sTypoAscender=1900, sTypoDescender=-500, usWinAscent=1900, usWinDescent=500, fsType=0)
fb.setupPost()
fb.setupMaxp()
fb.font["head"].created = fb.font["head"].modified = 3600000000

glyf = fb.font["glyf"]
# E: a composite of A and D, one shifted and one scaled, with its own program
comp = glyf["E"]
comp.numberOfContours = -1
c1 = GlyphComponent(); c1.glyphName = "A"; c1.x, c1.y = 0, 0; c1.flags = 0x0004  # ROUND_XY_TO_GRID
c2 = GlyphComponent(); c2.glyphName = "D"; c2.x, c2.y = 300, -200; c2.flags = 0x0004
c2.transform = [[0.5, 0], [0, 0.5]]
comp.components = [c1, c2]
comp.program = prog([0xb1, 3, 7, 0x2f, 0x2f])           # PUSHB 3 7, MDAP MDAP

# glyph programs: bytes; words; NPUSHB with repeats (A X A X A -> hop codes); NPUSHW with negatives
glyf["A"].program = prog([0xb2, 1, 2, 3, 0xb8, 0x01, 0x2c, 0x2f, 0x2f, 0x2f, 0x2f])
glyf["B"].program = prog([0x40, 9, 5, 1, 5, 2, 5, 3, 5, 4, 5, 0x41, 2, 0xff, 0x38, 0x7f, 0xff, 0x1f, 0x1f])
glyf["C"].program = prog([0xb9, 0x01, 0xf4, 0x02, 0x58, 0xb0, 250, 0x2f])
glyf["D"].program = prog([0x2f])                        # code without pushes
for g in order:
    glyf[g].recalcBounds(glyf)

cvt = newTable("cvt ")
cvt.values = array.array("h", [0, 5, 237, 238, 2159, 2160, -1, -237, -2159, -2160, 32767, -32768, 100, 90, 1000])
fb.font["cvt "] = cvt
fpgm = newTable("fpgm"); fpgm.program = prog([0xb0, 0, 0x2c, 0x2d]); fb.font["fpgm"] = fpgm   # FDEF 0 ENDF
prep = newTable("prep"); prep.program = prog([0xb0, 1, 0x1d]); fb.font["prep"] = prep

hdmx = newTable("hdmx")
hdmx.hdmx = {}
for ppem in (9, 12, 17, 33):
    widths = {}
    for i, g in enumerate(order):
        scaled = ((64 * ppem * advances[g] + UPEM // 2) // UPEM + 32) // 64
        widths[g] = scaled + (i % 3) - 1            # -1, 0, +1 from the scaled advance
    hdmx.hdmx[ppem] = widths
fb.font["hdmx"] = hdmx

vdmx = newTable("VDMX")
vdmx.version = 1
vdmx.ratRanges = [{"bCharSet": 1, "xRatio": 1, "yStartRatio": 1, "yEndRatio": 1, "groupIndex": 0},
                  {"bCharSet": 1, "xRatio": 0, "yStartRatio": 0, "yEndRatio": 0, "groupIndex": 1}]
vdmx.groups = [{ppem: (ppem * 1900 // 2048 + (ppem % 2), -(ppem * 500 // 2048) - (ppem % 3 == 0)) for ppem in range(8, 20)},
               {ppem: (ppem * 1900 // 2048, -(ppem * 500 // 2048)) for ppem in (10, 11, 15, 40)}]
vdmx.numRatios, vdmx.numRecs = len(vdmx.ratRanges), len(vdmx.groups)
fb.font["VDMX"] = vdmx

fb.font.save(sys.argv[1])
