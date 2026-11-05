#!/usr/bin/env python3
# overlayid-mkico.py OUT: an overlay icon as Windows draws them -- each frame full size, a red
# square over its bottom-left quarter, the rest transparent (frames 16, 32, 48, 96); the 256 frame's
# square is an eighth, as real overlays' big frames have smaller marks (OneDrive's is a fifth), so a
# 96-pixel picture scaled from it is not the 96-pixel frame
import struct, sys

def frame(n):
    q = n // 2 if n < 256 else n // 8
    rows = []
    for y in range(n - 1, -1, -1):          # bottom-up
        for x in range(n):
            red = x < q and y >= n - q
            rows.append(struct.pack('<BBBB', 0, 0, 255, 255) if red else b'\0\0\0\0')
    xor = b''.join(rows)
    mask_stride = ((n + 31) // 32) * 4
    andmask = b'\0' * mask_stride * n
    head = struct.pack('<IiiHHIIiiII', 40, n, n * 2, 1, 32, 0, len(xor) + len(andmask), 0, 0, 0, 0)
    return head + xor + andmask

sizes = [16, 32, 48, 96, 256]
data = [frame(n) for n in sizes]
out = struct.pack('<HHH', 0, 1, len(sizes))
offset = 6 + 16 * len(sizes)
for n, d in zip(sizes, data):
    out += struct.pack('<BBBBHHII', n % 256, n % 256, 0, 0, 1, 32, len(d), offset)
    offset += len(d)
out += b''.join(data)
open(sys.argv[1], 'wb').write(out)
