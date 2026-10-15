#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.web content headers (patch 3290): test/fastwebhdr-probe.c exercises HttpContentHeaderCollection,
# the typed header values and HttpStringContent.Headers, and the content headers on the wire against a
# local python server (started here).
#
#   WINE=/opt/wine-sg/bin/wine test/fastwebhdr-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/windows.web): SG_MUTANT_WEBHDR_* (see work/muts-webhdr.sh).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: needs python3"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-fastwebhdr.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
SRV=
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/fastwebhdr-probe.exe" "$HERE/fastwebhdr-probe.c" \
    -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/server.py" <<'EOP'
import http.server, sys
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_POST(self):
        n = int(self.headers.get('Content-Length') or 0)
        data = self.rfile.read(n)
        body = ('ct=%s|lang=%s|x=%s|len=%d' % (self.headers.get('Content-Type'), self.headers.get('Content-Language'),
                self.headers.get('X-Content-Test'), len(data))).encode()
        self.send_response(200)
        self.send_header('Content-Type', 'text/x-test; charset=utf-8')
        self.send_header('Content-Language', 'de')
        self.send_header('Last-Modified', 'Tue, 15 Nov 1994 12:45:26 GMT')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
s = http.server.ThreadingHTTPServer(('127.0.0.1', 0), H)
open(sys.argv[1], 'w').write(str(s.server_address[1]))
s.serve_forever()
EOP
python3 "$T/server.py" "$T/port" >/dev/null 2>&1 &
SRV=$!
n=0; while [ ! -s "$T/port" ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n+1)); done
PORT=$(cat "$T/port" 2>/dev/null)
[ -n "$PORT" ] || { echo "FAIL  server did not start"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/fastwebhdr-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" fastwebhdr-probe.exe $PORT 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
