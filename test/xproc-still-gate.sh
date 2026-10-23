#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Another process's frames in a browser's window are shown, not copied (6046). Chromium's GPU process
# draws into the browser's top-level (patches/sg/0586). Its frames were rendered offscreen and copied
# into the browser's X window, so the browser's own repaint (an expose, a focus change, a compositor
# starting, a lock) wiped them white until the next present -- which a still page never makes: Opera
# and Dropbox turned white, frames included (qa4/qa5, 2026-10-22). Now such a swap chain's client
# window is shown in place (X keeps the browser's painting out of it, as Windows' compositor keeps a
# swap chain over GDI), unless a DPI scale needs the copy.
# The probe's host makes a top-level with a child window (as a browser's has); its render process
# presents into the top-level; the host repaints. The render process's surface must not have gone
# offscreen. (Whether the frame is on screen is not measurable here: Xvfb and a headless Xwayland show
# no Vulkan frame of this probe; Opera's still page under sg-deskcomp on weston/Xwayland was the
# visual check -- white before, its page after.)
#   WINE=/opt/wine-sg/bin/wine DXVK=/opt/sg-d3d/dxvk/x64 test/xproc-still-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
DXVK="${DXVK:-/opt/sg-d3d/dxvk/x64}"
MINGWXX="${MINGWXX:-x86_64-w64-mingw32-g++}"
for t in Xvfb "$MINGWXX"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
[ -f "$DXVK/dxgi.dll" ] && [ -f "$DXVK/d3d11.dll" ] || { echo "SKIP: no DXVK at $DXVK"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xprocstill.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER DXVK_LOG_LEVEL=none
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;dxgi,d3d11=n"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGWXX" -O2 -static -mwindows -o "$T/xproc-still-probe.exe" "$HERE/xproc-still-probe.cpp" -ld3d11 -ldxgi -lgdi32 ||
    { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$DXVK/dxgi.dll" "$DXVK/d3d11.dll" "$WINEPREFIX/drive_c/windows/system32/"
cp "$T/xproc-still-probe.exe" "$WINEPREFIX/drive_c/"
C="$WINEPREFIX/drive_c"
wait_phase() { i=0; while [ "$(cat "$C/phase.txt" 2>/dev/null)" != "$1" ] && [ $i -lt 300 ]; do sleep 0.1; i=$((i + 1)); done; }
# in a desktop window, as Stained Glass OS runs every program
(cd "$C" && WINEDEBUG=+vulkan timeout 60 "$WINE" explorer /desktop=gate,1024x700 'C:\xproc-still-probe.exe' host \
    >"$T/run.log" 2>&1 &)
wait_phase repainted
sleep 1
"$WINESERVER" -k 2>/dev/null; sleep 1
fail=0
if grep -q "X11DRV_vulkan_surface_create Created surface" "$T/run.log"; then
    echo "PASS  another process made a swap chain for the host's top-level"
else
    echo "SKIP: no Vulkan surface (no Direct3D 11 device here)"; exit 77
fi
if grep -q ": offscreen 1" "$T/run.log"; then
    echo "FAIL  its frames are rendered offscreen and copied, so the host's repaint wipes them"; fail=1
else
    echo "PASS  its client window is shown in place, over what the host paints"
fi
[ $fail = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
