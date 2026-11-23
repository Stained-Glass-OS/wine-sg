# imphash-expect.py FILE: the import hash of a PE file (wine-sg 9702's definition: "module.function" or
# "module.ordN", module lower case without .dll/.ocx/.sys, function lower case, comma-joined, MD5)
import hashlib, struct, sys
d = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec, optsize = struct.unpack_from('<H', d, pe + 6)[0], struct.unpack_from('<H', d, pe + 20)[0]
opt = pe + 24
is64 = struct.unpack_from('<H', d, opt)[0] == 0x20b
dd = opt + (112 if is64 else 96)
imp_rva = struct.unpack_from('<I', d, dd + 8)[0]
secs = [struct.unpack_from('<IIII', d, opt + optsize + 40 * i + 8) for i in range(nsec)]
def off(rva):
    for vsize, va, rsize, raw in secs:
        if va <= rva < va + max(vsize, rsize): return rva - va + raw
    raise ValueError(rva)
def cstr(o): return d[o:d.index(b'\0', o)].decode('latin-1')
parts = []
k = 0
while True:
    oft, _, _, name, ft = struct.unpack_from('<IIIII', d, off(imp_rva) + 20 * k)
    if not name: break
    mod = cstr(off(name)).lower()
    if mod[-4:] in ('.dll', '.ocx', '.sys'): mod = mod[:-4]
    t = off(oft or ft); step = 8 if is64 else 4
    while True:
        v = struct.unpack_from('<Q' if is64 else '<I', d, t)[0]
        if not v: break
        if v >> (63 if is64 else 31): parts.append('%s.ord%d' % (mod, v & 0xffff))
        else: parts.append('%s.%s' % (mod, cstr(off(v & 0x7fffffff) + 2).lower()))
        t += step
    k += 1
print(hashlib.md5(','.join(parts).encode()).hexdigest(), len(parts))
