#!/usr/bin/python3 -I
# dwbitmapmetrics-font.py OUT.ttf: a tiny font for the dwbitmapmetrics gate: no outline table at all,
# U+2605 only a PNG image (an sbix strike at 109 ppem) -- bitmap-only, as Noto Color Emoji (CBDT) is.
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
fb = FontBuilder(2048, isTTF=True)
fb.setupGlyphOrder(['.notdef', 'star'])
fb.setupCharacterMap({0x2605: 'star'})
empty = TTGlyphPen(None).glyph()
fb.setupGlyf({'.notdef': empty, 'star': empty})
fb.setupHorizontalMetrics({'.notdef': (1024, 0), 'star': (2550, 0)})
fb.setupHorizontalHeader(ascent=1900, descent=-500)
fb.setupNameTable({'familyName': 'SG Bitmap Glyph Test', 'styleName': 'Regular'})
fb.setupOS2(sTypoAscender=1900, sTypoDescender=-500, usWinAscent=1900, usWinDescent=500)
fb.setupPost()
sbix = newTable('sbix')
strike = Strike(ppem=109, resolution=72)
strike.glyphs['star'] = SbixGlyph(glyphName='star', graphicType='png ', originOffsetX=0, originOffsetY=0, imageData=png(136, 128, (255, 0, 0, 255)))
sbix.strikes[109] = strike
fb.font['sbix'] = sbix
del fb.font['glyf']; del fb.font['loca']
fb.font.save(sys.argv[1])
