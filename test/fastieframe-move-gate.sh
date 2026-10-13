#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ieframe + mshtml: window.moveTo/moveBy/resizeTo/resizeBy move and resize the window of the host browser.
#
#   WINE=/opt/wine-sg/bin/wine test/fastieframe-move-gate.sh   (mutant SG_MUTANT_WINDOW_NOMOVE)
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
T=$(mktemp -d /var/tmp/sg-fastiemove.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastieframe-move-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
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
[ "$(w CREATE)" = 1 ] && [ "$(w START)" = "10 20 800 600" ] && pass "a browser is created at a known position and size" || fail "setup: $(w CREATE) $(w START)"
[ "$(w MOVETO)" = "150 170 800 600" ] && pass "window.moveTo sets Left and Top of the host browser" || fail "moveTo: $(w MOVETO)"
[ "$(w MOVEBY)" = "155 160 800 600" ] && pass "window.moveBy moves it relative to where it is" || fail "moveBy: $(w MOVEBY)"
[ "$(w RESIZETO)" = "155 160 500 420" ] && pass "window.resizeTo sets Width and Height" || fail "resizeTo: $(w RESIZETO)"
[ "$(w RESIZEBY)" = "155 160 400 450" ] && pass "window.resizeBy changes the size by an amount" || fail "resizeBy: $(w RESIZEBY)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
