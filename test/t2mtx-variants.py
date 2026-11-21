#!/usr/bin/python3
# t2mtx-variants.py EOT XORED CORRUPT: test/t2mtx-gate.sh's XOR-encrypted copy of an EOT (flag 0x10000000,
# font data XORed with 0x50) and a copy whose MTX data is corrupt (a cut-off LZCOMP stream)
# SPDX-License-Identifier: LGPL-2.1-or-later
import sys
src = bytearray(open(sys.argv[1], 'rb').read())
eot_size = int.from_bytes(src[0:4], 'little')
font_size = int.from_bytes(src[4:8], 'little')
start = eot_size - font_size
x = bytearray(src)
x[12:16] = (int.from_bytes(x[12:16], 'little') | 0x10000000).to_bytes(4, 'little')
for i in range(start, eot_size):
    x[i] ^= 0x50
open(sys.argv[2], 'wb').write(x)
c = bytearray(src)
for i in range(start + 12, eot_size):
    c[i] = 0xff     # the bit stream of block 1 garbled (the MTX header kept)
open(sys.argv[3], 'wb').write(c)
