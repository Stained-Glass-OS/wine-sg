#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Line patterns in wined3d's OpenGL core contexts (patches/sg/9307), under Xvfb: test/d3dlinepat-probe.c.
# Core contexts have no glLineStipple, so wined3d ignores D3DRS_LINEPATTERN -- yet it reported
# D3DPRASTERCAPS_PAT, and every device printed "Setting line patterns is not supported" for the default
# state. As a Direct3D driver without line patterns does, the device does not report PAT (Direct3D 7/8 caps)
# and a pattern set anyway is ignored: the line is drawn whole (a legacy context reports PAT and draws it).
#   WINE=/opt/wine-sg/bin/wine test/d3dlinepat-gate.sh
# Mutant: SG_MUTANT_PAT_ALWAYS (dlls/wined3d/adapter_gl.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="$(sg_wineserver)"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-d3dlinepat.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/d3dlinepat-probe.c" || { fail "probe did not build"; exit 1; }
sg_gate_display || exit $?
unset WAYLAND_DISPLAY
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -q '^no_' && { echo "SKIP: no Direct3D 8 here"; exit 77; }
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
check consistent       "D3DPRASTERCAPS_PAT is reported only where the pattern is drawn (a legacy context)"
printf '%s\n' "$out" | grep -q '^d3d8_pat=1' && echo "NOTE: a legacy OpenGL context here (line patterns drawn)"
check done             "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
