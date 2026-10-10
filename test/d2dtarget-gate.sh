#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# d2d1 results recorded from Windows by Wine's tests (patches/sg/2619): Map access rules, DXGI surface
# pixel formats, a closed command list, GetSurface errors, the WIC target's lock, GetDC without a target:
# test/d2dtarget-probe.c.
#
#   WINE=/opt/wine-sg/bin/wine test/d2dtarget-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutants (d2d1): SG_MUTANT_D2D_MAP_ACCESS_UNCHECKED, SG_MUTANT_D2D_MAP_DISCARD_UNCHECKED (bitmap.c),
# SG_MUTANT_D2D_SURFACE_FORMAT_UNCHECKED (bitmap.c), SG_MUTANT_D2D_SURFACE_ERROR_SAME (bitmap.c),
# SG_MUTANT_D2D_CLOSE_KEEPS_TARGET (command_list.c), SG_MUTANT_D2D_GETDC_NO_TARGET (device.c),
# SG_MUTANT_D2D_WIC_NO_DRAW_LOCK, SG_MUTANT_D2D_WIC_LOCK_ERROR_DROPPED (wic_render_target.c).
# The probe needs the d2d1_3 interfaces: Wine's own headers are used when the system ones lack them.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
DPY="${D2DCTL_DPY:-246}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
XP=""
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dtarget.XXXXXX)
cleanup() {
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

built=
for inc in "" "-I$(dirname "$WINE")/include" "-I$HERE/../../obj/include" "-I/opt/wine-sg/include/wine/windows"; do
    TMPDIR=/var/tmp "$MINGW" -O2 -mwindows $inc -o "$T/d2dtarget-probe.exe" "$HERE/d2dtarget-probe.c" \
        -ld3d11 -ld2d1 -luuid -ldxguid -lole32 2>/dev/null && { built=1; break; }
done
[ -n "$built" ] || { echo "FAIL  the probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset WAYLAND_DISPLAY; export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# a crashed probe leaves wine helpers holding a pipe: go through a file
timeout -s KILL 60 "$WINE" "$T/d2dtarget-probe.exe" >"$T/out.raw" 2>/dev/null </dev/null
tr -d '\r' <"$T/out.raw" >"$T/out"
cat "$T/out"
grep -qx 'RESULT: PASS' "$T/out" && ! grep -q '^FAIL' "$T/out" && exit 0
grep -q '^RESULT:' "$T/out" || echo "RESULT: FAIL (the probe gave no result)"
exit 1
