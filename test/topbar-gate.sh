#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The taskbar is above the windows, and behind a full-screen one
# (patches/sg/0588). It was among the windows unless it hid itself: a window
# dragged over it went over it (QA VM, Notepad). Windows keeps it on top, and
# puts it behind a window in the foreground that fills the screen (a game, a
# video). A red window of another program over the bottom of the screen: the
# taskbar is still seen there. A green one filling the screen: it is not.
#
#   WINE=/opt/wine-sg/bin/wine test/topbar-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb import convert "$MINGW" cc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-topbar.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -mwindows -o "$T/topbar-probe.exe" "$HERE/topbar-probe.c" -lgdi32 -luser32 || { echo "FAIL  probe did not build"; exit 1; }
cc -O2 -o "$T/stack-watch" "$HERE/stack-watch.c" -lX11 || { echo "SKIP: stack-watch needs libX11 headers"; exit 77; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/topbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$T/run"
export SG_LOCK_CONTROL=/nonexistent XDG_RUNTIME_DIR="$T/run"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ $i -lt 60 ]; do sleep 0.5; i=$((i + 1)); done
sleep 5
cd "$WINEPREFIX/drive_c"
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
import -window root "$T/bar.png"
BAR=$(px bar 400 690)
"$WINE" 'C:\topbar-probe.exe' window >/dev/null 2>&1 & P=$!
sleep 3; import -window root "$T/window.png"; wait $P
"$WINE" 'C:\topbar-probe.exe' full >/dev/null 2>&1 & P=$!
sleep 3; import -window root "$T/full.png"; wait $P
# switching windows (the foreground taking turns, as the taskbar switches
# them): the red one must never show where the bar is, not for a frame (0604)
"$WINE" 'C:\topbar-probe.exe' flip >/dev/null 2>&1 & P=$!
sleep 0.8; FLIP=$("$T/stack-watch" 2400); wait $P
W=$(px window 400 690); A=$(px window 400 500); F=$(px full 400 690)
echo "      taskbar $BAR; window over it: bar $W, window $A; full screen: bar $F"
rc=0
[ "$A" = "255,0,0" ] || { echo "FAIL  the probe window did not show ($A)"; rc=1; }
[ -n "$BAR" ] && [ "$W" = "$BAR" ] && echo "PASS  the taskbar stays above a window over it" \
    || { echo "FAIL  a window went over the taskbar ($W, taskbar $BAR)"; rc=1; }
[ "$F" = "0,255,0" ] && echo "PASS  a full-screen window in the foreground covers the taskbar" \
    || { echo "FAIL  the taskbar is over the full-screen window ($F)"; rc=1; }
set -- $FLIP
[ "${2:-0}" -gt 5 ] && [ "${1:-1}" = 0 ] && echo "PASS  switching windows never stacks one over the taskbar, not for a moment ($2 restacks)" \
    || { echo "FAIL  a window was stacked over the taskbar while switching (${1:-?} times of ${2:-?} restacks)"; rc=1; }
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
