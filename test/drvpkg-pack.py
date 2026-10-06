#!/usr/bin/env python3
"""Packs files the way printer makers ship them, for the drvpkg gate:
    drvpkg-pack.py szdd FILE OUT        a file compressed for setup (name.ex_)
    drvpkg-pack.py cab OUT FILE...      a cabinet holding the files (stored)
Our own writer of the documented formats."""
import os, struct, sys

def szdd(path, out):
    data = open(path, 'rb').read()
    name = os.path.basename(path)
    hdr = b'SZDD\x88\xf0\x27\x33' + b'A' + name[-1:].encode() + struct.pack('<I', len(data))
    body = bytearray()
    for i in range(0, len(data), 8):
        chunk = data[i:i + 8]
        body.append((1 << len(chunk)) - 1)   # every item a literal byte
        body += chunk
    open(out, 'wb').write(hdr + body)

def cab(out, files):
    entries = [(os.path.basename(f), open(f, 'rb').read()) for f in files]
    blob = b''.join(d for _, d in entries)
    cffiles = b''
    off = 0
    for name, d in entries:
        cffiles += struct.pack('<IIHHHH', len(d), off, 0, 0x5021, 0, 0x20) + name.encode() + b'\0'
        off += len(d)
    blocks = [blob[i:i + 32768] for i in range(0, len(blob), 32768)] or [b'']
    cfdata = b''.join(struct.pack('<IHH', 0, len(b), len(b)) + b for b in blocks)
    header_len, folder_len = 36, 8
    coff_files = header_len + folder_len
    coff_data = coff_files + len(cffiles)
    total = coff_data + len(cfdata)
    header = b'MSCF' + struct.pack('<IIIIIBBHHHHH', 0, total, 0, coff_files, 0, 3, 1, 1, len(entries), 0, 0x1234, 0)
    folder = struct.pack('<IHH', coff_data, len(blocks), 0)
    open(out, 'wb').write(header + folder + cffiles + cfdata)

if sys.argv[1] == 'szdd':
    szdd(sys.argv[2], sys.argv[3])
else:
    cab(sys.argv[2], sys.argv[3:])
