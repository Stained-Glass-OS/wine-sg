#!/usr/bin/env python3
# regrestore-gate.sh's hive writer (patches/sg/0506): a Windows binary
# registry hive ("regf") from a description, written by us for the gate --
# nested keys with compressed (Latin-1) and UTF-16 names, lh, li and ri
# subkey lists, and values held inline (<= 4 bytes), in a cell, and as big
# data (db, over 16344 bytes).
#
#   regf-write.py OUT
import struct, sys

class Hive:
    def __init__(self):
        # cell offsets count from the first hbin; its 32-byte header comes first
        self.bins = bytearray(32)
    def cell(self, data):
        size = (len(data) + 4 + 7) & ~7
        off = len(self.bins)
        self.bins += struct.pack('<i', -size) + data + b'\0' * (size - 4 - len(data))
        return off

def key(h, name, values, subkeys, listkind='lh', compressed=True):
    offs = []
    for sk in subkeys:
        offs.append(key(h, *sk))
    vk_offs = []
    for vname, vtype, data in values:
        n = vname.encode('latin-1')
        size = len(data)
        if size <= 4:
            dword = struct.unpack('<I', (data + b'\0' * 4)[:4])[0]
            vk = b'vk' + struct.pack('<HIIIHH', len(n), size | 0x80000000, dword, vtype, 1, 0) + n
        elif size > 16344:
            segs = [h.cell(data[i:i + 16344]) for i in range(0, size, 16344)]
            lst = h.cell(b''.join(struct.pack('<I', s) for s in segs))
            db = h.cell(b'db' + struct.pack('<HI', len(segs), lst))
            vk = b'vk' + struct.pack('<HIIIHH', len(n), size, db, vtype, 1, 0) + n
        else:
            d = h.cell(data)
            vk = b'vk' + struct.pack('<HIIIHH', len(n), size, d, vtype, 1, 0) + n
        vk_offs.append(h.cell(vk))
    vlist = h.cell(b''.join(struct.pack('<I', o) for o in vk_offs)) if vk_offs else 0xffffffff
    if not offs:
        slist = 0xffffffff
    elif listkind == 'li':
        slist = h.cell(b'li' + struct.pack('<H', len(offs)) + b''.join(struct.pack('<I', o) for o in offs))
    elif listkind == 'ri':
        half = len(offs) // 2
        a = h.cell(b'li' + struct.pack('<H', half) + b''.join(struct.pack('<I', o) for o in offs[:half]))
        b = h.cell(b'lh' + struct.pack('<H', len(offs) - half) + b''.join(struct.pack('<II', o, 0) for o in offs[half:]))
        slist = h.cell(b'ri' + struct.pack('<H', 2) + struct.pack('<II', a, b))
    else:
        slist = h.cell(b'lh' + struct.pack('<H', len(offs)) + b''.join(struct.pack('<II', o, 0) for o in offs))
    nm = name.encode('latin-1') if compressed else name.encode('utf-16-le')
    flags = 0x20 if compressed else 0
    nk = (b'nk' + struct.pack('<H', flags) + b'\0' * 8 + struct.pack('<I', 0) + struct.pack('<I', 0xffffffff) +
          struct.pack('<II', len(offs), 0) + struct.pack('<II', slist, 0xffffffff) +
          struct.pack('<II', len(vk_offs), vlist) + struct.pack('<II', 0xffffffff, 0xffffffff) +
          b'\0' * 20 + struct.pack('<HH', len(nm), 0) + nm)
    return h.cell(nk)

def sz(s): return (s + '\0').encode('utf-16-le')

h = Hive()
big = bytes((i * 7) & 0xff for i in range(40000))
tree = ('ROOT', [('RootValue', 1, sz('at the root'))], [
    ('Components', [('Path', 1, sz('C:\\Program Files\\Test\\core.dll')), ('Count', 4, struct.pack('<I', 42))], [
        ('A8EF02CD', [('', 1, sz('default value'))], [], 'lh'),
        ('Deep', [], [('Deeper', [('Leaf', 3, b'\x01\x02\x03\x04\x05\x06')], [], 'lh')], 'li'),
    ], 'li'),
    ('Big', [('Blob', 3, big), ('Multi', 7, sz('one') + sz('two') + b'\0\0')], [], 'lh'),
    ('Ünïcode Kéy', [('Small', 3, b'\xab\xcd')], [], 'lh', False),
    ('Ri1', [], [], 'lh'), ('Ri2', [], [], 'lh'), ('Ri3', [], [], 'lh'), ('Ri4', [], [], 'lh'),
], 'ri')
root = key(h, *tree)
while len(h.bins) % 4096: h.bins += b'\0'
hbin = h.bins
hbin[0:32] = b'hbin' + struct.pack('<III', 0, len(hbin), 0) + b'\0' * 16
base = bytearray(4096)
base[0:4] = b'regf'
struct.pack_into('<IIIIIIII', base, 4, 1, 1, 0, 0, 1, 5, 0, 1)
struct.pack_into('<II', base, 0x24, root, len(hbin))
# the base block's checksum: the XOR of its first 127 dwords
x = 0
for i in range(0, 0x1fc, 4): x ^= struct.unpack_from('<I', base, i)[0]
struct.pack_into('<I', base, 0x1fc, x)
open(sys.argv[1], 'wb').write(bytes(base) + bytes(hbin))
