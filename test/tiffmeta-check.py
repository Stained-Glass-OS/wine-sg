#!/usr/bin/python3
# tiffmeta-check.py FILE.tif: test/tiffmeta-gate.sh (wine-sg 9506). The first directory of a TIFF the GDI+
# probe saved holds ImageDescription, Artist, Orientation 3 and the private tag 0xC350 = 7, 9, and no Exif
# directory pointer (the Exif tag set is not written). Pure Python, no imaging library.
# SPDX-License-Identifier: LGPL-2.1-or-later
import struct
import sys

data = open(sys.argv[1], "rb").read()
order = {b"II": "<", b"MM": ">"}[data[:2]]
u16 = lambda o: struct.unpack_from(order + "H", data, o)[0]
u32 = lambda o: struct.unpack_from(order + "I", data, o)[0]
ifd = u32(4)
tags = {}
size = {1: 1, 2: 1, 3: 2, 4: 4, 5: 8, 7: 1}
for i in range(u16(ifd)):
    e = ifd + 2 + i * 12
    tag, typ, n = u16(e), u16(e + 2), u32(e + 4)
    nbytes = size.get(typ, 1) * n
    at = e + 8 if nbytes <= 4 else u32(e + 8)
    raw = data[at:at + nbytes]
    if typ == 2:
        tags[tag] = raw.rstrip(b"\0").decode("latin-1")
    elif typ == 3:
        tags[tag] = list(struct.unpack(order + "%dH" % n, raw))
    elif typ == 4:
        tags[tag] = list(struct.unpack(order + "%dI" % n, raw))
    else:
        tags[tag] = raw
want = {270: "SG TIFF description", 315: "g2d", 274: [3], 0xC350: [7, 9]}
bad = ["%d=%r (want %r)" % (t, tags.get(t), v) for t, v in want.items() if tags.get(t) != v]
if 34665 in tags:
    bad.append("an Exif IFD pointer was written")
if 256 not in tags or tags[256] != [8] and tags[256] != 8:
    bad.append("ImageWidth %r" % tags.get(256))
if bad:
    print("FAIL  the TIFF's directory: " + "; ".join(bad))
    sys.exit(1)
print("PASS  the TIFF's directory has ImageDescription, Artist, Orientation and tag 0xC350, no Exif pointer")
