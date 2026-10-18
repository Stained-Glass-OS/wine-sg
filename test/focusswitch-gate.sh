#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Clicking between two overlapping windows: the one clicked comes up and gets
# the keyboard, every time (patch 5936; David, 2026-10-18: Notepad partly
# under the Registry Editor, one switch in five it stayed under it or its
# keys went to the Registry Editor). winex11 gave the X keyboard focus -- and
# the top -- to any window whose program moved the focus, the one behind too
# (the Registry Editor between its panes as it was deactivated).
# Here, on a shell desktop: Notepad and the Registry Editor overlapping,
# clicked in turn eight times each; after each click the clicked one is in
# front and on top, and what is typed after the last Notepad click is in it.
#   WINE=/opt/wine-sg/bin/wine test/focusswitch-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Mutant: SG_MUTANT_BACKGROUND_FOCUS (winex11).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW=x86_64-w64-mingw32-gcc
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
for t in Xvfb xdotool $MINGW; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
T=$(mktemp -d /var/tmp/sg-focusswitch.XXXXXX)
export HOME="$T/home" WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d;mscoree,mshtml=" WINESERVER
mkdir -p "$HOME"
XP=
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/focusswitch-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
"$WINESERVER" -w
"$WINE" explorer /desktop=shell,1280x800 >/dev/null 2>&1 &
sleep 5
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
"$WINE" notepad >/dev/null 2>&1 &
sleep 3
"$WINE" regedit >/dev/null 2>&1 &
sleep 4
"$WINE" probe.exe place; sleep 1
P() { "$WINE" probe.exe 2>/dev/null | tr -d '\r'; }
misses=0
for i in 1 2 3 4 5 6 7 8; do
    xdotool mousemove 120 300 click 1; sleep 0.8
    a=$(P); case "$a" in *Notepad*notepad_above=1*) ;; *) misses=$((misses + 1)); echo "      Notepad click $i: $a";; esac
    xdotool mousemove 850 500 click 1; sleep 0.8
    b=$(P); case "$b" in *Registry*notepad_above=0*) ;; *) misses=$((misses + 1)); echo "      Registry Editor click $i: $b";; esac
done
xdotool mousemove 120 300 click 1; sleep 0.8; xdotool type --delay 30 hello; sleep 0.8
c=$(P)
rc=0
[ "$misses" = 0 ] && echo "PASS  16 clicks between Notepad and the Registry Editor: the one clicked is in front and on top each time" \
    || { echo "FAIL  $misses of 16 clicks left the clicked window behind or not in front"; rc=1; }
case "$c" in *"chars=5"*) echo "PASS  what is typed after clicking Notepad goes to Notepad ($c)" ;;
    *) echo "FAIL  typing after clicking Notepad: $c (want chars=5)"; rc=1 ;; esac
exit $rc
