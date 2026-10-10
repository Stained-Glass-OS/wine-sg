#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.web HttpClient verbs batch (patches/sg/2037): test/webverbs-probe.c
# sends GET / DELETE / POST / PUT with an HttpClient to a local python server
# (started here on a free port) and reads the status and body back.
#
#   WINE=/opt/wine-sg/bin/wine test/webverbs-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dlls/windows.web/http_client.c): SG_MUTANT_WEB_VERB_CONTENT (the body is not
# sent), WEB_VERB_POST (PostAsync
# sends a PUT).
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
T=$(mktemp -d /var/tmp/sg-webverbs.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
SRV=
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/webverbs-probe.exe" "$HERE/webverbs-probe.c" \
    -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/server.py" <<'EOP'
import http.server, sys
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def handle_any(self):
        n = int(self.headers.get('Content-Length') or 0)
        body = self.rfile.read(n) if n else b''
        if self.path == '/missing':
            self.send_response(404); self.send_header('Content-Length', '0'); self.end_headers(); return
        out = (self.command + ' ' + self.path).encode()
        if body: out += b' ' + body
        self.send_response(200); self.send_header('Content-Type', 'text/plain'); self.send_header('Content-Length', str(len(out))); self.end_headers(); self.wfile.write(out)
    do_GET = do_POST = do_PUT = do_DELETE = handle_any
s = http.server.HTTPServer(('127.0.0.1', 0), H)
open(sys.argv[1], 'w').write(str(s.server_address[1]))
s.serve_forever()
EOP
python3 "$T/server.py" "$T/port" >/dev/null 2>&1 &
SRV=$!
n=0; while [ ! -s "$T/port" ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n+1)); done
PORT=$(cat "$T/port" 2>/dev/null)
[ -n "$PORT" ] || { echo "FAIL  no local server"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/webverbs-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" webverbs-probe.exe $PORT 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
grep -q '^RESULT:' "$T/probe.out" || echo 'FAIL  probe did not finish (crashed?)'
exit 1
