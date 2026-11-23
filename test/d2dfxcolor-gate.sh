#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D built-in effects that were drawn as their input ("Built-in effect ... is drawn as its input";
# patches/sg/9509), under Xvfb (test/d2dfxcolor-probe.cpp): Linear, Gamma, Table and Discrete Transfer
# (the Filter Effects spec's feComponentTransfer, on straight colour, per channel, with Disable), Hue
# Rotation (feColorMatrix hueRotate, degrees), Luminance To Alpha and Exposure (times 2^value), each on a
# solid bitmap, compared with the formula's colour.
#   WINE=/opt/wine-sg/bin/wine test/d2dfxcolor-gate.sh   (WINESERVER=... beside a build tree)
# Mutant: SG_MUTANT_D2D_FX_STUBS (dlls/d2d1/effect_render.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for need in Xvfb xdpyinfo "$MINGWXX"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d2dfxcolor.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/d2dfxcolor-probe.exe" "$HERE/d2dfxcolor-probe.cpp" -ld2d1 -ld3d11 -ldxgi \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
DPY="${D2DFXCOLOR_DPY:-326}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
unset WAYLAND_DISPLAY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dfxcolor-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct2D device here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check passthrough_check   "the target is cleared (no effect drawn)"
check linear_transfer     "Linear Transfer: slope and intercept per channel, a disabled channel left alone"
check gamma_transfer      "Gamma Transfer: amplitude, exponent, offset, alpha too"
check table_transfer      "Table Transfer: piecewise linear tables"
check discrete_transfer   "Discrete Transfer: step tables"
check hue_rotation        "Hue Rotation by 90 degrees"
check luminance_to_alpha  "Luminance To Alpha: black, the luminance as alpha"
check exposure            "Exposure -1: half the colour"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
