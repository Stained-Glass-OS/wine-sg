#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D custom effects with a compute transform (patches/sg/9204), under Xvfb (test/d2dcompute-probe.cpp):
# an ID2D1ComputeTransform was never given its ID2D1ComputeInfo and its shader never ran (Paint.NET 5.2's
# Auto-Level: "Unexpected null", nothing drawn). As Direct2D does, the transform gets SetComputeInfo, is
# asked CalculateThreadgroups for its output rectangle, and its shader runs with Direct2D's constants at b0
# (resultRect, outputOffset, sceneToInput0X/Y), its own at b1, its input at t0 and the output at u0 -- also
# for an output rectangle that does not start at 0 (through a crop). 9206: an analysis transform
# (ID2D1AnalysisTransform) is handed its output on the CPU after drawing (ProcessAnalysisResults), in the
# buffer precision it asked for -- Paint.NET's own Histogram effect reads its counts so (Auto-Level, Levels).
# 9208: a source transform (ID2D1SourceTransform) is told its render info and Draw()s into a bitmap Direct2D
# hands it, which is its output (Paint.NET's "Bitmap Source 2" feeding Auto-Level: the result was transparent).
#   WINE=/opt/wine-sg/bin/wine test/d2dcompute-gate.sh
# Mutants: SG_MUTANT_D2D_NO_COMPUTE (dlls/d2d1/effect.c), SG_MUTANT_D2D_NO_ANALYSIS, SG_MUTANT_D2D_NO_SOURCE
# (dlls/d2d1/effect_render.c).
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
T=$(mktemp -d /var/tmp/sg-d2dcompute.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/probe.exe" "$HERE/d2dcompute-probe.cpp" -ld2d1 -ld3d11 -ldxgi -ld3dcompiler_47 \
    -luuid -ldxguid || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
unset WAYLAND_DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct3D 11 compute here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check shader_loaded   "the compute shader is loaded"
check info_set        "the transform is given its compute info (SetComputeInfo)"
check groups_asked    "it is asked how many thread groups its output takes"
check dispatched      "its shader runs: red and green swapped over the image"
check offset_rect     "an output rectangle at 4,4 is placed by resultRect/outputOffset/sceneToInput"
check offset_outside  "outside the rectangle is transparent"
check constants_b1    "the effect's own constants reach b1"
check analysis_called "an analysis transform is handed its output after drawing (ProcessAnalysisResults)"
check analysis_data   "as the 32-bit floats it asked for, row by row"
check source_info     "a source transform is told its render info"
check source_drawn    "it fills the bitmap Direct2D hands it (Draw), which is drawn"
check source_chained  "and feeds the next effect"
check source_mapped   "the bitmap can be mapped for writing (WRITE | DISCARD), as Paint.NET does"
check done            "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
