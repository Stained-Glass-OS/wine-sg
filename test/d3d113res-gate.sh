#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 11.3 resources, views, queries and rasterizer states through
# ID3D11Device3 (patches/sg/2612), under Xvfb: test/d3d113res-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/d3d113res-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (d3d11): SG_MUTANT_D3D113_PLANE_ACCEPTED (view.c, a plane slice other
# than 0 is dropped instead of refused), SG_MUTANT_D3D113_ARRAY_MIXED (view.c, the
# first slice and size of a 2D array view are exchanged on the way back),
# SG_MUTANT_D3D113_LAYOUT_IGNORED (device.c, CreateTexture2D1 accepts any
# layout), SG_MUTANT_D3D113_QUERY_CTX (async.c, GetDesc1 reports a wrong context
# type), SG_MUTANT_D3D113_CONSERVATIVE_ACCEPTED (device.c), SG_MUTANT_D3D113_TEX_LAYOUT
# (texture.c, GetDesc1 reports a layout that is not undefined).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D3D113RES_DPY:-245}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d3d113res.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/d3d113res-probe.exe" "$HERE/d3d113res-probe.c" \
    -ld3d11 -luuid || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

timeout -s KILL 120 "$WINE" "$T/d3d113res-probe.exe" >"$T/out.txt" 2>/dev/null </dev/null
out=$(tr -d '\r' <"$T/out.txt")
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
