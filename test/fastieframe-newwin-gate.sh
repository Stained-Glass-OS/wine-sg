#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ieframe + mshtml: window.open in a browser fires NewWindow3/NewWindow2 at the host (cancel, hand over a browser).
#
#   WINE=/opt/wine-sg/bin/wine test/fastieframe-gate.sh   (mutants SG_MUTANT_IEFRAME_NONEWWIN, SG_MUTANT_IEFRAME_NOFALLBACK)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastiewin.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastieframe-newwin-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 150 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(w CREATE)" = 1 ] && pass "a browser is created and navigates to a local page" || fail "setup: $(w CREATE)"
[ "$(w CANCEL3)" = "00000000 null n3=1 n2=0" ] && pass "window.open fires NewWindow3; the host cancels and open returns null" || fail "cancel3: $(w CANCEL3)"
[ "$(w ARGS)" = "0 url-ok" ] && [ "$(w CTX)" = ctx-ok ] && pass "NewWindow3 carries the flags, the context URL and the target URL" || fail "args: $(w ARGS) $(w CTX)"
[ "$(w CANCEL2)" = "00000000 null n3=1 n2=1" ] && pass "a sink without NewWindow3 gets NewWindow2 instead and can cancel" || fail "cancel2: $(w CANCEL2)"
[ "$(w HANDOVER)" = "00000000 win n3=1 n2=0" ] && [ "$(w HANDOVERLOC)" = 1 ] && pass "the browser the host hands over receives the page and open returns its window" || fail "handover: $(w HANDOVER) $(w HANDOVERLOC)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
