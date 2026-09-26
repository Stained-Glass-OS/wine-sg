#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D geometries answer what drawing programs ask of them
# (patches/sg/0427) -- Paint.NET's selection, shapes and effects:
#   - a path's area, length, point at a length, widened bounds and relation
#     to another geometry (stubs, E_NOTIMPL, in Wine 10.0)
#   - an elliptical arc in a path is an arc, not a straight line
#   - a command list's bounds are what it draws (were unbounded), so an
#     effect taking it as input gets the right rectangle
#
#   WINE=/opt/wine-sg/bin/wine test/d2dgeom-gate.sh     (needs Xvfb)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
CXX="${MINGW_CXX:-x86_64-w64-mingw32-g++}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$CXX" >/dev/null || { echo "SKIP: $CXX not installed"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: no Xvfb"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dgeom.XXXXXX)
# A display of our own -- never the desktop's.
DN=$(( 400 + $$ % 200 ))
Xvfb ":$DN" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 &
XPID=$!
export DISPLAY=":$DN" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
cleanup() { "$WINESERVER" -k 2>/dev/null; kill "$XPID" 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
sleep 2
"$CXX" -O2 -static -o "$T/d2dgeom-probe.exe" "$HERE/d2dgeom-probe.cpp" -ld2d1 -ld3d11 -ldxgi -ldxguid -luuid \
    || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dgeom-probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has 'AREA 00000000 5000.0' && pass "a triangle's area" || fail "area"
has 'LENGTH 00000000 341.4' && pass "its perimeter" || fail "length"
has 'POINTAT 00000000 50.0,0.0 tangent 1.0,0.0' && pass "the point and tangent at a length" || fail "point at length"
has 'INSIDE 00000000 1' && has 'OUTSIDE 00000000 0' && has 'ONSTROKE 00000000 1' && pass "fill and stroke contain points" || fail "contains"
has 'WIDENED 00000000 0.0,-5.0,100.0,5.0' && pass "a stroked line's widened bounds" || fail "widened bounds"
has 'CONTAINS 00000000 3' && has 'CONTAINED 00000000 2' && has 'DISJOINT 00000000 1' && has 'OVERLAP 00000000 4' \
    && pass "contains, is contained, disjoint, overlap" || fail "relations"
has 'ARCBOUNDS 00000000 0,0,100,50' && pass "an arc bulges to its radius" || fail "arc bounds"
has 'ARCAREA 00000000 3927' && pass "a half circle's area (pi r^2 / 2)" || fail "arc area"
has 'LISTBOUNDS 00000000 10,20,90,60' && pass "a command list's bounds are what it draws" || fail "command list bounds"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
