#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct3D 10/11 stream output appending and DrawAuto (patches/sg/9306), wined3d's OpenGL renderer (no DXVK),
# under Xvfb: test/d3d11soauto-probe.cpp. A stream output buffer has a filled size (BufferFilledSize):
# SOSetTargets with an offset of -1 goes on where the buffer's last stream output stopped -- also after the
# buffer was unbound -- and DrawAuto draws as many vertices as that size holds at the vertex buffer's stride.
# wined3d wrote every "append" from the start of the buffer ("Appending to stream output buffers not
# implemented", 4 points then 3 more left 3 of the first overwritten) and DrawAuto was a stub that drew
# nothing (GPU particle systems: nothing on screen). An explicit offset starts there and counts from there.
#   WINE=/opt/wine-sg/bin/wine test/d3d11soauto-gate.sh
# Mutant: SG_MUTANT_SO_NO_FILLED_SIZE (dlls/wined3d/context_gl.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGWXX" >/dev/null || { echo "SKIP: $MINGWXX missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d3d11soauto.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGWXX" -O2 -static -o "$T/probe.exe" "$HERE/d3d11soauto-probe.cpp" -ld3d11 -ldxgi \
    -ld3dcompiler_47 -luuid -ldxguid || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
unset WAYLAND_DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
[ -n "${D3D11SOAUTO_RENDERER:-}" ] && export WINE_D3D_CONFIG="renderer=$D3D11SOAUTO_RENDERER"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct3D 11 stream output here: $(printf '%s\n' "$out" | grep '^no_')"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check first_draw          "4 points are written from offset 0"
check append              "3 more with offset -1 follow them"
check append_after_rebind "unbound and bound again with -1, 2 more follow those"
check explicit_offset     "an explicit offset of 32 bytes, then -1: entries 2, 3 and 4"
check drawauto            "DrawAuto from that buffer draws its 9 vertices (copied by stream output)"
check drawauto_count      "a SO statistics query around DrawAuto sees 9 primitives"
check drawauto_offset     "DrawAuto of the 80-byte filled buffer draws 5 vertices"
check done                "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
