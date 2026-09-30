#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A process renders into another process's window through Vulkan
# (patches/sg/0586). Chromium's GPU process draws into the browser's window:
# its swap chain for another process's top-level or child window failed
# ("Failed to allocate client window"), so Chrome and Edge fell back to
# software compositing. The probe's host makes a window with a child; a
# second process makes a Direct3D 11 (DXVK: Vulkan) swap chain for each and
# presents. Xvfb shows nothing of a Vulkan surface, so the gate asks whether
# the swap chain was made (render=0), not what it drew.
#
#   WINE=/opt/wine-sg/bin/wine DXVK=/opt/sg-d3d/dxvk/x64 test/xproc-vk-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
DXVK="${DXVK:-/opt/sg-d3d/dxvk/x64}"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
for t in Xvfb "$MINGWXX"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$DXVK/dxgi.dll" ] && [ -f "$DXVK/d3d11.dll" ] || { echo "SKIP: no DXVK at $DXVK"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xprocvk.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DXVK_LOG_LEVEL=none
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;dxgi,d3d11=n"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGWXX" -O2 -static -o "$T/xproc-vk-probe.exe" "$HERE/xproc-vk-probe.cpp" -ld3d11 -ldxgi -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$DXVK/dxgi.dll" "$DXVK/d3d11.dll" "$WINEPREFIX/drive_c/windows/system32/"
cp "$T/xproc-vk-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
fail=0
for m in child top; do
    out=$(timeout 60 "$WINE" 'C:\xproc-vk-probe.exe' host $m 2>/dev/null | tr -d '\r' | grep '^xproc=')
    echo "      $m: $out"
    case "$out" in
    *render=0) echo "PASS  another process's swap chain for the $m window" ;;
    *render=2*) echo "SKIP: no Direct3D 11 device (Vulkan)"; exit 77 ;;
    *) echo "FAIL  another process could not make a swap chain for the $m window"; fail=1 ;;
    esac
done
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
