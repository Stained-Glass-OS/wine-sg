#!/bin/sh
# A DirectWrite glyph run analysis whose size is in its transform (patches/sg/0222):
# cairo's (GTK's) text. Needs Liberation Sans.
#   WINE=/opt/wine-sg/bin/wine test/dwscale-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
fc-list 2>/dev/null | grep -q 'Liberation Sans' || { echo "SKIP: Liberation Sans is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dwscale.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/dwscale-probe.exe" "$HERE/dwscale-probe.c" -ldwrite -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/dwscale-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qx 'same_box=5/5' && pass "em size 1 with the size in the matrix: the texture box cairo asks for" || fail "boxes: $(printf '%s\n' "$out" | grep same_box)"
printf '%s\n' "$out" | grep -qx 'same_pixels=5/5' && pass "and the same glyph pixels as drawing at the real size" || fail "pixels: $(printf '%s\n' "$out" | grep same_pixels)"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
