#!/usr/bin/env python3
# A WS-Discovery (2005/04) target service for test/wsdprovider-gate.sh, run inside the gate's own network
# namespace (only a loopback interface): it answers Probes and Resolves sent to the discovery group with
# unicast ProbeMatches/ResolveMatches, takes directed Probes over HTTP, and sends Hello/Bye when told to on
# a control port. Every message it receives is logged, one line each.
#   wsdprovider-device.py LOG HTTP_PORT CONTROL_PORT
import re
import socket
import struct
import sys
import threading
import uuid
from http.server import BaseHTTPRequestHandler, HTTPServer

LOG, HTTP_PORT, CONTROL_PORT = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
GROUP, PORT = "239.255.255.250", 3702
EPR = "urn:uuid:5e1d2c3b-0000-4000-8000-00000000c0de"
TYPES_NS = "http://sg.test/wsd"
XADDRS = "http://127.0.0.1:%d/dev" % HTTP_PORT
SCOPES = "ldap:///ou=floor2,o=sg http://sg.test/scope/lobby"
lock = threading.Lock()
instance = 1000
msgnum = 0


def log(line):
    with lock, open(LOG, "a") as f:
        f.write(line + "\n")


def tag(xml, name):
    m = re.search(r"<(?:[\w-]+:)?%s\b[^>]*>(.*?)</(?:[\w-]+:)?%s>" % (name, name), xml, re.S)
    return m.group(1).strip() if m else None


def attr(xml, element, name):
    m = re.search(r"<(?:[\w-]+:)?%s\b([^>]*)>" % element, xml, re.S)
    if not m:
        return None
    a = re.search(r'\b%s="([^"]*)"' % name, m.group(1))
    return a.group(1) if a else None


def envelope(action, body, relates=None, to="http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"):
    global msgnum
    msgnum += 1
    rel = "<wsa:RelatesTo>%s</wsa:RelatesTo>" % relates if relates else ""
    return ('<?xml version="1.0" encoding="utf-8"?>'
            '<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" '
            'xmlns:wsa="http://schemas.xmlsoap.org/ws/2004/08/addressing" '
            'xmlns:wsd="http://schemas.xmlsoap.org/ws/2005/04/discovery" xmlns:sg="%s">'
            '<soap:Header><wsa:To>%s</wsa:To>'
            '<wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/%s</wsa:Action>'
            '<wsa:MessageID>urn:uuid:%s</wsa:MessageID>%s'
            '<wsd:AppSequence InstanceId="%d" MessageNumber="%d"/></soap:Header>'
            '<soap:Body>%s</soap:Body></soap:Envelope>'
            % (TYPES_NS, to, action, uuid.uuid4(), rel, instance, msgnum, body)).encode()


def match_body(kind, address=EPR):
    return ('<wsd:%s><wsa:EndpointReference><wsa:Address>%s</wsa:Address></wsa:EndpointReference>'
            '<wsd:Types>sg:Printer sg:Scanner</wsd:Types><wsd:Scopes>%s</wsd:Scopes>'
            '<wsd:XAddrs>%s</wsd:XAddrs><wsd:MetadataVersion>7</wsd:MetadataVersion></wsd:%s>'
            % (kind, address, SCOPES, XADDRS, kind))


def probe_matches(xml):
    """the ProbeMatches for a Probe, or None when the types or scopes do not match"""
    types = tag(xml, "Types") or ""
    for t in types.split():
        prefix, _, local = t.rpartition(":")
        ns = re.search(r'xmlns:%s="([^"]*)"' % re.escape(prefix), xml)
        if local not in ("Printer", "Scanner") or not ns or ns.group(1) != TYPES_NS:
            return None
    scopes = tag(xml, "Scopes")
    if scopes:
        mine = SCOPES.split()
        rule = attr(xml, "Scopes", "MatchBy") or "http://schemas.xmlsoap.org/ws/2005/04/discovery/rfc2396"
        for s in scopes.split():
            if rule.endswith("/strcmp0"):
                ok = s in mine
            else:   # rfc2396: a prefix by path segments
                ok = any(m == s or m.startswith(s.rstrip("/") + "/") for m in mine)
            if not ok:
                return None
    return "<wsd:ProbeMatches>%s</wsd:ProbeMatches>" % match_body("ProbeMatch")


def udp_server():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("0.0.0.0", PORT))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                 struct.pack("4s4s", socket.inet_aton(GROUP), socket.inet_aton("127.0.0.1")))
    out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    while True:
        data, peer = s.recvfrom(65536)
        xml = data.decode("utf-8", "replace")
        action = (tag(xml, "Action") or "").rsplit("/", 1)[-1]
        mid = tag(xml, "MessageID")
        if action == "Probe":
            log("probe types=%s scopes=%s matchby=%s to=%s" % (tag(xml, "Types"), tag(xml, "Scopes"),
                attr(xml, "Scopes", "MatchBy"), tag(xml, "To")))
            body = probe_matches(xml)
            # a reply to some other search first: the provider must not take it
            out.sendto(envelope("ProbeMatches", "<wsd:ProbeMatches>%s</wsd:ProbeMatches>"
                                % match_body("ProbeMatch", "urn:uuid:not-for-you"), relates="urn:uuid:%s"
                                % uuid.uuid4()), peer)
            if body:
                out.sendto(envelope("ProbeMatches", body, relates=mid), peer)
        elif action == "Resolve":
            address = tag(xml, "Address")
            log("resolve %s" % address)
            if address == EPR:
                out.sendto(envelope("ResolveMatches", "<wsd:ResolveMatches>%s</wsd:ResolveMatches>"
                                    % match_body("ResolveMatch"), relates=mid), peer)
        elif action:
            log("other %s" % action)


def control_server():
    global instance
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", CONTROL_PORT))
    out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    out.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton("127.0.0.1"))
    while True:
        data, _ = s.recvfrom(100)
        what = data.decode().strip()
        log("control %s" % what)
        to = "urn:schemas-xmlsoap-org:ws:2005:04:discovery"
        if what == "hello":
            instance += 1
            msg = envelope("Hello", match_body("Hello"), to=to)
        elif what == "bye":
            msg = envelope("Bye", '<wsd:Bye><wsa:EndpointReference><wsa:Address>%s</wsa:Address>'
                           '</wsa:EndpointReference></wsd:Bye>' % EPR, to=to)
        else:
            continue
        out.sendto(msg, (GROUP, PORT))


class Http(BaseHTTPRequestHandler):
    def do_POST(self):
        xml = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode("utf-8", "replace")
        log("http %s %s action=%s to=%s" % (self.path, self.headers.get("Content-Type"),
            (tag(xml, "Action") or "").rsplit("/", 1)[-1], tag(xml, "To")))
        body = probe_matches(xml) if self.path == "/dev" else None
        if body is None:
            self.send_response(404)
            self.end_headers()
            return
        reply = envelope("ProbeMatches", body, relates=tag(xml, "MessageID"))
        self.send_response(200)
        self.send_header("Content-Type", "application/soap+xml")
        self.send_header("Content-Length", str(len(reply)))
        self.end_headers()
        self.wfile.write(reply)

    def log_message(self, *args):
        pass


threading.Thread(target=udp_server, daemon=True).start()
threading.Thread(target=control_server, daemon=True).start()
http = HTTPServer(("127.0.0.1", HTTP_PORT), Http)
log("ready")
http.serve_forever()
