#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D's 3D Transform effect, and the built-in effects' matrices
# (patches/sg/1525). Word draws its text cursor as a one-pixel image through
# a 3D Transform (scaled to the caret, moved to it), a Color Matrix, then
# DrawImage with D2D1_COMPOSITE_MODE_MASK_INVERT. Wine drew the 3D
# Transform as its input -- one pixel at the origin -- and the built-in
# effects' shader read its matrix column-major, so a transform lost its
# translation and a colour matrix was transposed: Word showed no caret
# (David's field report, 2026-10-08).
#
#   WINE=/opt/wine-sg/bin/wine test/d2d3dxform-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in xvfb-run x86_64-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2d3dxform.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O2 -o "$T/probe.exe" "$HERE/d2d3dxform-probe.c" -ld2d1 -lole32 -luuid -lwindowscodecs ||
    { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
run() { timeout -s KILL 120 xvfb-run -a -s '-screen 0 1024x768x24' sh -c "\"$WINE\" \"$T/probe.exe\" $1 > \"$T/out\" 2>/dev/null"; tr -d '\r' < "$T/out"; }
M=$(run mask); O=$(run over); C=$(run colormatrix)
echo "      mask invert: $M"
echo "      source over: $O"
echo "      $C"
case "$M" in "rt="*|"wic="*|"factory="*|"") echo "SKIP: no Direct2D device here ($M)"; exit 77 ;; esac
case "$M" in "bounds=30,40,34,60 "*) pass "a 3D Transform's output is its input moved and scaled (bounds 30,40-34,60)" ;;
    *) fail "bounds: $M" ;; esac
case "$M" in *" in=000000 out=ffffff origin=ffffff") pass "drawn with MASK_INVERT it inverts the caret's place, not the origin" ;;
    *) fail "mask invert: $M" ;; esac
case "$O" in *" in=ff0000 out=ffffff origin=ffffff") pass "drawn source-over the moved pixel is where the matrix puts it" ;;
    *) fail "source over: $O" ;; esac
[ "$C" = "colormatrix=00ff00" ] && pass "a Color Matrix applies to row vectors (red made green)" || fail "color matrix: $C"
echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
