#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# ID2D1PathGeometry1::ComputePointAndSegmentAtLength gives the point, the
# tangent, and the segment and figure it lies on (patches/sg/8213), 64- and
# 32-bit: test/d2dseg-probe.cpp. It returned E_NOTIMPL.
#
#   WINE=/opt/wine-sg/bin/wine test/d2dseg-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_D2D_NO_POINT_SEGMENT (d2d1/geometry.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
for cc in x86_64-w64-mingw32-g++ i686-w64-mingw32-g++; do
    command -v "$cc" >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dseg.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-g++ -O1 -static -o "$T/p64.exe" "$HERE/d2dseg-probe.cpp" -ld2d1 -luuid -ldxguid &&
    TMPDIR=/var/tmp i686-w64-mingw32-g++ -O1 -static -o "$T/p32.exe" "$HERE/d2dseg-probe.cpp" -ld2d1 -luuid -ldxguid || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
for p in p64 p32; do
    echo "== $p"
    out=$(cd "$T" && timeout -s KILL 120 "$WINE" "$T/$p.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out"
    printf '%s\n' "$out" | grep -qx 'RESULT: PASS' || RC=1
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
