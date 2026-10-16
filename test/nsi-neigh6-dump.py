#!/usr/bin/env python3
"""Write the bytes of a kernel RTM_GETNEIGH (AF_INET6) dump to argv[1]: the
stand-in nsiproxy.sys reads when SG_NSI_NEIGH6_DUMP names a file (a kernel
without IPv6 cannot make a real one). argv[2] is the interface index.
The entries, by the last byte of fe80::N:
  1 permanent, 2 reachable, 3 stale + router, 4 incomplete (no lladdr),
  5 failed, 6 delay, 7 probe, 8 noarp; and one AF_INET entry, which is not
  ours to report."""
import socket
import struct
import sys

NUD = dict(incomplete=0x01, reachable=0x02, stale=0x04, delay=0x08, probe=0x10,
           failed=0x20, noarp=0x40, permanent=0x80)
NTF_ROUTER = 0x80
RTM_NEWNEIGH = 28
NLMSG_DONE = 3
NDA_DST, NDA_LLADDR = 1, 2


def attr(kind, payload):
    length = 4 + len(payload)
    return struct.pack("=HH", length, kind) + payload + b"\0" * (-length % 4)


def neigh(family, ifindex, state, flags, dst, lladdr):
    body = struct.pack("=BBHiHBB", family, 0, 0, ifindex, state, flags, 0)
    body += attr(NDA_DST, dst)
    if lladdr:
        body += attr(NDA_LLADDR, lladdr)
    return struct.pack("=IHHII", 16 + len(body), RTM_NEWNEIGH, 2, 1, 0) + body


ifindex = int(sys.argv[2])
out = b""
for n, state, flags, mac in [
        (1, "permanent", 0, "00:11:22:33:44:55"), (2, "reachable", 0, "02:00:00:00:00:02"),
        (3, "stale", NTF_ROUTER, "02:00:00:00:00:03"), (4, "incomplete", 0, None),
        (5, "failed", 0, None), (6, "delay", 0, "02:00:00:00:00:06"),
        (7, "probe", 0, "02:00:00:00:00:07"), (8, "noarp", 0, "02:00:00:00:00:08")]:
    out += neigh(socket.AF_INET6, ifindex, NUD[state], flags,
                 socket.inet_pton(socket.AF_INET6, "fe80::%d" % n),
                 bytes.fromhex(mac.replace(":", "")) if mac else None)
# not an IPv6 entry, but with a 16-byte address so that only the family tells
out += neigh(socket.AF_INET, ifindex, NUD["permanent"], 0,
             socket.inet_pton(socket.AF_INET6, "::ffff:10.1.2.3"), bytes.fromhex("020000000099"))
out += struct.pack("=IHHII", 20, NLMSG_DONE, 2, 1, 0) + struct.pack("=i", 0)
open(sys.argv[1], "wb").write(out)
