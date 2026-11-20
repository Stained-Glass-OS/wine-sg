#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Dynamic properties (IHTMLStyle2::setExpression, getExpression, removeExpression; IHTMLDocument3::recalc)
# in Internet Explorer (wine-sg 8228): test/cssexpr.html under Xvfb. They were E_NOTIMPL.
#   WINE=/opt/wine-sg/bin/wine test/cssexpr-gate.sh
# Mutant: SG_MUTANT_NO_CSS_EXPRESSIONS (mshtml/htmlstyle.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${CSSEXPR_DPY:-284}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-cssexpr.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${CSSEXPR_DEBUG:--all} WINEDLLOVERRIDES="mscoree=;winemenubuilder.exe=d" WINESERVER
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
            body = open(root + "/cssexpr.html", "rb").read()
        self.send_response(200); self.send_header("Content-Type", "text/html")
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass
http.server.HTTPServer(("127.0.0.1", port), H).serve_forever()
PY
SP=$!
trap 'kill $SP 2>/dev/null; cleanup' EXIT INT TERM
sleep 1
timeout -s KILL 120 "$WINE" iexplore "http://127.0.0.1:$PORT/cssexpr.html" >"$T/ie.log" 2>&1 &
i=0; while ! grep -qx loaded "$T/log.txt" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
grep -qx loaded "$T/log.txt" 2>/dev/null || { fail "the page did not load"; grep -v '^$' "$T/ie.log" | tail -15; exit 1; }
i=0; while ! grep -qx done "$T/log.txt" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done

out=$(cat "$T/log.txt")
printf '%s\n' "$out" | sed 's/^/      /'
check() { if printf '%s\n' "$out" | grep -qxF "$1"; then pass "$2"; else fail "$2 (wanted '$1')"; fi; }
check "set width=10px"            "setExpression sets the property to the expression's value"
check "get=w + 'px'"               "getExpression gives the expression"
check "recalc width=30px"          "document.recalc evaluates it again"
check "bg=red"                     "an expression in a named language (JScript)"
check "removed=true again=false"   "removeExpression: true once, then false"
check "after remove width=30px get=undefined" "a removed expression is no longer evaluated or given"
printf '%s\n' "$out" | grep -q THREW && fail "a call threw" || pass "no call threw"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
