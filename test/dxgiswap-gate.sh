#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DXGI swap chain long tail (patches/sg/2608), under Xvfb: test/dxgiswap-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/dxgiswap-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (dxgi): SG_MUTANT_DXGI_TEARING_NOT_VALIDATED, _LATENCY_NEVER_SIGNALLED,
# _ROTATION_NOT_VALIDATED, _SOURCE_SIZE_NOT_VALIDATED, _HDR_NOT_VALIDATED,
# _STATS_NO_PRESENT_COUNT (utils.c), _FLAGS_NOT_REPORTED (swapchain.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${DXGISWAP_DPY:-242}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dxgiswap.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -mwindows -o "$T/dxgiswap-probe.exe" "$HERE/dxgiswap-probe.c" \
    -ld3d11 -ldxgi -luuid || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# Through a file, not a pipe: a crashed probe leaves wine helpers holding a pipe open.
timeout -s KILL 120 "$WINE" "$T/dxgiswap-probe.exe" >"$T/out.txt" 2>/dev/null </dev/null
out=$(tr -d '\r' <"$T/out.txt")
printf '%s\n' "$out"
printf '%s\n' "$out" | grep -qx 'RESULT: PASS' && ! printf '%s\n' "$out" | grep -q '^FAIL' && exit 0
printf '%s\n' "$out" | grep -q '^RESULT:' || echo "RESULT: FAIL (the probe gave no result)"
exit 1
