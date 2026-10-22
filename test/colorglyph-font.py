#!/usr/bin/python3 -I
# colorglyph-font.py OUT.ttf: a tiny TrueType font for the colorglyph gate:
# "A" is an outline square, U+2605 is a 64x64 PNG glyph image (sbix strike at 64 ppem),
# solid red, its left origin at the image's bottom-left corner.
import struct, sys, zlib
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable
from fontTools.ttLib.tables.sbixStrike import Strike
from fontTools.ttLib.tables.sbixGlyph import Glyph as SbixGlyph

def png(w, h, rgba):
    raw = b''.join(b'\0' + bytes(rgba) * w for _ in range(h))
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))

def square(size):
    pen = TTGlyphPen(None)
    pen.moveTo((0, 0)); pen.lineTo((0, size)); pen.lineTo((size, size)); pen.lineTo((size, 0)); pen.closePath()
    return pen.glyph()

fb = FontBuilder(1024, isTTF=True)
order = ['.notdef', 'A', 'star']
fb.setupGlyphOrder(order)
fb.setupCharacterMap({0x41: 'A', 0x2605: 'star'})
empty = TTGlyphPen(None).glyph()
fb.setupGlyf({'.notdef': empty, 'A': square(800), 'star': empty})
fb.setupHorizontalMetrics({'.notdef': (512, 0), 'A': (1024, 0), 'star': (1024, 0)})
fb.setupHorizontalHeader(ascent=900, descent=-124)
fb.setupNameTable({'familyName': 'SG Color Glyph Test', 'styleName': 'Regular'})
fb.setupOS2(sTypoAscender=900, sTypoDescender=-124, usWinAscent=900, usWinDescent=124)
fb.setupPost()
sbix = newTable('sbix')
strike = Strike(ppem=64, resolution=72)
strike.glyphs['star'] = SbixGlyph(glyphName='star', graphicType='png ', originOffsetX=0, originOffsetY=0,
                                  imageData=png(64, 64, (255, 0, 0, 255)))
sbix.strikes[64] = strike
fb.font['sbix'] = sbix
fb.font.save(sys.argv[1])
