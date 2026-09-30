#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The status bar follows the view (patches/sg/0599). After a paste added a
# file, File Explorer's status bar still said "0 items" beside it (QA VM): it
# counted the items only when the folder was opened or the selection changed.
# A file pasted into the open folder (Ctrl+V): the status bar says 2 items.
#
#   WINE=/opt/wine-sg/bin/wine test/statuscount-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb xdotool "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-statuscount.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/statuscount-probe.exe" "$HERE/statuscount-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/statuscount-probe.exe" "$WINEPREFIX/drive_c/"
cd "$WINEPREFIX/drive_c"
"$WINE" 'C:\statuscount-probe.exe' setup >/dev/null 2>&1
WINEDEBUG=trace+explorer "$WINE" explorer.exe 'C:\one' > "$T/log" 2>&1 </dev/null &
sleep 8
"$WINE" 'C:\statuscount-probe.exe' clip >/dev/null 2>&1
w=$(xdotool search --name '^one$' 2>/dev/null | head -1)
[ -n "$w" ] && xdotool windowactivate --sync "$w" 2>/dev/null; sleep 1
xdotool mousemove 500 400 click 1; sleep 0.5; xdotool key ctrl+v
sleep 4
grep -o 'status: L"[^"]*"' "$T/log" | uniq | sed 's/^/      /'
if grep -q 'status: L"2 items"' "$T/log"; then
    echo "PASS  the status bar counts a file copied into the open folder"; echo "RESULT: PASS"; exit 0
fi
echo "FAIL  the status bar did not follow the view"; echo "RESULT: FAIL"; exit 1
