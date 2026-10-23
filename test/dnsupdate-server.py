#!/usr/bin/env python3
# A small authoritative DNS server for test/dnsupdate-gate.sh, run inside the gate's own network namespace on
# 127.0.0.1:53: zone sg.test (primary ns1.sg.test = 127.0.0.1) takes RFC 2136 updates; zone locked.test refuses
# them. Writes the zone's records to LOG after each update, as JSON lines.
# SPDX-License-Identifier: LGPL-2.1-or-later
import json, socket, struct, sys

LOG = sys.argv[1]
ZONES = {'sg.test': True, 'locked.test': False}
store = {('ns1.sg.test', 1): [socket.inet_aton('127.0.0.1')], ('ns1.locked.test', 1): [socket.inet_aton('127.0.0.1')]}
updates = 0

def soa(zone):
    def name(n):
        return b''.join(bytes([len(l)]) + l.encode() for l in n.split('.')) + b'\0'
    return name('ns1.' + zone) + name('admin.' + zone) + struct.pack('>IIIII', 1, 3600, 600, 86400, 60)

def read_name(msg, off):
    labels = []
    while True:
        n = msg[off]
        if n & 0xc0 == 0xc0:
            ptr = struct.unpack('>H', msg[off:off + 2])[0] & 0x3fff
            labels.append(read_name(msg, ptr)[0])
            return '.'.join(l for l in labels if l), off + 2
        off += 1
        if not n:
            return '.'.join(labels), off
        labels.append(msg[off:off + n].decode())
        off += n

def enc_name(n):
    return b''.join(bytes([len(l)]) + l.encode() for l in n.split('.') if l) + b'\0'

def zone_of(n):
    for z in ZONES:
        if n == z or n.endswith('.' + z):
            return z
    return None

QUESTION = b''

def answer(msg, rcode, answers=b'', count=0, opcode=0):
    # the question (or, for an update, the zone) is echoed, as resolvers check it
    hdr = struct.pack('>HHHHHH', struct.unpack('>H', msg[:2])[0], 0x8400 | (opcode << 11) | rcode, 1, count, 0, 0)
    return hdr + QUESTION + answers

def handle(msg):
    global updates
    qid, flags, qd, an, ns, ar = struct.unpack('>HHHHHH', msg[:12])
    opcode = (flags >> 11) & 0xf
    off = 12
    name, off = read_name(msg, off)
    qtype, qclass = struct.unpack('>HH', msg[off:off + 4]); off += 4
    global QUESTION
    QUESTION = msg[12:off]
    name = name.lower().rstrip('.')
    if opcode == 0:
        z = zone_of(name)
        if not z:
            return answer(msg, 5)
        if qtype == 6:
            if name == z:
                rd = soa(z)
                return answer(msg, 0, enc_name(name) + struct.pack('>HHIH', 6, 1, 60, len(rd)) + rd, 1)
            return answer(msg, 0 if any(k[0] == name for k in store) else 3)
        rrs = store.get((name, qtype), [])
        out = b''.join(enc_name(name) + struct.pack('>HHIH', qtype, 1, 60, len(rd)) + rd for rd in rrs)
        if not rrs:
            return answer(msg, 0 if any(k[0] == name for k in store) else 3)
        return answer(msg, 0, out, len(rrs))
    if opcode == 5:
        z = name
        if z not in ZONES:
            return answer(msg, 9, opcode=5)
        if not ZONES[z]:
            return answer(msg, 5, opcode=5)
        for _ in range(an):     # prerequisites: none expected
            pass
        for _ in range(ns):
            n, off = read_name(msg, off)
            t, c, ttl, l = struct.unpack('>HHIH', msg[off:off + 10]); off += 10
            rd = msg[off:off + l]; off += l
            n = n.lower().rstrip('.')
            if zone_of(n) != z:
                return answer(msg, 10, opcode=5)
            # names inside rdata are uncompressed in our updates: keep the bytes as they are
            if c == 1:
                lst = store.setdefault((n, t), [])
                if rd not in lst:
                    lst.append(rd)
            elif c == 254:
                if rd in store.get((n, t), []):
                    store[(n, t)].remove(rd)
            elif c == 255:
                for k in [k for k in store if k[0] == n and (t == 255 or k[1] == t)]:
                    del store[k]
        updates += 1
        with open(LOG, 'a') as f:
            f.write(json.dumps({'updates': updates, 'records': {'%s/%d' % k: [r.hex() for r in v] for k, v in store.items() if v}}) + '\n')
        return answer(msg, 0, opcode=5)
    return answer(msg, 4)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('127.0.0.1', 53))
print('ready', flush=True)
while True:
    msg, peer = s.recvfrom(4096)
    try:
        s.sendto(handle(msg), peer)
    except Exception as e:
        print('error', e, flush=True)
