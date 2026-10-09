#!/usr/bin/env python3
# A stand-in for sg-compositor's control socket SESSION (test/wtsnotify-gate.sh,
# patches/sg/1704): each SESSION connection is told the state, and once the
# go file appears the session is locked, unlocked, taken by Remote Desktop and
# given back (locked), unlocked and viewed remotely, one event every 0.6 s.
#
#   wtsnotify-standin.py SOCKET GOFILE
import os, socket, sys, threading, time

path, go = sys.argv[1], sys.argv[2]
state = {"locked": 0, "remote": 0, "shadow": 0}
watchers = []
lock = threading.Lock()

def serve(conn):
    try:
        if conn.recv(64).strip() != b"SESSION":
            conn.sendall(b"ERR unknown command\n")
            conn.close()
            return
        with lock:
            conn.sendall(("OK session locked=%(locked)d remote=%(remote)d shadow=%(shadow)d\n" % state).encode())
            watchers.append(conn)
    except OSError:
        conn.close()

def accept():
    while True:
        conn, _ = srv.accept()
        threading.Thread(target=serve, args=(conn,), daemon=True).start()

def event(name, **changes):
    with lock:
        state.update(changes)
        for w in list(watchers):
            try:
                w.sendall(name.encode() + b"\n")
            except OSError:
                watchers.remove(w)
    time.sleep(0.6)

srv = socket.socket(socket.AF_UNIX)
srv.bind(path)
srv.listen(16)
threading.Thread(target=accept, daemon=True).start()
while not os.path.exists(go):
    time.sleep(0.1)
time.sleep(0.3)
event("lock", locked=1)
event("unlock", locked=0)
event("remote-connect", remote=1)
event("remote-disconnect", remote=0)
event("lock", locked=1)
event("unlock", locked=0)
event("shadow-start", shadow=1)
event("shadow-end", shadow=0)
time.sleep(30)
