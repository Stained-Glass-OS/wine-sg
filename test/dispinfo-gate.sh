#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Graphics.Display.DisplayInformation (patches/sg/6204).
# The class did not exist; React Native for Windows (OneDrive) asks GetForCurrentView for the scale
# it lays out with and stopped. test/dispinfo-probe.c on a 1024x768 Xvfb: DPI, scale, orientation,
# size, one object per thread, AutoRotationPreferences.
#   WINE=/opt/wine-sg/bin/wine test/dispinfo-gate.sh
# Mutant (dlls/windows.ui/display.c): SG_MUTANT_NO_DISPLAY_INFORMATION.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: x86_64-w64-mingw32-gcc missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dispinfo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
XP=""
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    if [ -n "$XP" ]; then kill "$XP" 2>/dev/null; rm -f "/tmp/.X${DPY}-lock"; fi   # only our own Xvfb's
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/dispinfo-probe.c" -lruntimeobject -luser32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
unset WAYLAND_DISPLAY
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
DPY="${XAML_DPY:-251}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY"
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
mkdir -p "$WINEPREFIX"
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
out=$(timeout -s KILL 60 "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'statics=00000000' "DisplayInformation's statics"
chk 'view=00000000' "GetForCurrentView"
chk 'orientation=1 native=1' "a 1024x768 display stands in landscape"
chk 'scale=100 dpi=96 raw=1' "96 DPI: scale 100 %, a raw DPI from the monitor's size"
chk 'per_view=100' "one raw pixel per view pixel"
chk 'size=1024x768' "its size in raw pixels"
chk 'same=1 other=1' "one object per thread, another thread's its own"
chk 'rotation=2' "AutoRotationPreferences kept"
chk 'done=1' "the probe finished"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
