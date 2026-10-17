#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A screen turned upright: the desktop follows it (patch 5933; David,
# 2026-10-17: after auto-rotate the bottom of the screen was unused, the
# taskbar not along the turned screen's bottom and its icons cut off).
# Wine's virtual desktop offered a portrait screen's own size only turned
# sideways, so Settings' desktop_follow (and sg-session's watcher) could not
# give the shell the turned size. Here: an X screen 1080x1920, a desktop
# started at 1920x1080 (the screen before it turned); asking for 1080x1920
# works and the desktop is that size.
#   WINE=/opt/wine-sg/bin/wine test/portrait-desktop-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_NO_PORTRAIT_MODE (win32u).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-portrait.XXXXXX)
export HOME="$T/home" WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
mkdir -p "$HOME"
XP=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/probe.exe" "$HERE/portrait-desktop-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1080x1920x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1920x1080 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1920x1080 >/dev/null 2>&1 &
sleep 6
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
P() { (cd "$WINEPREFIX/drive_c" && timeout 120 "$WINE" probe.exe "$@" 2>/dev/null | tr -d '\r'); }
land=$(P 1920 1080); port=$(P 1080 1920)
case "$land" in *"now=1920x1080"*) echo "PASS  the desktop, as the session started it: $land" ;; *) echo "FAIL  the desktop at the start: $land"; exit 1 ;; esac
case "$port" in
    *"change=0 now=1080x1920"*) echo "PASS  the desktop takes the turned screen's size ($port)"; rc=0 ;;
    *) echo "FAIL  the desktop could not take the turned screen's size ($port)"; rc=1 ;;
esac
exit $rc
