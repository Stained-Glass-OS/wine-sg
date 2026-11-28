#!/usr/bin/env python3
"""The NCSI probe page for test/ncsi-gate.sh, on ADDRESS port 80: what it answers is in MODEFILE --
"ok" the page Windows' probe expects, "portal" a captive portal's redirect to its sign-in page,
"down" nothing (the connection is closed unanswered). Logs each request's path and Host."""
import socket
import sys

addr, modefile, log = sys.argv[1:4]
srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind((addr, 80))
srv.listen(8)
while True:
    conn, _ = srv.accept()
    conn.settimeout(5)
    data = b""
    try:
        while b"\r\n\r\n" not in data:
            chunk = conn.recv(4096)
            if not chunk:
                break
            data += chunk
    except OSError:
        pass
    mode = open(modefile).read().strip()
    first = data.split(b"\r\n", 1)[0].decode("latin-1", "replace")
    host = [l for l in data.decode("latin-1", "replace").split("\r\n") if l.lower().startswith("host:")]
    with open(log, "a") as fh:
        fh.write("%s %s %s\n" % (mode, first, host[0] if host else "-"))
    if mode == "ok":
        body = b"Microsoft Connect Test"
        conn.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\nConnection: close\r\n\r\n" % len(body) + body)
    elif mode == "portal":
        conn.sendall(b"HTTP/1.1 302 Found\r\nLocation: http://portal.example/login\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")
    conn.close()
