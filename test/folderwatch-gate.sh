#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The view follows its folder (patches/sg/0601). A file another program made
# or deleted in the folder File Explorer shows never appeared or went until
# the folder was opened again: the shell's change notices reach only the
# process that sends them. File Explorer on C:\one (one file): a file written
# by another Wine program, one made by a Linux program, then the first file
# deleted -- the view (read through its status bar) follows: 2, 3, 2 items.
#
#   WINE=/opt/wine-sg/bin/wine test/folderwatch-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-folderwatch.XXXXXX); XP=
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1024x700x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/one"; : > "$C/one/first.txt"
cd "$C"
WINEDEBUG=trace+explorer "$WINE" explorer.exe 'C:\one' > "$T/log" 2>&1 </dev/null &
sleep 8
"$WINE" cmd /c 'echo x > C:\one\second.txt' >/dev/null 2>&1; sleep 3
: > "$C/one/third.txt"; sleep 3
rm "$C/one/first.txt"; sleep 3
seq=$(grep -o 'status: L"[^"]*"' "$T/log" | uniq | sed 's/status: L"\([0-9]*\) item.*/\1/' | tr '\n' ' ')
echo "      items shown: $seq"
case "$seq" in
*"1 2 3 2 "*) echo "PASS  files made and deleted by other programs appear and go"; echo "RESULT: PASS"; exit 0 ;;
esac
echo "FAIL  the view did not follow its folder"; echo "RESULT: FAIL"; exit 1
