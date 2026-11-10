#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A service's threads stay on the service's desktop (wine-sg 7724). services.exe starts a service on
# __wineservice_winstation\Default, but the process's default desktop -- what every later thread starts
# on -- was services.exe's WinSta0\Default, and only the first thread moved. A ServiceMain that created
# any window (Glary Utilities' GUBootService, GUMemfilesService) forced a desktop window with no display
# into the user's desktop; no explorer ever started there and every program after it failed with "The
# explorer process failed to start". An auto-start service, 64- and 32-bit, started at boot by a
# program on WinSta0 under Xvfb; test/svcdesktop-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/svcdesktop-gate.sh
# Mutant: SG_MUTANT_SVC_DESKTOP_STALE (server/winstation.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
DPY="${SVCDESKTOP_DPY:-247}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }
done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-svcdesktop.XXXXXX)
XP=""
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T" "${SG_GATE_HOME:-$T}"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/p64.exe" "$HERE/svcdesktop-probe.c" &&
    TMPDIR=/var/tmp i686-w64-mingw32-gcc -O1 -o "$T/p32.exe" "$HERE/svcdesktop-probe.c" ||
    { echo "FAIL  probe did not build"; exit 1; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
i=0; while ! DISPLAY=":$DPY" xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
C="$WINEPREFIX/drive_c"
RC=0
for p in p64 p32; do
    rm -f "$C/svcdesktop.log"; cp "$T/$p.exe" "$C/svcdesktop.exe"
    (cd "$C" && timeout -s KILL 120 "$WINE" sc create sgsvcdesk binPath= 'C:\svcdesktop.exe' start= auto >/dev/null 2>&1)
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
    # the first program after the restart boots the prefix, which starts the service
    out=$(cd "$C" && DISPLAY=":$DPY" timeout -s KILL 180 "$WINE" 'C:\svcdesktop.exe' check 2>/dev/null </dev/null | tr -d '\r')
    if printf '%s\n' "$out" | grep '^RESULT: ' | tail -n1 | grep -qx 'RESULT: PASS'; then
        echo "ok   $p ($(printf '%s\n' "$out" | grep -c '^ok') checks)"
    else
        echo "FAIL $p"; printf '%s\n' "$out" | grep -v '^ok'; RC=1
    fi
    (cd "$C" && timeout -s KILL 60 "$WINE" sc stop sgsvcdesk >/dev/null 2>&1;
        timeout -s KILL 60 "$WINE" sc delete sgsvcdesk >/dev/null 2>&1)
    "$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w 2>/dev/null
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
