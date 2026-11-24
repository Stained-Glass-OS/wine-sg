#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msxml3 XMLHTTP and ServerXMLHTTP open() take any HTTP method (patches/sg/9613):
# test/xmlhttpverbs.vbs sends PATCH, OPTIONS, MKCOL, PROPPATCH and DELETE with a
# JSON body to a local echo server (127.0.0.1 only) and reads the echo back;
# Wine refused every method but GET/PUT/POST/DELETE/HEAD/PROPFIND with E_FAIL.
# cscript needs no display: the gate runs headless.
#
#   WINE=/opt/wine-sg/bin/wine test/xmlhttpverbs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (dlls/msxml3/httprequest.c): SG_MUTANT_XMLHTTP_KNOWN_VERBS_ONLY.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xmlhttpverbs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
SRV=
cleanup() { [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
python3 - "$T/port" <<'PY' >"$T/server.log" 2>&1 &
import sys, http.server
class H(http.server.BaseHTTPRequestHandler):
    def handle_one_request(self):
        self.raw_requestline = self.rfile.readline(65537)
        if not self.raw_requestline:
            self.close_connection = True
            return
        if not self.parse_request():
            return
        n = int(self.headers.get('Content-Length') or 0)
        body = self.rfile.read(n).decode('utf-8', 'replace') if n else ''
        out = ('%s %s %s' % (self.command, self.path, body)).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Content-Length', str(len(out)))
        self.end_headers()
        if self.command != 'HEAD':
            self.wfile.write(out)
        self.wfile.flush()
    def log_message(self, *a):
        sys.stderr.write((a[0] % a[1:]) + '\n')
s = http.server.HTTPServer(('127.0.0.1', 0), H)
open(sys.argv[1], 'w').write(str(s.server_address[1]))
s.serve_forever()
PY
SRV=$!
i=0; while [ ! -s "$T/port" ] && [ $i -lt 50 ]; do i=$((i+1)); python3 -c 'import time; time.sleep(0.1)'; done
[ -s "$T/port" ] || { echo "FAIL  the echo server did not start"; exit 1; }
PORT=$(cat "$T/port")
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$HERE/xmlhttpverbs.vbs" "$WINEPREFIX/drive_c/"
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 180 env DISPLAY= no_proxy='*' NO_PROXY='*' http_proxy= https_proxy= "$WINE" cscript.exe //nologo xmlhttpverbs.vbs "http://127.0.0.1:$PORT" 2>&1 </dev/null | tr -d '\r' > "$T/probe.out")
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || { echo 'FAIL  the script did not finish (an error stops cscript)'; cat "$T/server.log"; }
exit 1
