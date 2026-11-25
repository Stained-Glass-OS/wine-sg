#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# HLSL: a uint's cast to int and its cast to float stay apart (patches/sg/9207), under Xvfb
# (test/hlslcastvec-probe.cpp). vkd3d-shader's expression vectorizer merged two casts of one source into one
# whatever their types: int2(gtid.x, 0) indexed the texture with utof(gtid.x)'s bits and every thread but the
# first wrote nowhere (Paint.NET 5.2's compute effects: Auto-Level transparent). All four texels must be written.
#   WINE=/opt/wine-sg/bin/wine test/hlslcastvec-gate.sh
# Mutant: SG_MUTANT_HLSL_VECTORIZE_ANY_TYPE (libs/vkd3d/libs/vkd3d-shader/hlsl_codegen.c).
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
T=$(mktemp -d /var/tmp/sg-hlslcastvec.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/probe.exe" "$HERE/hlslcastvec-probe.cpp" -ld3d11 -ld3dcompiler_47 \
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
check group_thread     "int2(SV_GroupThreadID.x, 0) and its float are both right in every thread"
check dispatch_thread  "int and float casts of SV_DispatchThreadID.x are both right"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
