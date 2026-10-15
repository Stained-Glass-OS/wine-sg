#!/usr/bin/env python3
# fastb-bitsup-gate.sh's server: the BITS upload protocol (Create-Session, Fragment,
# Close-Session, Cancel-Session) on /up, /reply (Close-Session answers with a body),
# /short (accepts half of each fragment), /plain (not a BITS server); GET /last gives back
# the last completed upload. Prints its port, then serves until killed.
import http.server, socketserver, uuid

PROTO = "{7DF0354D-249B-430F-820D-3D2A9BEF4931}"
sessions = {}
last = b""

class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *a): pass
    def reply(self, code, headers, body=b""):
        self.send_response(code)
        for k, v in headers.items(): self.send_header(k, v)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        if self.path == "/last":
            self.reply(200, {}, last)
        else:
            self.reply(404, {})
    def do_POST(self):
        global last
        n = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(n) if n else b""
        path = self.path
        if path == "/plain":
            self.reply(200, {}, b"hello")
            return
        if path not in ("/up", "/reply", "/short"):
            self.reply(404, {})
            return
        ptype = self.headers.get("BITS-Packet-Type", "")
        sid = self.headers.get("BITS-Session-Id")
        if ptype == "Create-Session":
            if PROTO.lower() not in self.headers.get("BITS-Supported-Protocols", "").lower():
                self.reply(200, {"BITS-Packet-Type": "Nack", "BITS-Error-Code": "80200014"})
                return
            sid = "{%s}" % str(uuid.uuid4()).upper()
            sessions[sid] = bytearray()
            self.reply(200, {"BITS-Packet-Type": "Ack", "BITS-Protocol": PROTO, "BITS-Session-Id": sid})
        elif ptype == "Fragment" and sid in sessions:
            rng = self.headers.get("Content-Range", "")
            start = int(rng.split()[1].split("-")[0])
            data = sessions[sid]
            if start <= len(data) and path == "/short":
                body = body[:len(body) // 2 + 1]
            if start <= len(data):
                data[start:start + len(body)] = body
            self.reply(200, {"BITS-Packet-Type": "Ack", "BITS-Session-Id": sid,
                             "BITS-Received-Content-Range": str(len(data))})
        elif ptype == "Close-Session" and sid in sessions:
            last = bytes(sessions.pop(sid))
            out = b"REPLY:%d" % len(last) if path == "/reply" else b""
            self.reply(200, {"BITS-Packet-Type": "Ack", "BITS-Session-Id": sid}, out)
        elif ptype == "Cancel-Session":
            sessions.pop(sid, None)
            self.reply(200, {"BITS-Packet-Type": "Ack", "BITS-Session-Id": sid or ""})
        else:
            self.reply(200, {"BITS-Packet-Type": "Nack", "BITS-Error-Code": "80200002"})

class S(socketserver.ThreadingTCPServer):
    daemon_threads = True
    allow_reuse_address = True

with S(("127.0.0.1", 0), H) as server:
    print(server.server_address[1], flush=True)
    server.serve_forever()
