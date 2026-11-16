#!/usr/bin/env python3
# wsauth-server.py PORT LOG: a SOAP 1.2 endpoint for test/wsauth-gate.sh. /svc wants HTTP Basic with the
# test account sgtest:sgpass (a made-up pair, nothing real); /deny refuses everyone; /ntlm only offers NTLM;
# anything else is 404. Every request's path and Authorization header are logged.
import base64, sys
from http.server import BaseHTTPRequestHandler, HTTPServer

PORT, LOG = int(sys.argv[1]), sys.argv[2]
GOOD = 'Basic ' + base64.b64encode(b'sgtest:sgpass').decode()
REPLY = ('<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" '
         'xmlns:a="http://www.w3.org/2005/08/addressing"><s:Header><a:Action s:mustUnderstand="1">act_req'
         '</a:Action></s:Header><s:Body><req xmlns="urn:sg"><val>4321</val></req></s:Body></s:Envelope>')

class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0) or 0)
        self.rfile.read(n)
        auth = self.headers.get('Authorization', '')
        with open(LOG, 'a') as f:
            f.write('%s %s\n' % (self.path, auth or '-'))
        if self.path == '/svc' and auth == GOOD:
            body = REPLY.encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/soap+xml; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path in ('/svc', '/deny', '/ntlm'):
            self.send_response(401)
            self.send_header('WWW-Authenticate', 'NTLM' if self.path == '/ntlm' else 'Basic realm="sg"')
            self.send_header('Content-Length', '0')
            self.end_headers()
        else:
            self.send_response(404)
            self.send_header('Content-Length', '0')
            self.end_headers()

HTTPServer(('127.0.0.1', PORT), H).serve_forever()
