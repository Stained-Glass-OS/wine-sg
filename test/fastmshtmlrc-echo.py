#!/usr/bin/env python3
# POST body echo for the XMLHttpRequest checks: answers "len=<bytes> sum=<byte sum>" and writes its port to argv[1].
import sys, http.server

class H(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        n = int(self.headers.get('Content-Length', '0') or 0)
        body = self.rfile.read(n)
        out = ('len=%d sum=%d' % (len(body), sum(body))).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(out)))
        self.end_headers()
        self.wfile.write(out)
    def do_GET(self):
        # a page to run the script from: XMLHttpRequest only reaches the origin it was loaded from
        out = b'<html><head><meta http-equiv="x-ua-compatible" content="IE=edge"><title>xhr</title></head><body>x</body></html>'
        self.send_response(200)
        self.send_header('Content-Type', 'text/html')
        self.send_header('Content-Length', str(len(out)))
        self.end_headers()
        self.wfile.write(out)
    def log_message(self, *a):
        pass

s = http.server.HTTPServer(('127.0.0.1', 0), H)
open(sys.argv[1], 'w').write(str(s.server_address[1]))
s.serve_forever()
