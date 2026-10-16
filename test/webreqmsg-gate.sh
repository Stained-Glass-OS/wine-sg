#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# windows.web (patch 5929): the HttpResponseMessage of SendRequestAsync and GetAsync names its request
# (RequestMessage, with its RequestUri), against a local python server (started here). Office's
# react-native-win32.dll crashed reading RequestMessage().RequestUri() of a NULL request.
#
#   WINE=/opt/wine-sg/bin/wine test/webreqmsg-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (dlls/windows.web): SG_MUTANT_RESPONSE_NO_REQUEST.
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
T=$(mktemp -d /var/tmp/sg-webreqmsg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
SRV=
cleanup() { "$WINESERVER" -k 2>/dev/null; [ -n "$SRV" ] && kill "$SRV" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/webreqmsg-probe.exe" "$HERE/webreqmsg-probe.c" \
    -lole32 -lruntimeobject -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
cat > "$T/server.py" <<'EOP'
import http.server, sys
class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass
    def do_GET(self):
        body = b'hello'
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
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
cp "$T/webreqmsg-probe.exe" "$WINEPREFIX/drive_c/"
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$WINEPREFIX/drive_c"
timeout -s KILL 180 "$WINE" webreqmsg-probe.exe $PORT 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -q '^RESULT: PASS' "$T/probe.out" && exit 0
exit 1
