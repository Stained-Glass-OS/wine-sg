#!/usr/bin/python3
# t2mtx-fontcmp.py ORIGINAL LOADED: test/t2mtx-gate.sh compares the font t2embed decompressed from MTX
# with the original: every table but glyf/loca byte for byte (head without its checksum adjustment and loca
# format), the glyphs by outline, components and instructions (pushed values, then the code: the push
# instructions themselves may be written differently)
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
from fontTools.ttLib import TTFont

def program(glyph):
    """(pushed values of the leading pushes, the rest of the code)"""
    if not hasattr(glyph, 'program') or not glyph.program: return ([], b'')
    code = bytes(glyph.program.getBytecode())
    vals, i = [], 0
    while i < len(code):
        op = code[i]
        if op == 0x40 or op == 0x41:
            if i + 1 >= len(code): break
            n = code[i + 1]; w = op == 0x41; j = i + 2
        elif 0xb0 <= op <= 0xbf:
            n = (op & 7) + 1; w = op >= 0xb8; j = i + 1
        else: break
        size = 2 if w else 1
        if j + n * size > len(code): break
        for k in range(n):
            if w: vals.append(int.from_bytes(code[j + 2*k:j + 2*k + 2], 'big', signed=True))
            else: vals.append(code[j + k])
        i = j + n * size
    return (vals, code[i:])

def main(a, b, skip=()):
    fa, fb = TTFont(a), TTFont(b)
    bad = 0
    for tag in sorted(set(fa.keys()) | set(fb.keys())):
        if tag in ('GlyphOrder',) or tag in skip: continue
        if tag not in fa.reader or tag not in fb.reader:
            print('table %s only in one font' % tag); bad += 1; continue
        if tag in ('glyf', 'loca'): continue
        da, db = fa.reader[tag], fb.reader[tag]
        if tag == 'head':
            da = da[:8] + b'\0'*4 + da[12:50] + da[52:]; db = db[:8] + b'\0'*4 + db[12:50] + db[52:]
        if da != db: print('table %s differs (%d / %d bytes)' % (tag, len(da), len(db))); bad += 1
    ga, gb = fa['glyf'], fb['glyf']
    for name in fa.getGlyphOrder():
        x, y = ga[name], gb[name]
        if x.numberOfContours != y.numberOfContours: print(name, 'contours', x.numberOfContours, y.numberOfContours); bad += 1; continue
        if x.numberOfContours > 0:
            if list(x.coordinates) != list(y.coordinates) or list(x.endPtsOfContours) != list(y.endPtsOfContours) or \
               [f & 1 for f in x.flags] != [f & 1 for f in y.flags]:
                print(name, 'outline differs'); bad += 1
        elif x.isComposite():
            ca = [(c.glyphName, c.flags & ~0x0020 & ~0x0100, getattr(c, 'x', None), getattr(c, 'y', None), getattr(c, 'transform', None), getattr(c, 'firstPt', None)) for c in x.components]
            cb = [(c.glyphName, c.flags & ~0x0020 & ~0x0100, getattr(c, 'x', None), getattr(c, 'y', None), getattr(c, 'transform', None), getattr(c, 'firstPt', None)) for c in y.components]
            if ca != cb: print(name, 'components differ', ca, cb); bad += 1
            if (x.xMin, x.yMin, x.xMax, x.yMax) != (y.xMin, y.yMin, y.xMax, y.yMax): print(name, 'composite bbox differs'); bad += 1
        if x.numberOfContours and program(x) != program(y): print(name, 'instructions differ'); bad += 1
        if bad > 20: break
    print('glyphs %d, differences %d' % (len(fa.getGlyphOrder()), bad))
    return bad

if __name__ == '__main__':
    sys.exit(1 if main(sys.argv[1], sys.argv[2], sys.argv[3:]) else 0)
