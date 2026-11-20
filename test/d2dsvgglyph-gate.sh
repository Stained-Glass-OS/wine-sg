#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ID2D1DeviceContext4::DrawSvgGlyphRun and GetSvgGlyphImage (patches/sg/8227):
# a glyph's OpenType SVG document is drawn, a glyph without one as an outline
# in the default fill brush; test/d2dsvgglyph-probe.cpp with the font
# test/d2dsvgglyph-font.py makes, under Xvfb. They were stubs.
#
#   WINE=/opt/wine-sg/bin/wine test/d2dsvgglyph-gate.sh
# Mutant: SG_MUTANT_D2D_NO_SVG_GLYPHS (d2d1/device.c, svg.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
CXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
DPY="${D2DSVGGLYPH_DPY:-281}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }
for need in Xvfb xdpyinfo "$CXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dsvgglyph.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$CXX" -O2 -static -o "$T/probe.exe" "$HERE/d2dsvgglyph-probe.cpp" -ld2d1 -ld3d11 -ldxgi -ldwrite \
    -lole32 -luuid 2>"$T/cc.log" || { cat "$T/cc.log"; echo "FAIL  the probe did not build"; exit 1; }
/usr/bin/python3 -I "$HERE/d2dsvgglyph-font.py" "$T/svgglyph.ttf" || { echo "FAIL  the test font was not made"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

timeout -s KILL 60 "$WINE" "$T/probe.exe" "Z:$T/svgglyph.ttf" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
