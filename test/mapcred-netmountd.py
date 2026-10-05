#!/usr/bin/env python3
"""A stand-in for sg-netmountd (sg-session), for test/mapcred-gate.sh: one
request line a connection on SOCKET. Every share refuses a connection without
a name and password (MAP/MOUNT -> ERR 13) until LOGON gives PASSWORD; then
MAP links the user's letter to TARGET. Logs the requests (never passwords)."""
import os
import socket
import sys

sock_path, drives, target, password, log = sys.argv[1:6]
logged_on = set()
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(sock_path)
os.chmod(sock_path, 0o777)
srv.listen(8)
while True:
    conn, _ = srv.accept()
    data = b""
    while not data.endswith(b"\n"):
        chunk = conn.recv(4096)
        if not chunk:
            break
        data += chunk
    f = data.decode("utf-8", "replace").rstrip("\n").split("\t")
    cmd = f[0]
    reply = "ERR 22 bad request"
    if cmd == "LOGON" and len(f) >= 5:
        key = (f[1].lower(), f[2].lower())
        if "\t".join(f[4:]) == password:
            logged_on.add(key)
            reply = "OK " + target
        else:
            reply = "ERR 13 refused"
        line = "LOGON %s %s %s" % (f[1], f[2], f[3])
    elif cmd in ("MAP", "MOUNT") and len(f) >= 3:
        letter, server, share = (f[1], f[2], f[3]) if cmd == "MAP" else (None, f[1], f[2])
        if (server.lower(), share.lower()) in logged_on:
            if letter:
                os.symlink(target, os.path.join(drives, letter.lower() + ":"))
            reply = "OK " + target
        else:
            reply = "ERR 13 refused"
        line = " ".join(f)
    else:
        line = " ".join(f[:3])
    with open(log, "a") as fh:
        fh.write("%s -> %s\n" % (line, reply.split()[0]))
    conn.sendall((reply + "\n").encode())
    conn.close()
