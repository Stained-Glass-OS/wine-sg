#!/usr/bin/python3
# A stand-in for sg-session's sg-defender, for test/defender-exec-gate.sh: the
# same socket protocol ("SCAN1 <kind> <path>\n" with the file's descriptor
# passed, "ALLOW" or "BLOCK <id> <signature>" back). It finds the EICAR test
# string -- never real malware -- anywhere in the file, and logs each
# request: kind, the descriptor's open mode, whether it is the caller's own
# file description, the peer's uid, the path the caller named.
#   defender-exec-service.py SOCKET LOG [DELAY_SECONDS]
import array
import fcntl
import os
import socket
import struct
import sys
import threading
import time

SOCK, LOG = sys.argv[1], sys.argv[2]
DELAY = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
EICAR = b"EICAR-STANDARD-ANTIVIRUS-TEST-FILE"


def serve(c):
    try:
        pid, uid, gid = struct.unpack("3i", c.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
        msg, anc, _, _ = c.recvmsg(4096, socket.CMSG_SPACE(4))
        fds = []
        for lvl, typ, data in anc:
            if lvl == socket.SOL_SOCKET and typ == socket.SCM_RIGHTS:
                a = array.array("i")
                a.frombytes(data)
                fds += list(a)
        parts = msg.decode(errors="replace").rstrip("\n").split(" ", 2)
        kind, hint = (parts[1], parts[2] if len(parts) > 2 else "") if len(parts) > 1 else ("?", "")
        out = "ALLOW"
        if fds:
            fd = fds[0]
            fl = fcntl.fcntl(fd, fcntl.F_GETFL)
            mode = {os.O_RDONLY: "r", os.O_WRONLY: "w", os.O_RDWR: "rw"}[fl & os.O_ACCMODE]
            data, off = b"", 0
            while True:
                b = os.pread(fd, 1 << 20, off)
                if not b:
                    break
                data += b
                off += len(b)
            os.lseek(fd, 0, os.SEEK_END)      # (the caller's own handle must not see this)
            where = os.readlink("/proc/self/fd/%d" % fd)
            with open(LOG, "a") as f:
                f.write("%s mode=%s uid=%d size=%d fd=%s hint=%s\n" % (kind, mode, uid, len(data), where, hint))
            time.sleep(DELAY)
            if EICAR in data:
                out = "BLOCK 20261125-000000-abcdef Eicar-Test-Signature"
        for fd in fds:
            os.close(fd)
        c.sendall(out.encode() + b"\n")
    except OSError:
        pass
    finally:
        c.close()


try:
    os.unlink(SOCK)
except OSError:
    pass
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
s.bind(SOCK)
s.listen(16)
while True:
    conn, _ = s.accept()
    threading.Thread(target=serve, args=(conn,), daemon=True).start()
