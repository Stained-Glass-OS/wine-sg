#!/usr/bin/env python3
"""Stand-in wineserver that dies before it answers a new process's first request (patches/sg/9200).

    srvdeath-standin.py MODE VERSION TIMEOUT WINE ARGS...

Starts WINE ARGS... with WINESERVERSOCKET set to one end of a socket pair, as a wineserver hands
a new process its connection, and plays the server's opening: it sends the request pipe's write
end, tagged with the protocol VERSION.  Then, by MODE:
  read   reads the init_first_thread request and the fds the process sent, then dies
  noread reads the request, leaves the sent fds in the socket, then dies
  probe  (VERSION 0) only reports what the process printed: its "version mismatch 0/N" names N
"Dies" means every descriptor this stand-in holds is closed, as when a server is killed.
Prints "EXIT <status> <seconds>" or "HUNG <seconds>" (the process still ran after TIMEOUT
seconds and was killed).  The process's stderr goes to $SRVDEATH_STDERR (default: dropped).
"""
import array
import os
import select
import socket
import struct
import subprocess
import sys
import time


def main():
    mode, version, timeout = sys.argv[1], int(sys.argv[2]), float(sys.argv[3])
    cmd = sys.argv[4:]
    srv, cli = socket.socketpair(socket.AF_UNIX, socket.SOCK_STREAM)
    env = dict(os.environ, WINESERVERSOCKET=str(cli.fileno()))
    err = open(os.environ.get("SRVDEATH_STDERR", "/dev/null"), "wb")
    start = time.monotonic()
    proc = subprocess.Popen(cmd, env=env, pass_fds=[cli.fileno()], stdin=subprocess.DEVNULL,
                            stdout=subprocess.DEVNULL, stderr=err)
    cli.close()
    req_r, req_w = os.pipe()
    srv.sendmsg([struct.pack("<I", version)],
                [(socket.SOL_SOCKET, socket.SCM_RIGHTS, array.array("i", [req_w]))])
    os.close(req_w)
    held = []
    if mode in ("read", "noread"):
        # the request arrives once the process has sent its reply and wait pipes
        os.set_blocking(req_r, True)
        deadline = time.monotonic() + timeout
        data = b""
        while not data and time.monotonic() < deadline and proc.poll() is None:
            r, _, _ = select.select([req_r], [], [], 0.2)
            if r:
                data = os.read(req_r, 4096)
        if mode == "read":
            srv.setblocking(False)
            try:
                while True:
                    _, anc, _, _ = srv.recvmsg(64, socket.CMSG_SPACE(16))
                    for lvl, typ, payload in anc:
                        if lvl == socket.SOL_SOCKET and typ == socket.SCM_RIGHTS:
                            fds = array.array("i")
                            fds.frombytes(payload[:len(payload) - len(payload) % fds.itemsize])
                            held.extend(fds)
                    if not anc:
                        break
            except BlockingIOError:
                pass
    # the server dies: everything it holds is closed
    for fd in held:
        os.close(fd)
    os.close(req_r)
    srv.close()
    try:
        rc = proc.wait(timeout=max(0.1, timeout - (time.monotonic() - start)))
        print("EXIT %d %.1f" % (rc, time.monotonic() - start))
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        print("HUNG %.1f" % (time.monotonic() - start))
    sys.stdout.flush()


if __name__ == "__main__":
    main()
