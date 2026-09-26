#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D effect graphs as Paint.NET builds them (patches/sg/0341), under
# Xvfb (test/d2dgraph-probe.c):
#
#  - one effect input feeding two transform nodes (the second connection
#    replaced the first: the output node had no input and drew nothing);
#  - a graph rebuilt in PrepareForRender: its draw transform is given its
#    draw info when added (it never was: no pixel shader, nothing drawn);
#  - DrawImage with an image rectangle puts its top left at the target
#    offset for effects too (effects were drawn where the image is).
#
#   WINE=/opt/wine-sg/bin/wine test/d2dgraph-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D2DGRAPH_DPY:-178}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dgraph.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/d2dgraph-probe.exe" "$HERE/d2dgraph-probe.c" \
    -ld2d1 -ld3d11 -ld3dcompiler_47 -lole32 -luuid -ldxguid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/d2dgraph-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
printf '%s\n' "$out" | grep -q '^no_' && { fail "probe could not start: $out"; exit 1; }
check shared_input               "one effect input feeds two nodes; the output node draws it"
check rebuilt_graph              "a graph made in PrepareForRender draws (its transform set its pixel shader)"
check image_rect_effect          "DrawImage of an effect: the image rectangle's top left at the target offset"
check image_rect_bitmap          "and of a bitmap, as before"
check done                       "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
