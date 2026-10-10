#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectComposition surfaces (patches/sg/2679): test/dcompsurface-probe.cpp checks the surface API
# (arguments, the BeginDraw/SuspendDraw/ResumeDraw/EndDraw states, the texture and offset BeginDraw
# gives, Scroll, a virtual surface's Resize and Trim) and commits a visual whose content is a
# surface; this script looks at the window's pixels with xwd: green, then red after a second
# BeginDraw/EndDraw/Commit. Under Xvfb, with Wine's own d3d11 (wined3d over the server's GL).
#
#   WINE=/opt/wine-sg/bin/wine test/dcompsurface-gate.sh
# Mutant: SG_MUTANT_SURFACE_NEVER_PRESENT (dcomp/surface.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
CXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
DPY="${DPY:-$((700 + $$ % 200))}"
RC=0 XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo xwininfo xwd python3 "$CXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dcompsurface.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$CXX" -O2 -o "$T/dcompsurface-probe.exe" "$HERE/dcompsurface-probe.cpp" -ld3d11 -ldxgi -ldcomp -luser32 -luuid \
    -static-libgcc -static-libstdc++ || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/prefix"
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

winpath=$(printf '%s' "$T" | sed 's|/|\\|g')
timeout -s KILL 180 "$WINE" "$T/dcompsurface-probe.exe" "Z:$winpath" >"$T/out" 2>/dev/null </dev/null & PP=$!
i=0; while ! grep -q '^PHASE1\|^NO' "$T/out" && [ $i -lt 240 ]; do sleep 0.5; i=$((i + 1)); done
grep -q '^NO' "$T/out" && { echo "SKIP: no d3d11 composition here ($(tr -d '\r' <"$T/out"))"; exit 77; }
pixel() {   # PIXEL X Y of the X window, as hex bytes in B G R order
    xwd -silent -id "0x$1" | python3 -c '
import struct, sys
d = sys.stdin.buffer.read()
h = struct.unpack(">25I", d[:100])
off, bpl = h[0] + h[19] * 12, h[12]
x, y = '"$2, $3"'
p = d[off + y * bpl + x * 4: off + y * bpl + x * 4 + 4]
print((p[::-1] if h[7] == 0 else p).hex())'
}
win=$(tr -d '\r' <"$T/out" | sed -n 's/^PHASE1 //p')
if [ -n "$win" ] && [ "$win" != 0 ]; then
    px=$(pixel "$win" 100 50)
    echo "      phase 1 pixel $px"
    case "$px" in ??00ff00|00??00ff00) pass "the surface's green is in the window";; *) fail "phase 1 pixel $px";; esac
else fail "no X window: $(tr -d '\r' <"$T/out" | tail -3)"; fi
: >"$T/go2"
i=0; while ! grep -q '^PHASE2' "$T/out" && [ $i -lt 120 ]; do sleep 0.5; i=$((i + 1)); done
if [ -n "$win" ] && [ "$win" != 0 ]; then
    px=$(pixel "$win" 100 50)
    echo "      phase 2 pixel $px"
    case "$px" in ??ff0000|00??ff0000) pass "after another BeginDraw/EndDraw and Commit the window shows red";; *) fail "phase 2 pixel $px";; esac
fi
: >"$T/done"; wait "$PP" 2>/dev/null
tr -d '\r' <"$T/out" | grep -v '^PHASE\|^RESULT' | sed 's/^/      /' | grep -v '^      PASS' || true
tr -d '\r' <"$T/out" | grep -q '^FAIL' && RC=1
tr -d '\r' <"$T/out" | grep -q '^RESULT: PASS' || RC=1
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
