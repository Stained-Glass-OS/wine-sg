#!/usr/bin/env python3
"""A stand-in for sg-netmountd (sg-session), for test/netreach-gate.sh: one request
line a connection on SOCKET, answered as a network with one file server, SRV:
  - any other computer is not there: ERR 113 (EHOSTUNREACH);
  - SRV's IPC$ is no folder (ERR 13), "open" mounts for anyone, "nosuch" is no share
    of SRV's (ERR 2, ENOENT), every other share refuses a guest (ERR 13) until LOGON
    gives PASSWORD for it.
A share that mounts for everyone is a directory under ROOT/unc/<server>/<share>; a LOGON
mounts a tmpfs on ROOT/users/<UID>/unc/<server>/<share> (ntdll looks there first and wants a
mount point), as sg-netmountd does. Runs as root in the gate's own mount namespace.
Logs the requests (never passwords)."""
import os
import socket
import subprocess
import sys

sock_path, root, password, log, uid = sys.argv[1:6]
uid = int(uid)
logged_on = set()
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(sock_path)
os.chmod(sock_path, 0o777)
srv.listen(8)


def logged_mount(server, share):
    path = os.path.join(root, "users", str(uid), "unc", server, share)
    os.makedirs(path, exist_ok=True)
    if not os.path.ismount(path):
        subprocess.run(["mount", "-t", "tmpfs", "-o", "size=1m,uid=%d" % uid, "tmpfs", path], check=True)
    with open(os.path.join(path, "hello.txt"), "w") as fh:
        fh.write("hello\n")
    for d, _, files in os.walk(os.path.join(root, "users", str(uid))):
        os.chown(d, uid, -1)
        for f in files:
            os.chown(os.path.join(d, f), uid, -1)
    return "OK " + path


def mounted(server, share):
    path = os.path.join(root, "unc", server, share)
    os.makedirs(path, exist_ok=True)
    with open(os.path.join(path, "hello.txt"), "w") as fh:
        fh.write("hello\n")
    return "OK " + path


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
    line = " ".join(f[:3])
    if cmd in ("MOUNT", "LOGON") and len(f) >= 3:
        server, share = f[1].lower(), f[2].lower()
        if server != "srv":
            reply = "ERR 113 cannot mount"
        elif share == "nosuch":
            reply = "ERR 2 cannot mount"
        elif share == "ipc$":
            reply = "ERR 13 cannot mount"
        elif cmd == "LOGON" and len(f) >= 5:
            line = "LOGON %s %s %s" % (f[1], f[2], f[3])
            if "\t".join(f[4:]) == password:
                logged_on.add(share)
                reply = logged_mount(server, share)
            else:
                reply = "ERR 13 cannot mount"
        elif share == "open" or share in logged_on:
            reply = mounted(server, share)
        else:
            reply = "ERR 13 cannot mount"
    with open(log, "a") as fh:
        fh.write("%s -> %s\n" % (line, reply.split()[0] + (" " + reply.split()[1] if reply.startswith("ERR") else "")))
    conn.sendall((reply + "\n").encode())
    conn.close()
