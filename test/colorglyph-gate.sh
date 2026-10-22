#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dwrite/d2d1: glyphs drawn from glyph images (color emoji fonts): TranslateColorGlyphRun
# image runs, DrawColorBitmapGlyphRun, GetColorBitmapGlyphImage, DrawText with color fonts (6007)
#   WINE=/opt/wine-sg/bin/wine test/colorglyph-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${COLORGLYPH_DPY:-531}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-colorglyph.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
# Wine's headers (mingw's lack ID2D1DeviceContext4): a build tree's, or the installed ones
W=$(dirname "$WINE")
for inc in "-I$W/include -I$W/../wine-10.0/include" "-I$W/../include/wine/windows" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -municode $inc -o "$T/colorglyph-probe.exe" "$HERE/colorglyph-probe.c" \
        -ld2d1 -ldwrite -lole32 -luuid -lwindowscodecs 2>/dev/null && { built=1; break; }
done
/usr/bin/python3 -I "$HERE/colorglyph-font.py" "$T/sgcolor.ttf" || { echo "FAIL  the test font was not made"; exit 1; }
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

rc=0
cd "$T" && timeout -s KILL 60 "$WINE" "$T/colorglyph-probe.exe" "Z:$T/sgcolor.ttf" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" || rc=1

[ $rc = 0 ] || { grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"; }
exit $rc
