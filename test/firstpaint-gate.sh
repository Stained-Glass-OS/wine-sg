#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A window is not shown black before its program paints it (patches/sg/0587).
# X exposes a window as soon as it is shown, and Wine sent its surface --
# black where the program had not painted yet: every window opened with a
# black flash (David 2026-09-29; 40 ms of a black Notepad on the QA VM). Now
# an unpainted surface is the window colour, and an exposed window is sent
# only once drawn. The probe paints a corner of its client area and leaves
# the rest for 1.5 s, then paints it red: before, the rest is the window
# colour, not black (Xvfb's root, under a window not sent at all, is black
# too); after, red.
#
#   WINE=/opt/wine-sg/bin/wine test/firstpaint-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb import convert "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-firstpaint.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -mwindows -o "$T/firstpaint-probe.exe" "$HERE/firstpaint-probe.c" -lgdi32 -luser32 2>/dev/null \
    || "$MINGW" -O2 -mwindows -o "$T/firstpaint-probe.exe" "$HERE/firstpaint-probe.c" -lgdi32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/firstpaint-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
px() { convert "$T/$1.png" -format "%[fx:int(255*p{$2,$3}.r)],%[fx:int(255*p{$2,$3}.g)],%[fx:int(255*p{$2,$3}.b)]" info: 2>/dev/null; }
"$WINE" 'C:\firstpaint-probe.exe' >/dev/null 2>&1 & P=$!
sleep 1; import -window root "$T/early.png"
sleep 1.5; import -window root "$T/late.png"
wait $P
# the client area's centre: the window is at 100,100 and 400x300
E=$(px early 300 260); L=$(px late 300 260)
echo "      unpainted: $E; painted: $L"
rc=0
case "$E" in
"") echo "FAIL  no capture"; rc=1 ;;
0,0,0) echo "FAIL  the unpainted window is shown black"; rc=1 ;;
255,255,255) echo "PASS  the unpainted window is the window colour, not black" ;;
*) echo "FAIL  the unpainted window is $E, not the window colour"; rc=1 ;;
esac
[ "$L" = "255,0,0" ] && echo "PASS  once painted, the window shows it" || { echo "FAIL  after painting: $L"; rc=1; }
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
