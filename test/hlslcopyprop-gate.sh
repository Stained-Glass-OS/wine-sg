#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The HLSL compiler's copy propagation (patches/sg/6530, backported from upstream vkd3d a87b1efb
# and 09b9f8ff): a constant out-of-range index in dead code no longer writes past the compiler's
# bookkeeping (Firefox's WebRender gradient shader crashed its GPU process), and a store into an
# inner array invalidates the right components (test/hlslcopyprop-probe.c; a compute shader on
# the gate's own X server).
#   WINE=/opt/wine-sg/bin/wine test/hlslcopyprop-gate.sh
# Mutants: SG_MUTANT_HLSL_OOB_INVALIDATE, SG_MUTANT_HLSL_NO_OUTER_OFFSET (libs/vkd3d hlsl_codegen.c;
# rebuild wined3d and the d3dcompilers).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hlslcopyprop.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/hlslcopyprop-probe.c" -ld3d11 -ld3dcompiler_47 -luuid || { fail "probe did not build"; exit 1; }
if [ -n "${SG_HLSL_DISPLAY:-}" ]; then
    Xvfb ":$SG_HLSL_DISPLAY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -e "/tmp/.X11-unix/X$SG_HLSL_DISPLAY" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    echo "$SG_HLSL_DISPLAY" > "$T/display"
else
    Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
    i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
fi
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 300 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check oob_dead_code   "an out-of-range constant index in dead code compiles, 300 times in other heap layouts"
check nested_compiled "the inner-array store shader compiles"
check device          "a Direct3D 11 device and the compute shader"
check nested_store    "a store into an inner array is seen by the read after it (1,2,3,5)"
check done            "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
