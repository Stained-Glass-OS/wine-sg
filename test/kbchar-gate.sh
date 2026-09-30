#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# IE's KeyboardEvent.char in mshtml (patches/sg/0534), under Xvfb: iexplore opens
# test/kbchar.html, whose keydown handler records each event's key and char;
# xdotool types into its text field.  Reading char threw E_NOTIMPL on every
# key -- Microsoft's sign-in page (Office's embedded browser) reads it in its
# keydown handler.
#
#   WINE=/opt/wine-sg/bin/wine test/kbchar-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${KBCHAR_DPY:-179}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo xdotool; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-kbchar.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${KBCHAR_DEBUG:--all} WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
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
            body = open(root + "/kbchar.html", "rb").read()
        self.send_response(200); self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass
http.server.HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
SP=$!
trap 'kill $SP 2>/dev/null; cleanup' EXIT INT TERM
sleep 1
timeout -s KILL 120 "$WINE" iexplore "http://127.0.0.1:$PORT/kbchar.html" >"$T/ie.log" 2>&1 &
i=0; while ! grep -qx loaded "$T/log.txt" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
grep -qx loaded "$T/log.txt" 2>/dev/null || { fail "the page did not load"; grep -v '^$' "$T/ie.log" | tail -15; exit 1; }
w=$(xdotool search --name "Internet Explorer" 2>/dev/null | head -1)
if [ -n "$w" ]; then
    eval "$(xdotool getwindowgeometry --shell "$w")"
    xdotool mousemove $((X + WIDTH / 2)) $((Y + HEIGHT * 3 / 4)) click 1
fi
sleep 1
xdotool type --delay 150 'a5'; xdotool key Shift_L; xdotool key Return; sleep 2

out=$(cat "$T/log.txt")
[ -n "${KBCHAR_KEEPLOG:-}" ] && cp "$T/ie.log" "$KBCHAR_KEEPLOG"
printf '%s\n' "$out" | sed 's/^/      /'
check() { if printf '%s\n' "$out" | grep -qxF "$1"; then pass "$2"; else fail "$2 (wanted '$1')"; fi; }
check "key=a char=[a]"     "a letter key: char is the letter"
check "key=5 char=[5]"     "a digit key: char is the digit"
check "key=Shift char=[]"  "Shift types no character: char is empty"
check "key=Enter char=[]"  "Enter types no character: char is empty"
printf '%s\n' "$out" | grep -q THREW && fail "reading char threw" || pass "reading char never throws"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
