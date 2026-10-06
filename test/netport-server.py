#!/usr/bin/env python3
"""Printers on the network, for the netport gate (patches/sg/1029): a raw
TCP printer (port 9100 style), an LPR printer (RFC 1179) and an IPP printer
(RFC 8010 Print-Job over HTTP), all on 127.0.0.1 at ports of the system's
choosing.  What each receives goes to OUTDIR.  Our own code.

    netport-server.py OUTDIR
"""
import http.server
import os
import socket
import socketserver
import struct
import sys
import threading

out = sys.argv[1]


def save(name, data, mode='wb'):
    with open(os.path.join(out, name), mode) as f:
        f.write(data)


class Raw(socketserver.BaseRequestHandler):
    def handle(self):
        data = b''
        while True:
            chunk = self.request.recv(65536)
            if not chunk:
                break
            data += chunk
        save('raw.bin', data)


class Lpr(socketserver.StreamRequestHandler):
    def line(self):
        return self.rfile.readline().rstrip(b'\n')

    def handle(self):
        cmd = self.line()
        if not cmd.startswith(b'\x02'):
            return
        save('lpr-queue.txt', cmd[1:])
        self.wfile.write(b'\0')
        while True:
            sub = self.line()
            if not sub:
                break
            kind = sub[0]
            size, name = sub[1:].split(b' ', 1)
            self.wfile.write(b'\0')
            data = self.rfile.read(int(size))
            self.rfile.read(1)
            self.wfile.write(b'\0')
            save('lpr-control.txt' if kind == 2 else 'lpr-data.bin', data)


class Ipp(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_POST(self):
        length = int(self.headers.get('Content-Length', 0))
        if length:
            body = self.rfile.read(length)
        else:
            body = b''
            while True:
                size = int(self.rfile.readline().strip(), 16)
                if not size:
                    self.rfile.readline()
                    break
                body += self.rfile.read(size)
                self.rfile.readline()
        version, op, reqid = struct.unpack('>HHI', body[:8])
        attrs, i = [], 8
        while body[i] != 3:
            tag = body[i]
            i += 1
            if tag < 0x10:
                continue
            n = struct.unpack('>H', body[i:i + 2])[0]
            name = body[i + 2:i + 2 + n].decode()
            i += 2 + n
            v = struct.unpack('>H', body[i:i + 2])[0]
            value = body[i + 2:i + 2 + v].decode()
            i += 2 + v
            attrs.append('%s=%s' % (name, value))
        lines = ['path=%s' % self.path, 'type=%s' % self.headers.get('Content-Type'),
                 'version=%x op=%x' % (version, op)] + attrs
        save('ipp-request.txt', ('\n'.join(lines) + '\n').encode())
        save('ipp-data.bin', body[i + 1:])
        reply = struct.pack('>HHI', 0x0101, 0, reqid) + b'\x01'
        for tag, name, value in ((0x47, b'attributes-charset', b'utf-8'),
                                 (0x48, b'attributes-natural-language', b'en')):
            reply += bytes([tag]) + struct.pack('>H', len(name)) + name + struct.pack('>H', len(value)) + value
        reply += b'\x03'
        self.send_response(200)
        self.send_header('Content-Type', 'application/ipp')
        self.send_header('Content-Length', str(len(reply)))
        self.end_headers()
        self.wfile.write(reply)


class Server(socketserver.ThreadingMixIn, socketserver.TCPServer):
    allow_reuse_address = True
    daemon_threads = True


servers = [('raw', Server(('127.0.0.1', 0), Raw)), ('lpr', Server(('127.0.0.1', 0), Lpr)),
           ('ipp', http.server.ThreadingHTTPServer(('127.0.0.1', 0), Ipp))]
for name, s in servers:
    threading.Thread(target=s.serve_forever, daemon=True).start()
save('ports.tmp', ''.join('%s %d\n' % (n, s.server_address[1]) for n, s in servers).encode())
os.rename(os.path.join(out, 'ports.tmp'), os.path.join(out, 'ports'))
threading.Event().wait()
