#!/usr/bin/python3 -I
"""A stand-in RFC 3161 time-stamping authority for tsretrieve-gate.sh: each POST's body
(application/timestamp-query) is answered by `openssl ts -reply` with the gate's TSA; POST /bad
answers status rejection. Listens on 127.0.0.1, writes the port to argv[2]."""
import http.server, os, subprocess, sys, tempfile

DIR = sys.argv[1]

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", "0")))
        if self.path.endswith("bad"):
            reply = bytes([0x30, 0x05, 0x30, 0x03, 0x02, 0x01, 0x02])   # status: rejection
        else:
            with tempfile.TemporaryDirectory(dir=DIR) as t:
                q = os.path.join(t, "q.tsq"); r = os.path.join(t, "r.tsr")
                open(q, "wb").write(body)
                subprocess.run(["openssl", "ts", "-reply", "-config", os.path.join(DIR, "tsa.cnf"), "-queryfile", q,
                                "-out", r], check=True, capture_output=True)
                reply = open(r, "rb").read()
        self.send_response(200)
        self.send_header("Content-Type", "application/timestamp-reply")
        self.send_header("Content-Length", str(len(reply)))
        self.end_headers()
        self.wfile.write(reply)

s = http.server.HTTPServer(("127.0.0.1", 0), H)
open(sys.argv[2], "w").write(str(s.server_address[1]))
s.serve_forever()
