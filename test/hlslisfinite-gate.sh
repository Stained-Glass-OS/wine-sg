#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HLSL isfinite() and isnan() (patches/sg/9205), under Xvfb (test/hlslisfinite-probe.cpp): the HLSL compiler
# (vkd3d-shader) knew isinf only -- "Function "isfinite" is not defined" -- and Paint.NET 5.2's Auto-Level,
# which compiles its compute shader at run time, failed. A cs_5_0 shader classifies 1, -2.5, +inf, -inf,
# NaN, 0 and FLT_MAX per the HLSL reference (vectors per component, integers always finite, never NaN);
# ps_2_0 compiles them too.
#   WINE=/opt/wine-sg/bin/wine test/hlslisfinite-gate.sh
# Mutant: SG_MUTANT_HLSL_NO_ISFINITE (libs/vkd3d/libs/vkd3d-shader/hlsl.y).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGWXX" >/dev/null || { echo "SKIP: $MINGWXX missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-hlslisfinite.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/probe.exe" "$HERE/hlslisfinite-probe.cpp" -ld3d11 -ld3dcompiler_47 \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
unset WAYLAND_DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct3D 11 here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check compiled_ps2  "a ps_2_0 shader using isfinite/isnan compiles"
check compiled_cs5  "a cs_5_0 shader using isfinite/isnan compiles"
check classified    "finite, NaN and infinite values are told apart, per component"
check integers      "integers are finite and not NaN"
check done          "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
