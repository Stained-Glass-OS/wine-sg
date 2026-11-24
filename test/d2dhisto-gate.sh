#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D Histogram effect (patches/sg/9354), under Xvfb (test/d2dfxres-probe.cpp): drawing it fills
# HistogramOutput -- NumBins floats, each the fraction of the input's straight pixels whose selected channel
# falls in the bin (Win2D's ComputeHistogram tests: red/lime/blue/white in 2 bins is 0.5 0.5) -- and draws
# its input. The output stayed empty before; Paint.NET's Levels and Auto-Level read histograms.
#   WINE=/opt/wine-sg/bin/wine test/d2dhisto-gate.sh   (WINESERVER=... beside a build tree)
# Mutant: SG_MUTANT_D2D_HISTOGRAM_EMPTY (dlls/d2d1/effect_render.c).
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
T=$(mktemp -d /var/tmp/sg-d2dhisto.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/d2dfxres-probe.exe" "$HERE/d2dfxres-probe.cpp" -ld2d1 -ld3d11 -ldxgi -ld3dcompiler_47 \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
DPY="${D2DHISTO_DPY:-381}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
unset WAYLAND_DISPLAY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dfxres-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct2D device here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check histogram_red          "red channel, 2 bins: 0.5 0.5"
check histogram_green        "green channel, 2 bins"
check histogram_blue_4bins   "blue channel, 4 bins: the ends"
check histogram_straight     "straight colour: red 64 at alpha 128 is 0.5"
check histogram_alpha        "alpha channel"
check histogram_passthrough  "the effect draws its input"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
