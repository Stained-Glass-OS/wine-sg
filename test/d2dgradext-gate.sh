#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D gradient extend modes and gamma (patches/sg/9507), under Xvfb (test/d2dgradext-probe.cpp): a
# gradient stop collection's WRAP and MIRROR extend modes repeat a linear or radial gradient beyond its
# ends (drawn as CLAMP before: "Ignoring extend mode"), and D2D1_GAMMA_1_0 interpolates the colours in
# linear light, the result in sRGB ("Ignoring gamma"); CLAMP and gamma 2.2 stay as they were.
#   WINE=/opt/wine-sg/bin/wine test/d2dgradext-gate.sh   (WINESERVER=... beside a build tree)
# Mutants: SG_MUTANT_D2D_GRADIENT_CLAMP_ONLY, SG_MUTANT_D2D_GRADIENT_GAMMA_22 (dlls/d2d1/brush.c).
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
T=$(mktemp -d /var/tmp/sg-d2dgradext.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/d2dgradext-probe.exe" "$HERE/d2dgradext-probe.cpp" -ld2d1 -ld3d11 -ldxgi \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
DPY="${D2DGRADEXT_DPY:-329}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
unset WAYLAND_DISPLAY
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/d2dgradext-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct2D device here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check linear_clamp_inside    "linear, CLAMP, inside the gradient"
check linear_clamp_beyond    "CLAMP beyond its end: the end colour"
check linear_wrap_inside     "WRAP inside the gradient: as CLAMP"
check linear_wrap_beyond     "WRAP beyond its end: the gradient again"
check linear_mirror_beyond   "MIRROR beyond its end: the gradient reversed"
check linear_mirror_far      "MIRROR near the far end of the reversed copy: back near red"
check linear_gamma10_middle  "D2D1_GAMMA_1_0: the middle interpolated in linear light"
check linear_gamma22_middle  "D2D1_GAMMA_2_2: the middle interpolated in sRGB, as before"
check radial_clamp_beyond    "radial, CLAMP beyond the radius: the end colour"
check radial_wrap_beyond     "radial, WRAP beyond the radius"
check radial_mirror_beyond   "radial, MIRROR beyond the radius"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
