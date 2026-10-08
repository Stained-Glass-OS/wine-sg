#!/usr/bin/env python3
# bitsmore-gate.sh's server: a file served with byte ranges, and a
# forbidden path. Prints its port, then serves until killed.
import http.server, socketserver, sys

DATA = bytes(range(48, 48 + 64))  # "0123456789:;<=>?@ABC..."

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass
    def do_HEAD(self):
        self.do_GET(head=True)
    def do_GET(self, head=False):
        if self.path == "/forbidden":
            self.send_response(403); self.send_header("Content-Length", "0"); self.end_headers(); return
        if self.path != "/data.bin":
            self.send_response(404); self.send_header("Content-Length", "0"); self.end_headers(); return
        rng = self.headers.get("Range")
        body, code = DATA, 200
        if rng and rng.startswith("bytes="):
            start, _, end = rng[6:].partition("-")
            start = int(start); end = int(end) if end else len(DATA) - 1
            body, code = DATA[start:end + 1], 206
        self.send_response(code)
        self.send_header("Content-Length", str(len(body)))
        if code == 206:
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, start + len(body) - 1, len(DATA)))
        self.end_headers()
        if not head:
            self.wfile.write(body)

with socketserver.TCPServer(("127.0.0.1", 0), Handler) as server:
    print(server.server_address[1], flush=True)
    server.serve_forever()
