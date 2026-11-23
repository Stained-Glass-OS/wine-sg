#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The HTML application object (<HTA:APPLICATION>) in mshta (wine-sg 9608): test/htaapp.hta under Xvfb --
# commandLine and the attributes as properties, and the window styles its attributes ask for (read by
# test/htaapp-probe.c). The element was an unknown one: commandLine was undefined, the window ignored it.
#   WINE=/opt/wine-sg/bin/wine test/htaapp-gate.sh
# Mutant: SG_MUTANT_NO_HTA_OBJECT (mshtml/htmldlg.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"; export WINESERVER
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${HTAAPP_DPY:-339}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-htaapp.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${HTAAPP_DEBUG:--all} WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
PORT=$((20000 + DPY))
python3 - "$HERE" "$PORT" "$T/log.txt" <<'PY' >/dev/null 2>&1 &
import sys, http.server, urllib.parse
root, port, log = sys.argv[1], int(sys.argv[2]), sys.argv[3]
class H(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith("/log?"):
            with open(log, "a") as f: f.write(urllib.parse.unquote(self.path[5:]) + "\n")
            body = b"ok"
        else:
            body = open(root + "/htaapp.hta", "rb").read()
        self.send_response(200); self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass
http.server.HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
SP=$!
trap 'kill $SP 2>/dev/null; cleanup' EXIT INT TERM
sleep 1
TMPDIR=/var/tmp "$MINGW" -O1 -o "$WINEPREFIX/drive_c/probe.exe" "$HERE/htaapp-probe.c" -luser32 2>/dev/null || { fail "probe did not build"; exit 1; }
timeout -s KILL 120 "$WINE" mshta "http://127.0.0.1:$PORT/htaapp.hta" first "two words" >"$T/ie.log" 2>&1 &
i=0; while ! grep -qx loaded "$T/log.txt" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
grep -qx loaded "$T/log.txt" 2>/dev/null || { fail "the application did not load"; cat "$T/log.txt" 2>/dev/null; grep -v '^$' "$T/ie.log" | tail -15; echo "RESULT: FAIL"; exit 1; }
sleep 1
(cd "$WINEPREFIX/drive_c" && timeout -s KILL 60 "$WINE" probe.exe) 2>/dev/null | tr -d '\r' >> "$T/log.txt"

out=$(cat "$T/log.txt")
printf '%s\n' "$out" | sed 's/^/      /'
check() { if printf '%s\n' "$out" | grep -qxF "$1"; then pass "$2"; else fail "$2 (wanted '$1')"; fi; }
check "cmd=http://127.0.0.1:$PORT/htaapp.hta first \"two words\"" "commandLine is mshta's command line"
check "name=htatest version=1.2 border=dialog" "the attributes read as properties"
check "maxbox=0 minbox=1 thick=0 caption=1 sysmenu=1 dlgframe=1" "the window has the styles the attributes ask for"
printf '%s\n' "$out" | grep -q THREW && fail "a call threw" || pass "no call threw"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
