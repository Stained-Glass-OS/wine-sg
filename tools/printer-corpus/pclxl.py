#!/usr/bin/env python3
"""Lists the operators of a PCL XL stream (after its PJL), from the public
PCL XL format: data types 0xc0-0xef with their values, attribute ids
(0xf8 byte / 0xf9 word), embedded data (0xfa dword length, 0xfb byte length)
and operators 0x41-0xbf.

    pclxl.py FILE [MAXOPS]
"""
import struct
import sys

OPS = {
    0x41: 'BeginSession', 0x42: 'EndSession', 0x43: 'BeginPage', 0x44: 'EndPage', 0x46: 'VendorUnique',
    0x47: 'Comment', 0x48: 'OpenDataSource', 0x49: 'CloseDataSource', 0x4f: 'BeginFontHeader',
    0x50: 'ReadFontHeader', 0x51: 'EndFontHeader', 0x52: 'BeginChar', 0x53: 'ReadChar', 0x54: 'EndChar',
    0x55: 'RemoveFont', 0x56: 'SetCharAttributes', 0x57: 'SetDefaultGS', 0x58: 'SetColorTreatment',
    0x5b: 'BeginStream', 0x5c: 'ReadStream', 0x5d: 'EndStream', 0x5e: 'ExecStream', 0x5f: 'RemoveStream',
    0x60: 'PopGS', 0x61: 'PushGS', 0x62: 'SetClipReplace', 0x63: 'SetBrushSource', 0x64: 'SetCharAngle',
    0x65: 'SetCharScale', 0x66: 'SetCharShear', 0x67: 'SetClipIntersect', 0x68: 'SetClipRectangle',
    0x69: 'SetClipToPage', 0x6a: 'SetColorSpace', 0x6b: 'SetCursor', 0x6c: 'SetCursorRel',
    0x6d: 'SetHalftoneMethod', 0x6e: 'SetFillMode', 0x6f: 'SetFont', 0x70: 'SetLineDash', 0x71: 'SetLineCap',
    0x72: 'SetLineJoin', 0x73: 'SetMiterLimit', 0x74: 'SetPageDefaultCTM', 0x75: 'SetPageOrigin',
    0x76: 'SetPageRotation', 0x77: 'SetPageScale', 0x78: 'SetPatternTxMode', 0x79: 'SetPenSource',
    0x7a: 'SetPenWidth', 0x7b: 'SetROP', 0x7c: 'SetSourceTxMode', 0x7d: 'SetCharBoldValue',
    0x7e: 'SetNeutralAxis', 0x7f: 'SetClipMode', 0x80: 'SetPathToClip', 0x81: 'SetCharSubMode',
    0x84: 'CloseSubPath', 0x85: 'NewPath', 0x86: 'PaintPath', 0x91: 'ArcPath', 0x92: 'SetColorTrapping',
    0x93: 'BezierPath', 0x94: 'SetAdaptiveHalftoning', 0x95: 'BezierRelPath', 0x96: 'Chord', 0x97: 'ChordPath',
    0x98: 'Ellipse', 0x99: 'EllipsePath', 0x9b: 'LinePath', 0x9c: 'Pie', 0x9d: 'PiePath', 0x9e: 'Rectangle',
    0x9f: 'RectanglePath', 0xa0: 'RoundRectangle', 0xa1: 'RoundRectanglePath', 0xa8: 'Text', 0xa9: 'TextPath',
    0xb0: 'BeginImage', 0xb1: 'ReadImage', 0xb2: 'EndImage', 0xb3: 'BeginRastPattern', 0xb4: 'ReadRastPattern',
    0xb5: 'EndRastPattern', 0xb6: 'BeginScan', 0xb8: 'EndScan', 0xb9: 'ScanLineRel',
}
SIZES = {0xc0: 1, 0xc1: 2, 0xc2: 4, 0xc3: 2, 0xc4: 4, 0xc5: 4}  # ubyte uint16 uint32 sint16 sint32 real32


def parse(data, maxops=10 ** 9):
    i = data.find(b') HP-PCL XL')
    if i < 0:
        raise SystemExit('no PCL XL stream header')
    nl = data.index(b'\n', i)
    big = data[i - 1:i] == b'('   # '(' big endian, ')' little endian
    p = nl + 1
    end = '>' if big else '<'
    ops = []
    args = []
    while p < len(data) and len(ops) < maxops:
        t = data[p]
        if 0xc0 <= t <= 0xc5:
            n = SIZES[t]
            args.append(('v', data[p + 1:p + 1 + n]))
            p += 1 + n
        elif 0xc8 <= t <= 0xcd:          # arrays: type, length (data type + value), items
            n = SIZES[t - 8]
            lt = data[p + 1]
            ln = data[p + 2] if lt == 0xc0 else struct.unpack(end + 'H', data[p + 2:p + 4])[0]
            hl = 2 if lt == 0xc0 else 3
            args.append(('a', ln))
            p += 1 + hl + ln * n
        elif 0xd0 <= t <= 0xd5:          # xy
            n = SIZES[t - 0x10]
            p += 1 + 2 * n
            args.append(('xy', 0))
        elif 0xe0 <= t <= 0xe5:          # box
            n = SIZES[t - 0x20]
            p += 1 + 4 * n
            args.append(('box', 0))
        elif t == 0xf8:
            args.append(('attr', data[p + 1]))
            p += 2
        elif t == 0xf9:
            args.append(('attr', struct.unpack(end + 'H', data[p + 1:p + 3])[0]))
            p += 3
        elif t == 0xfa:
            n = struct.unpack(end + 'I', data[p + 1:p + 5])[0]
            ops.append(('data', n))
            p += 5 + n
        elif t == 0xfb:
            n = data[p + 1]
            ops.append(('data', n))
            p += 2 + n
        elif t in OPS:
            ops.append((OPS[t], len([a for a in args if a[0] == 'attr'])))
            args = []
            p += 1
        elif t in (0x00, 0x09, 0x0a, 0x0d, 0x20):
            p += 1
        elif t == 0x1b:                   # UEL: the end of the PCL XL
            ops.append(('UEL', p))
            break
        else:
            ops.append(('?%02x@%d' % (t, p), 0))
            break
    return ops


if __name__ == '__main__':
    data = open(sys.argv[1], 'rb').read()
    for name, n in parse(data, int(sys.argv[2]) if len(sys.argv) > 2 else 10 ** 9):
        print(name, n)
