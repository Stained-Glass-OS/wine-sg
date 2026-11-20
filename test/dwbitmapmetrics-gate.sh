#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dwrite: design metrics of a glyph in a bitmap-only font (patches/sg/8407): no garbage from an
# unloadable glyph (SpeedCrunch crashed drawing Noto Color Emoji's "box" as a 805592x24980944 glyph).
# test/dwbitmapmetrics-probe.c, test/dwbitmapmetrics-font.py.
#
#   WINE=/opt/wine-sg/bin/wine test/dwbitmapmetrics-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant (dwrite): SG_MUTANT_DWRITE_BITMAP_METRICS_UNSET (freetype.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -x /usr/bin/python3 ] && /usr/bin/python3 -c 'import fontTools' 2>/dev/null || { echo "SKIP: python3-fonttools missing"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dwbitmapmetrics.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -municode -o "$T/dwbitmapmetrics-probe.exe" "$HERE/dwbitmapmetrics-probe.c" \
    -ldwrite || { echo "FAIL  the probe did not build"; exit 1; }
/usr/bin/python3 -I "$HERE/dwbitmapmetrics-font.py" "$T/sgbitmap.ttf" || { echo "FAIL  the test font was not made"; exit 1; }

mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sgbitmap.ttf" "$WINEPREFIX/drive_c/sgbitmap.ttf"

timeout -s KILL 60 "$WINE" "$T/dwbitmapmetrics-probe.exe" 'C:\sgbitmap.ttf' >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
grep '^metrics:' "$T/out"

fail=0
check() {
    if grep -qx "$1=1" "$T/out"; then echo "PASS  $2"; else echo "FAIL  $2"; fail=1; fi
}
check done "the probe ran to the end"
check face "the bitmap-only font opens as a font face"
check advance "the glyph's advance is its hmtx advance (2550), not stack garbage"
check bearings "its side bearings and vertical metrics are within the em, its vertical origin the ascent"
check cached "the cached second answer is the same"
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
